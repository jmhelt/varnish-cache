#ifndef DRR_H
#define	DRR_H

#include <stdint.h>

struct drr;

struct drr*
drr_init(uint32_t);

void
drr_destroy(struct drr*);

int
drr_enqueue(struct drr*, uint32_t, void*);

void*
drr_dequeue(struct drr*);

#endif /* DRR_H */
