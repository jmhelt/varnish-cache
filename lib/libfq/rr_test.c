#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "rr.h"

void
rr_test_enqueue1(void)
{
	uint32_t key = 1;
	void *v = (void *)1;
	struct rr *rr = rr_init(1);

	int err = rr_enqueue(rr, key, v);

	assert(!err);

	rr_destroy(rr);
}

void
rr_test_dequeue1(void)
{
	struct rr *rr = rr_init(1);

	void *v = rr_dequeue(rr);

	assert(!v);

	rr_destroy(rr);
}

void
rr_test_enqueue_dequeue1(void)
{
	struct rr *rr = rr_init(1);
	uint32_t key = 1;
	void *ret;
	void *v = (void *)1;

	rr_enqueue(rr, key, v);

	ret = rr_dequeue(rr);

	assert(ret == v);

	rr_destroy(rr);
}

void
rr_test_enqueue_dequeue2(void)
{
	struct rr *rr = rr_init(1);
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

void
rr_test_enqueue_dequeue4(void)
{
	struct rr *rr = rr_init(1);
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

void
rr_test_example_from_paper(void)
{
	struct rr *rr = rr_init(1);
	uint32_t key1 = 1;
	uint32_t key2 = 2;
	uint32_t key3 = 3;
	uint32_t cost;
	void *ret;
	void *v11 = (void *)11;
	void *v12 = (void *)12;
	void *v13 = (void *)13;
	void *v14 = (void *)14;

	void *v21 = (void *)21;
	void *v22 = (void *)22;
	void *v23 = (void *)23;
	void *v24 = (void *)24;
	void *v25 = (void *)25;

	void *v31 = (void *)31;
	void *v32 = (void *)32;
	void *v33 = (void *)33;
	void *v34 = (void *)34;
	void *v35 = (void *)35;

	rr_enqueue(rr, key1, v11);
	rr_enqueue(rr, key1, v12);
	rr_enqueue(rr, key1, v13);
	rr_enqueue(rr, key1, v14);

	rr_enqueue(rr, key2, v21);
	rr_enqueue(rr, key2, v22);
	rr_enqueue(rr, key2, v23);
	rr_enqueue(rr, key2, v24);
	rr_enqueue(rr, key2, v25);

	rr_enqueue(rr, key3, v31);
	rr_enqueue(rr, key3, v32);
	rr_enqueue(rr, key3, v33);
	rr_enqueue(rr, key3, v34);
	rr_enqueue(rr, key3, v35);

	/* Req 1.1: cost 32 */
	ret = rr_dequeue(rr);
	assert(ret == v11);
	cost = 32;
	rr_complete(rr, key1, ret, &cost);

	/* Req 2.1: cost 16 */
	ret = rr_dequeue(rr);
	assert(ret == v21);
	cost = 16;
	rr_complete(rr, key2, ret, &cost);

	/* Req 3.1: cost 24 */
	ret = rr_dequeue(rr);
	assert(ret == v31);
	cost = 24;
	rr_complete(rr, key3, ret, &cost);

	/* Req 1.2: cost 8 */
	ret = rr_dequeue(rr);
	assert(ret == v12);
	cost = 8;
	rr_complete(rr, key1, ret, &cost);

	/* Req 2.2: cost 8 */
	ret = rr_dequeue(rr);
	assert(ret == v22);
	cost = 8;
	rr_complete(rr, key2, ret, &cost);

	/* Req 2.3: cost 8 */
	ret = rr_dequeue(rr);
	assert(ret == v23);
	cost = 8;
	rr_complete(rr, key2, ret, &cost);

	/* Req 2.4: cost 24 */
	ret = rr_dequeue(rr);
	assert(ret == v24);
	cost = 24;
	rr_complete(rr, key2, ret, &cost);

	/* Req 3.2: cost 4 */
	ret = rr_dequeue(rr);
	assert(ret == v32);
	cost = 4;
	rr_complete(rr, key3, ret, &cost);

	/* Req 3.3: cost 8 */
	ret = rr_dequeue(rr);
	assert(ret == v33);
	cost = 8;
	rr_complete(rr, key3, ret, &cost);

	/* Req 1.3: cost 12 */
	ret = rr_dequeue(rr);
	assert(ret == v13);
	cost = 12;
	rr_complete(rr, key1, ret, &cost);

	/* Req 1.4: cost 16 */
	ret = rr_dequeue(rr);
	assert(ret == v14);
	cost = 16;
	rr_complete(rr, key1, ret, &cost);

	/* Req 2.5: cost 4 */
	ret = rr_dequeue(rr);
	assert(ret == v25);
	cost = 4;
	rr_complete(rr, key2, ret, &cost);

	/* Req 3.4: cost 20 */
	ret = rr_dequeue(rr);
	assert(ret == v34);
	cost = 20;
	rr_complete(rr, key3, ret, &cost);

	/* Req 3.5: cost 32 */
	ret = rr_dequeue(rr);
	assert(ret == v35);
	cost = 32;
	rr_complete(rr, key3, ret, &cost);

	rr_destroy(rr);
}

int main(int argc, char **argv)
{
	rr_test_enqueue1();
	rr_test_dequeue1();
	rr_test_enqueue_dequeue1();
	rr_test_enqueue_dequeue2();
	rr_test_enqueue_dequeue4();
	rr_test_example_from_paper();

	return 0;
}
