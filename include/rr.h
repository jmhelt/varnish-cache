#ifndef RR_H
#define	RR_H

#include <stdbool.h>
#include <stdint.h>

#include "uint32_void_tbl.h"
#include "vqueue.h"

/* Value node */
struct rr_vn {
	VTAILQ_ENTRY(rr_vn) list;
	void *v;
	uint32_t amount_charged;
};

VTAILQ_HEAD(q_head, rr_vn);

/* Queue node */
struct rr_qn {
	VTAILQ_ENTRY(rr_qn) list;
	struct q_head q;
//	struct q_head in_progress;
	int64_t surplus;
	uint32_t cost;
	uint32_t key;
	bool active;
};

VTAILQ_HEAD(active_head, rr_qn);

/* Round robin */
struct rr {
	uint32_void_tbl *qs;	     /* Queues */
	struct active_head active_q; /* LL of active queues */
	struct rr_qn *next;	     /* Next queue in active list to pull from */
	int64_t max_surplus;	     /* Max surplus in this round */
	int64_t prev_max_surplus;    /* Max surplus in previous round */
	uint32_t n_active;	     /* Length of active queue */
	uint32_t round_remaining;    /* Number queues left in this round */
	bool gave_quantum;	     /* State for dequeue */
};

struct rr*
rr_init(void);

void
rr_destroy(struct rr *rr);

int
rr_enqueue(struct rr *rr, uint32_t key, void *v);

void*
rr_dequeue(struct rr *rr);

void
rr_complete(struct rr *rr, uint32_t key, void *v, uint32_t cost);

#endif /* RR_H */