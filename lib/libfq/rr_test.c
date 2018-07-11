#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "rr.h"

void rr_test_enqueue1(void)
{
	uint32_t key = 1;
	void *v = (void *)1;
	struct rr *rr = rr_init();

	int err = rr_enqueue(rr, key, v);

	assert(!err);

	rr_destroy(rr);
}

void rr_test_dequeue1(void)
{
	struct rr *rr = rr_init();

	void *v = rr_dequeue(rr);

	assert(!v);

	rr_destroy(rr);
}

void rr_test_enqueue_dequeue1(void)
{
	struct rr *rr = rr_init();
	uint32_t key = 1;
	void *ret;
	void *v = (void *)1;

	rr_enqueue(rr, key, v);

	ret = rr_dequeue(rr);

	assert(ret == v);

	rr_destroy(rr);
}

void rr_test_enqueue_dequeue2(void)
{
	struct rr *rr = rr_init();
	uint32_t key1 = 1;
	uint32_t key2 = 2;
	void *ret;
	void *v1 = (void *)1;
	void *v2 = (void *)2;

	rr_enqueue(rr, key1, v1);
	rr_enqueue(rr, key2, v2);

	ret = rr_dequeue(rr);
	assert(ret == v1);

	ret = rr_dequeue(rr);
	assert(ret == v2);

	rr_destroy(rr);
}

void rr_test_enqueue_dequeue4(void)
{
	struct rr *rr = rr_init();
	uint32_t key1 = 1;
	uint32_t key2 = 2;
	void *ret;
	void *v1 = (void *)1;
	void *v2 = (void *)2;
	void *v3 = (void *)3;
	void *v4 = (void *)4;

	rr_enqueue(rr, key1, v1);
	rr_enqueue(rr, key1, v2);
	rr_enqueue(rr, key2, v3);
	rr_enqueue(rr, key2, v4);

	ret = rr_dequeue(rr);
	assert(ret == v1);

	/* Queue 2 should be serviced before queue 1 gets to go again */
	ret = rr_dequeue(rr);
	assert(ret == v3);

	ret = rr_dequeue(rr);
	assert(ret == v2);

	ret = rr_dequeue(rr);
	assert(ret == v4);

	rr_destroy(rr);
}

int main(int argc, char **argv)
{
	rr_test_enqueue1();
	rr_test_dequeue1();
	rr_test_enqueue_dequeue1();
	rr_test_enqueue_dequeue2();
	rr_test_enqueue_dequeue4();

	return 0;
}
