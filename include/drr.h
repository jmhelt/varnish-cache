#ifndef DRR_H
#define	DRR_H

#include <stdbool.h>
#include <stdint.h>

/* Queue node */
struct drr_qn {
	size_t rid;
	uint64_t credit;
	bool active;
};

/* Active node */
struct drr_an {
	struct drr_an *next;
	struct drr_an *prev;
	struct drr_qn *qn;
};

/* Deficit round robin */
struct drr {
	uint32_t quantum;
	uint32_t n_active;
	struct drr_an *active_q;
	struct drr_an *next;
	bool gave_quantum;
};

void
drr_init(void);

#endif /* DRR_H */
