#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "uint32_void_tbl.h"

#include "vqueue.h"
#include "drr.h"

#ifdef __cplusplus
extern "C" {
#endif

VSTAILQ_HEAD(q_head, pool_task);

/* Queue node */
struct drr_qn {
	struct q_head q;
	uint32_t rid;
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

int
drr_enqueue(struct drr *drr, void *v)
{
	return 0;
}

void*
drr_dequeue(struct drr *drr)
{
	return NULL;
}

#ifdef __cplusplus
}
#endif

