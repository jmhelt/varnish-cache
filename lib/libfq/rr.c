#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "rr.h"

struct rr*
rr_init(void)
{
	struct rr *rr = malloc(sizeof(struct rr));

	rr->qs = uint32_void_tbl_init(0);
	VTAILQ_INIT(&rr->active_q);
	rr->next = NULL;
	rr->n_active = 0;
	rr->round_remaining = 0;
	rr->max_surplus = 0;
	rr->prev_max_surplus = 0;
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
	struct rr_qn *qn = malloc(sizeof(struct rr_qn));
	VTAILQ_INIT(&qn->q);
	qn->key = key;
	qn->surplus = 0;
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

	/* Insert value */
	vn = malloc(sizeof(struct rr_vn));
	vn->v = v;
	VTAILQ_INSERT_TAIL(&qn->q, vn, list);

	/* If newly active, add to active queue */
	if (!qn->active) {
		VTAILQ_INSERT_TAIL(&rr->active_q, qn, list);
		rr->n_active += 1;
		qn->active = true;
	}

	return 0;
}

int32_t
rr_get_cost(uint32_t key)
{
	int32_t cost;

	switch (key) {
        case 1: // ('noop', '/index.html')
		cost = 14;
		break;
        case 2: // ('embed-json-noload', '/index-embed-json-001.html')
		cost = 90;
		break;
	case 3: // ('embed-json-noload', '/index-embed-json-005.html')
		cost = 401;
		break;
	case 4: // ('geoip2', '/index.html')
		cost = 15;
		break;
	case 5: // ('jwt', '/index.html')
		cost = 19; // jwt
		break;
	case 6: // ('syslog', '/index.html')
		cost = 16;
		break;
	case 7: // ('noop', '/rand/1k.txt')
		cost = 5;
		break;
 	case 8: // ('noop', '/rand/20k.txt')
		cost = 17;
		break;
 	case 9: // ('noop', '/rand/40k.txt')
		cost = 34;
		break;
 	case 10: // ('spin-1e4', 'index.html')
		cost = 17;
		break;
 	case 11: // ('spin-1e5', 'index.html')
		cost = 40;
		break;
 	case 12: // ('spin-1e6', 'index.html')
		cost = 273;
		break;
 	case 13: // ('spin-1e7', 'index.html')
		cost = 2600;
		break;
 	case 14: // ('spin-1e8', 'index.html')
		cost = 25876;
		break;
 	case 15: // ('spin-1e9', 'index.html')
		cost = 258635;
		break;
	default:
		cost = 1;
	}

	return cost;
}

static inline int32_t
max(int32_t x, int32_t y)
{
	if (x > y)
		return x;
	else
		return y;
}

void*
rr_dequeue(struct rr *rr)
{
	uint32_t round_remaining = rr->round_remaining;
	uint32_t n_active = rr->n_active;
	int32_t max_surplus = rr->max_surplus;
	int32_t prev_max_surplus = rr->prev_max_surplus;
	bool gave_quantum = rr->gave_quantum;
	struct rr_qn *qn = rr->next;
	struct rr_qn *tmp;
	uint32_t key;
	int32_t cost;
	struct rr_vn *vn = NULL;
	void *v = NULL;

	if (n_active == 0)
		return NULL;

	while (!vn) {
		if (round_remaining == 0) {
			round_remaining = n_active;
			prev_max_surplus = max_surplus;
			max_surplus = 0;
		}

		/* qn may be NULL if this is the first call to dequeue
		 * after rr was completely empty or if we hit the
		 * head of the active queue.
		 */
		if (!qn)
			qn = VTAILQ_FIRST(&rr->active_q);

		/* Give this queue a quantum if haven't already */
		if (!gave_quantum) {
			qn->surplus -= prev_max_surplus + 1;
			gave_quantum = true;
		}

		if (qn->surplus >= 0) {
			/* Already in surplus for this queue. Move on. */
			gave_quantum = false;
			max_surplus = max(qn->surplus, max_surplus);
			qn = VTAILQ_NEXT(qn, list);
			round_remaining -= 1;
		} else {
			assert(!VTAILQ_EMPTY(&qn->q));
			vn = VTAILQ_FIRST(&qn->q);
			VTAILQ_REMOVE(&qn->q, vn, list);
		}
	}

	key = qn->key;
	cost = rr_get_cost(key);
	qn->surplus += cost;
	max_surplus = max(qn->surplus, max_surplus);

	v = vn->v;
	free(vn);

	/* If queue is no longer active */
	if (VTAILQ_EMPTY(&qn->q)) {
		/* Remove any existing surplus */
		qn->surplus = 0;
		qn->active = false;

		/* Get next queue node before we remove this one */
		tmp = VTAILQ_NEXT(qn, list);

		/* Remove node from active queue */
		VTAILQ_REMOVE(&rr->active_q, qn, list);

		/* We'll start from tmp the next time this is called */
		qn = tmp;
		gave_quantum = false;
		n_active -= 1;
	}

	/* Store state for next call */
	rr->gave_quantum = gave_quantum;
	rr->next = qn;
	rr->n_active = n_active;
	rr->round_remaining = round_remaining;
	rr->max_surplus = max_surplus;
	rr->prev_max_surplus = prev_max_surplus;

	return v;
}

void
rr_complete(struct rr *rr, uint32_t key, int32_t cost)
{
	struct rr_qn *qn;

	/* Find or create queue */
	if (!uint32_void_tbl_find(rr->qs, &key, (void**)&qn))
		assert(false);	/* TODO: Handle error */

	return;
}
