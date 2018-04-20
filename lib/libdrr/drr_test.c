#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "drr.h"

void drr_test_enqueue1(void)
{
	uint32_t key = 1;
	void *v = (void *)1;
	struct drr *drr = drr_init(1);

	int err = drr_enqueue(drr, key, v);

	assert(!err);

	drr_destroy(drr);
}

void drr_test_dequeue1(void)
{
	struct drr *drr = drr_init(1);

	void *v = drr_dequeue(drr);

	assert(!v);

	drr_destroy(drr);
}

void drr_test_enqueue_dequeue1(void)
{
	struct drr *drr = drr_init(1);
	uint32_t key = 1;
	void *ret;
	void *v = (void *)1;

	drr_enqueue(drr, key, v);
	ret = drr_dequeue(drr);

	assert(ret == v);

	drr_destroy(drr);
}

void drr_test_enqueue_dequeue2(void)
{
	struct drr *drr = drr_init(1);
	uint32_t key1 = 1;
	uint32_t key2 = 2;
	void *ret;
	void *v1 = (void *)1;
	void *v2 = (void *)2;

	drr_enqueue(drr, key1, v1);
	drr_enqueue(drr, key2, v2);

	ret = drr_dequeue(drr);
	assert(ret == v1);

	ret = drr_dequeue(drr);
	assert(ret == v2);

	drr_destroy(drr);
}

void drr_test_enqueue_dequeue4(void)
{
	struct drr *drr = drr_init(1);
	uint32_t key1 = 1;
	uint32_t key2 = 2;
	void *ret;
	void *v1 = (void *)1;
	void *v2 = (void *)2;
	void *v3 = (void *)3;
	void *v4 = (void *)4;

	drr_enqueue(drr, key1, v1);
	drr_enqueue(drr, key1, v2);
	drr_enqueue(drr, key2, v3);
	drr_enqueue(drr, key2, v4);

	ret = drr_dequeue(drr);
	assert(ret == v1);

	/* Queue 2 should be serviced before queue 1 gets to go again */
	ret = drr_dequeue(drr);
	assert(ret == v3);

	ret = drr_dequeue(drr);
	assert(ret == v2);

	ret = drr_dequeue(drr);
	assert(ret == v4);

	drr_destroy(drr);
}

int main(int argc, char **argv)
{
	drr_test_enqueue1();
	drr_test_dequeue1();
	drr_test_enqueue_dequeue1();
	drr_test_enqueue_dequeue2();
	drr_test_enqueue_dequeue4();

	return 0;
}
