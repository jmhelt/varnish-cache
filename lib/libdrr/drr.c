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
	uint32_t (*hash)(const void *v);
	uint32_void_tbl *qs;
	struct active_head active_q;
	struct drr_qn *next;
	uint32_t quantum;
	uint32_t n_active;
	bool gave_quantum;
};

struct drr*
drr_init(uint32_t quantum, uint32_t (*hash)(const void *v))
{
	struct drr *drr = malloc(sizeof(struct drr));

	drr->hash = hash;
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
	return NULL;
}
