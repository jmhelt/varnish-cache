#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "drr.h"
#include "uint32_void_tbl.h"
#include "vqueue.h"

/* Value node */
struct drr_vn {
	VTAILQ_ENTRY(drr_vn) list;
	void *v;
};

VTAILQ_HEAD(q_head, drr_vn);

/* Queue node */
struct drr_qn {
	VTAILQ_ENTRY(drr_qn) list;
	struct q_head q;
	uint32_t key;
	uint32_t credit;
	bool active;
};

VTAILQ_HEAD(active_head, drr_qn);

/* Deficit round robin */
struct drr {
	uint32_void_tbl *qs;	     /* Queues */
	struct active_head active_q; /* LL of active queues */
	struct drr_qn *next;	     /* Next queue in active list to pull from */
	uint32_t quantum;	     /* Quantum */
	uint32_t n_active;	     /* Length of active queue */
	bool gave_quantum;	     /* State for dequeue */
};

struct drr*
drr_init(uint32_t quantum)
{
	struct drr *drr = malloc(sizeof(struct drr));

	drr->qs = uint32_void_tbl_init(0);
	VTAILQ_INIT(&drr->active_q);
	drr->next = NULL;
	drr->quantum = quantum;
	drr->n_active = 0;
	drr->gave_quantum = false;

	return drr;
}

void
drr_destroy(struct drr *drr)
{
	struct drr_qn *qn;
	struct drr_vn *vn;
	uint32_void_tbl_const_iterator *end;
	uint32_void_tbl_const_iterator *it;
	uint32_void_tbl_locked_table *locked = uint32_void_tbl_lock_table(drr->qs);

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
	uint32_void_tbl_free(drr->qs);
	free(drr);
}

struct drr_qn*
drr_add_queue(struct drr *drr, uint32_t key)
{
	struct drr_qn *qn = malloc(sizeof(struct drr_qn));
	VTAILQ_INIT(&qn->q);
	qn->key = key;
	qn->credit = 0;
	qn->active = false;

	if (!uint32_void_tbl_insert(drr->qs, &key, (void**)&qn)) {
		// TODO: Handle error
	}

	return qn;
}

int
drr_enqueue(struct drr *drr, uint32_t key, void *v)
{
	struct drr_qn *qn;
	struct drr_vn *vn;

	/* Find or create queue */
	if (!uint32_void_tbl_find(drr->qs, &key, (void**)&qn))
		qn = drr_add_queue(drr, key);

	/* Insert value */
	vn = malloc(sizeof(struct drr_vn));
	vn->v = v;
	VTAILQ_INSERT_TAIL(&qn->q, vn, list);

	/* If newly active, add to active queue */
	if (!qn->active) {
		VTAILQ_INSERT_TAIL(&drr->active_q, qn, list);
		drr->n_active += 1;
		qn->active = true;
	}

	return 0;
}

void*
drr_dequeue(struct drr *drr)
{
	bool gave_quantum = drr->gave_quantum;
	struct drr_qn *qn = drr->next;
	struct drr_qn *tmp;
	struct drr_vn *vn = NULL;
	uint32_t n_active = drr->n_active;
	uint32_t quantum = drr->quantum;
	void *v = NULL;

	if (n_active == 0)
		return NULL;

	while (!vn) {

		/* qn may be NULL if this is the first call to dequeue
		 * after drr was completely empty or if we hit the
		 * head of the active queue.
		 */
		if (!qn)
			qn = VTAILQ_FIRST(&drr->active_q);

		/* Give this queue a quantum if haven't already */
		if (!gave_quantum) {
			qn->credit += quantum;
			gave_quantum = true;
		}

		/* Get first value from the queue */
		assert(!VTAILQ_EMPTY(&qn->q));
		vn = VTAILQ_FIRST(&qn->q);

		/* TODO: Make this a real cost function based on value */
		/* Perhaps we should pass in a cost_func as param? */
		uint32_t cost = 1;

		if (qn->credit >= cost) {
			VTAILQ_REMOVE(&qn->q, vn, list);
			qn->credit -= cost;
		} else {
			/* Not enough credit left for this queue. Move on. */
			vn = NULL;
			gave_quantum = false;
			tmp = VTAILQ_NEXT(qn, list);
			qn = tmp;
		}
	}

	if (vn) {
		v = vn->v;
		free(vn);

		/* If queue is no longer active */
		if (VTAILQ_EMPTY(&qn->q)) {
			/* Remove any existing credit */
			qn->credit = 0;
			qn->active = false;

			/* Get next queue node before we remove this one */
			tmp = VTAILQ_NEXT(qn, list);

			/* Remove node from active queue */
			VTAILQ_REMOVE(&drr->active_q, qn, list);

			/* We'll start from tmp the next time this is called */
			qn = tmp;
			gave_quantum = false;
			n_active -= 1;
		}
	}

	/* Store state for next call */
	drr->gave_quantum = gave_quantum;
	drr->next = qn;
	drr->n_active = n_active;

	return v;
}
