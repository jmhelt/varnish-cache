#ifndef DRR_H
#define	DRR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct drr;

struct drr*
drr_init(uint32_t, uint32_t (*)(const void *v));

void
drr_destroy(struct drr*);

int
drr_enqueue(struct drr*, void*);

void*
drr_dequeue(struct drr*);

#ifdef __cplusplus
}
#endif
#endif /* DRR_H */
