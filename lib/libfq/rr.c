#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "rr.h"

struct rr*
rr_init(void)
{
	struct rr *rr = calloc(1, sizeof(struct rr));

	rr->qs = uint32_void_tbl_init(0);
	VTAILQ_INIT(&rr->active_q);
	rr->next = NULL;
	rr->n_active = 0;
	rr->rr_counter = 0;
	rr->max_ec = 0;
	rr->prev_max_ec = 0;
	rr->gave_quantum = false;

	return rr;
}

void
rr_destroy(struct rr *rr)
{
	struct rr_qn *qn;
	struct rr_vn *vn;
	uint32_void_tbl_const_iterator *end;
	uint32_void_tbl_const_iterator *it;
	uint32_void_tbl_locked_table *locked = uint32_void_tbl_lock_table(rr->qs);

	it = uint32_void_tbl_locked_table_cbegin(locked);
	end = uint32_void_tbl_locked_table_cend(locked);

	/* Iterate through table and free all queue nodes */
	for (; !uint32_void_tbl_const_iterator_equal(it, end); uint32_void_tbl_const_iterator_increment(it)) {
		/* Free any value nodes still dangline off the queue node */
		qn = *uint32_void_tbl_const_iterator_mapped(it);
		while (!VTAILQ_EMPTY(&qn->q)) {
			vn = VTAILQ_FIRST(&qn->q);
			VTAILQ_REMOVE(&qn->q, vn, list);
			free(vn);
		}

		free(qn);
	}

	/* Free the table */
	uint32_void_tbl_const_iterator_free(it);
	uint32_void_tbl_const_iterator_free(end);
	uint32_void_tbl_locked_table_free(locked);
	uint32_void_tbl_free(rr->qs);
	free(rr);
}

struct rr_qn*
rr_add_queue(struct rr *rr, uint32_t key)
{
	struct rr_qn *qn = calloc(1, sizeof(struct rr_qn));
	VTAILQ_INIT(&qn->q);
	qn->key = key;
	qn->ec = 0;
	qn->cost = 1;
	qn->active = false;

	if (!uint32_void_tbl_insert(rr->qs, &key, (void**)&qn)) {
		assert(false); // TODO: Handle error
	}

	return qn;
}

int
rr_enqueue(struct rr *rr, uint32_t key, void *v)
{
	struct rr_qn *qn;
	struct rr_vn *vn;

	/* Find or create queue */
	if (!uint32_void_tbl_find(rr->qs, &key, (void**)&qn))
		qn = rr_add_queue(rr, key);

	/* If newly active, add to active queue */
	if (!qn->active) {
		qn->prev_seq_num = qn->seq_num;
		rr->next_seq_num += 1;
		qn->seq_num = rr->next_seq_num;
		qn->active = true;
		VTAILQ_INSERT_TAIL(&rr->active_q, qn, list);
		rr->n_active += 1;
	}

	/* Insert value */
	vn = calloc(1, sizeof(struct rr_vn));
	vn->v = v;
	VTAILQ_INSERT_TAIL(&qn->q, vn, list);

	return 0;
}

static inline int64_t
int64_max(int64_t x, int64_t y)
{
	if (x > y)
		return x;
	else
		return y;
}

int64_t
rr_cost(uint32_t key)
{
	switch (key) {
	default:
		return 1;
	}
}

void*
rr_dequeue(struct rr *rr, uint64_t *seq_num)
{
	uint32_t rr_counter = rr->rr_counter;
	uint32_t n_active = rr->n_active;
	int64_t max_ec = rr->max_ec;
	int64_t prev_max_ec = rr->prev_max_ec;
	bool gave_quantum = rr->gave_quantum;
	struct rr_qn *qn = rr->next;
	struct rr_qn *tmp = NULL;
	struct rr_vn *vn = NULL;
	void *v = NULL;
	int64_t ec = 0;

	if (n_active == 0)
		return NULL;

	if (rr_counter == 0) {
		rr_counter = n_active;
		prev_max_ec = max_ec;
		max_ec = 0;
	}

	/* qn may be NULL if this is the first call to dequeue
	 * after rr was completely empty or if we hit the
	 * head of the active queue.
	 */
	qn = rr->next;
	if (!qn)
		qn = VTAILQ_FIRST(&rr->active_q);

	ec = qn->ec;
	if (!gave_quantum) {
		ec -= prev_max_ec;
		gave_quantum = true;
	}

	if (rr->last_completed_seq_num >= qn->prev_seq_num) {
		vn = VTAILQ_FIRST(&qn->q);
		VTAILQ_REMOVE(&qn->q, vn, list);

		ec += rr_cost(qn->key);

		v = vn->v;
		free(vn);
		*seq_num = qn->seq_num;

		// We need to advance to next queue
		if (ec > 0 || VTAILQ_EMPTY(&qn->q)) {
			/* Get next queue node before we potentially remove this one */
			tmp = VTAILQ_NEXT(qn, list);

			if (!VTAILQ_EMPTY(&qn->q)) { // exhausted quantum
				rr->next_seq_num += 1;
				qn->prev_seq_num = qn->seq_num;
				qn->seq_num = rr->next_seq_num;
			} else { // queue is empty
				ec = 0; // Reset excess counter

				/* Remove node from active queue */
				qn->active = false;
				VTAILQ_REMOVE(&rr->active_q, qn, list);
				n_active -= 1;
			}

			qn->ec = ec;
			max_ec = int64_max(ec, max_ec);
			rr_counter -= 1;

			gave_quantum = false;
			qn = tmp; // Advance to next queue
		}
	} else {
		v = NULL; // Slow down the scheduler
	}
		

	/* Store state for next call */
	rr->gave_quantum = gave_quantum;
	rr->next = qn;
	rr->n_active = n_active;
	rr->rr_counter = rr_counter;
	rr->max_ec = max_ec;
	rr->prev_max_ec = prev_max_ec;

	return v;
}

void
rr_complete(struct rr *rr, uint32_t key, uint64_t seq_num)
{

	rr->last_completed_seq_num = seq_num;
}
