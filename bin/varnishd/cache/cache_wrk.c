/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Worker thread stuff unrelated to the worker thread pools.
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "cache_varnishd.h"
#include "cache_pool.h"

#include "vtim.h"

#include "hash/hash_slinger.h"

static void Pool_Work_Thread(struct pool *pp, struct worker *wrk);

/*--------------------------------------------------------------------
 * Create and start a back-ground thread which as its own worker and
 * session data structures;
 */

struct bgthread {
	unsigned	magic;
#define BGTHREAD_MAGIC	0x23b5152b
	const char	*name;
	bgthread_t	*func;
	void		*priv;
};

static void *
wrk_bgthread(void *arg)
{
	struct bgthread *bt;
	struct worker wrk;
	struct VSC_main ds;

	CAST_OBJ_NOTNULL(bt, arg, BGTHREAD_MAGIC);
	THR_SetName(bt->name);
	THR_Init();
	INIT_OBJ(&wrk, WORKER_MAGIC);
	memset(&ds, 0, sizeof ds);
	wrk.stats = &ds;

	(void)bt->func(&wrk, bt->priv);

	WRONG("BgThread terminated");

	NEEDLESS(return NULL);
}

void
WRK_BgThread(pthread_t *thr, const char *name, bgthread_t *func, void *priv)
{
	struct bgthread *bt;

	ALLOC_OBJ(bt, BGTHREAD_MAGIC);
	AN(bt);

	bt->name = name;
	bt->func = func;
	bt->priv = priv;
	AZ(pthread_create(thr, NULL, wrk_bgthread, bt));
}

/*--------------------------------------------------------------------*/
static void
WRK_Thread(struct pool *qp, size_t stacksize, unsigned thread_workspace)
{
	struct worker *w, ww;
	struct VSC_main ds;
	unsigned char ws[thread_workspace];

	AN(qp);
	AN(stacksize);
	AN(thread_workspace);

	THR_SetName("cache-worker");
	w = &ww;
	INIT_OBJ(w, WORKER_MAGIC);
	w->lastused = NAN;
	memset(&ds, 0, sizeof ds);
	w->stats = &ds;
	AZ(pthread_cond_init(&w->cond, NULL));

	WS_Init(w->aws, "wrk", ws, thread_workspace);

	VSL(SLT_WorkThread, 0, "%p start", w);
	Pool_Work_Thread(qp, w);
	AZ(w->pool);

	VSL(SLT_WorkThread, 0, "%p end", w);
	if (w->vcl != NULL)
		VCL_Rel(&w->vcl);
	AZ(pthread_cond_destroy(&w->cond));
	HSH_Cleanup(w);
	Pool_Sumstat(w);
}

/*--------------------------------------------------------------------
 * Summing of stats into pool counters
 */

static void
pool_addstat(struct VSC_main *dst, struct VSC_main *src)
{

	dst->summs++;
	VSC_main_Summ(dst, src);
	memset(src, 0, sizeof *src);
}

static inline int
pool_reserve(void)
{
	unsigned lim = cache_param->wthread_reserve;

	if (cache_param->wthread_reserve == 0)
		return (cache_param->wthread_min / 20 + 1);
	//lim = cache_param->wthread_min * 950 / 1000;
	if (cache_param->wthread_reserve > lim)
		return (lim);
	return (cache_param->wthread_reserve);
}

/*--------------------------------------------------------------------*/

static struct worker *
pool_getidleworker(struct pool *pp, enum task_prio prio)
{
	struct pool_task *pt = NULL;
	struct worker *wrk;

	CHECK_OBJ_NOTNULL(pp, POOL_MAGIC);
	Lck_AssertHeld(&pp->mtx);
	if (prio <= TASK_QUEUE_RESERVE || pp->nidle > pool_reserve()) {
		pt = VTAILQ_FIRST(&pp->idle_queue);
		if (pt == NULL)
			AZ(pp->nidle);
	}

	if (pt == NULL) {
		if (pp->nthr < cache_param->wthread_max) {
			pp->dry++;
			AZ(pthread_cond_signal(&pp->herder_cond));
		}
		return (NULL);
	}
	AZ(pt->func);
	CAST_OBJ_NOTNULL(wrk, pt->priv, WORKER_MAGIC);
	return (wrk);
}

/*--------------------------------------------------------------------
 * Special scheduling:  If no thread can be found, the current thread
 * will be prepared for rescheduling instead.
 * The selected threads workspace is reserved and the argument put there.
 * Return one if another thread was scheduled, otherwise zero.
 */

int
Pool_Task_Arg(struct worker *wrk, enum task_prio prio, task_func_t *func,
    const void *arg, size_t arg_len)
{
	struct pool *pp;
	struct worker *wrk2;
	int retval;

	CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
	AN(arg);
	AN(arg_len);
	pp = wrk->pool;
	CHECK_OBJ_NOTNULL(pp, POOL_MAGIC);

	Lck_Lock(&pp->mtx);
	wrk2 = pool_getidleworker(pp, prio);
	if (wrk2 != NULL) {
		AN(pp->nidle);
		VTAILQ_REMOVE(&pp->idle_queue, &wrk2->task, list);
		pp->nidle--;
		retval = 1;
	} else {
		wrk2 = wrk;
		retval = 0;
	}
	Lck_Unlock(&pp->mtx);
	AZ(wrk2->task.func);

	assert(arg_len <= WS_Reserve(wrk2->aws, arg_len));
	memcpy(wrk2->aws->f, arg, arg_len);
	wrk2->task.func = func;
	wrk2->task.priv = wrk2->aws->f;
	if (retval)
		AZ(pthread_cond_signal(&wrk2->cond));
	return (retval);
}

/*--------------------------------------------------------------------
 * Enter a new task to be done
 */

int
Pool_Task(struct pool *pp, struct pool_task *task, enum task_prio prio)
{
	struct worker *wrk;
	int retval = 0;
	CHECK_OBJ_NOTNULL(pp, POOL_MAGIC);
	AN(task);
	AN(task->func);
	assert(prio < TASK_QUEUE_END);

	Lck_Lock(&pp->mtx);

	/* The common case first:  Take an idle thread, do it. */

	wrk = pool_getidleworker(pp, prio);
	if (wrk != NULL) {
		AN(pp->nidle);
		VTAILQ_REMOVE(&pp->idle_queue, &wrk->task, list);
		pp->nidle--;
		AZ(wrk->task.func);
		wrk->task.func = task->func;
		wrk->task.priv = task->priv;
		Lck_Unlock(&pp->mtx);
		AZ(pthread_cond_signal(&wrk->cond));
		return (0);
	}

	/*
	 * queue limits only apply to client threads - all other
	 * work is vital and needs do be done at the earliest
	 */
	//if (!TASK_QUEUE_CLIENT(prio) ||
	//    pp->lqueue + pp->nthr < cache_param->wthread_max +
	//    cache_param->wthread_queue_limit) {
		pp->nqueued++;
		pp->lqueue++;
		VTAILQ_INSERT_TAIL(&pp->queues[prio], task, list);
	//} else {
	//	if (prio == TASK_QUEUE_REQ)
	//		pp->sdropped++;
	//	else
	//		pp->rdropped++;
	//	retval = -1;
	//}
	Lck_Unlock(&pp->mtx);
	return (retval);
}

/*--------------------------------------------------------------------
 * Empty function used as a pointer value for the thread exit condition.
 */

static void v_matchproto_(task_func_t)
pool_kiss_of_death(struct worker *wrk, void *priv)
{
	(void)wrk;
	(void)priv;
}


static inline int64_t
int64_max(int64_t x, int64_t y)
{
	if (x > y)
		return x;
	else
		return y;
}


void *
rr_batch_dequeue(struct rr *rr, uint64_t *seq_num) {
	uint32_t rr_counter = rr->rr_counter;
	uint32_t n_active = rr->n_active;
	int64_t max_ec = rr->max_ec;
	int64_t prev_max_ec = rr->prev_max_ec;
	bool gave_quantum = rr->gave_quantum;
	struct rr_qn *qn = rr->next;
	struct rr_qn *tmp = NULL;
	struct rr_vn *vn = NULL;
	struct rr_vn *vn2 = NULL;
	struct rr_b_vn *bvn = NULL;
	void *v = NULL;
	int64_t ec = 0;

//	VSL(SLT_Debug, 0, "rr_batch_dequeue starts.");

	if (n_active == 0) {
//		VSL(SLT_Debug, 0, "rr_batch_dequeue ends.");
		return NULL;
	}

	if (!VTAILQ_EMPTY(&rr->batch_q)) {
//		VSL(SLT_Debug, 0, "Returning subsequent request in batch.");

		bvn = VTAILQ_FIRST(&rr->batch_q);
		VTAILQ_REMOVE(&rr->batch_q, bvn, list);
		v = bvn->v;
		*seq_num = bvn->seq_num;
		free(bvn);
//		VSL(SLT_Debug, 0, "Subsequent request returning.");
//		VSL(SLT_Debug, 0, "rr_batch_dequeue ends.");
		return v;
	}

	if (rr_counter == 0) {
		rr_counter = n_active;
		prev_max_ec = max_ec;
		max_ec = 0;
	}

	/* qn may be NULL if this is the first call to dequeue
	 * after rr was completely empty or if we hit the
	 * head of the active queue.
	 */
	qn = rr->next;
	if (!qn)
		qn = VTAILQ_FIRST(&rr->active_q);

	ec = qn->ec;
	if (!gave_quantum) {
		ec -= prev_max_ec;
		gave_quantum = true;
	}

	if (rr->last_completed_seq_num >= qn->prev_seq_num) {
		vn = VTAILQ_FIRST(&qn->q);
		VTAILQ_REMOVE(&qn->q, vn, list);

		ec += rr_cost(qn->key);

		v = vn->v;
		free(vn);
		*seq_num = qn->seq_num;

		uint32_t batch_residual_count = qn->batch_size - 1;

		while (batch_residual_count > 0 && !VTAILQ_EMPTY(&qn->q)) {
//			VSL(SLT_Debug, 0, "Putting new request in batch.");

			vn2 = VTAILQ_FIRST(&qn->q);
			VTAILQ_REMOVE(&qn->q, vn2, list);

			ec += rr_cost(qn->key);

			bvn = calloc(1, sizeof(struct rr_b_vn));
			bvn->v = vn2->v;
			free(vn2);
			bvn->seq_num = qn->seq_num;

			VTAILQ_INSERT_TAIL(&rr->batch_q, bvn, list);

			if (!VTAILQ_EMPTY(&qn->q)) { // exhausted quantum
				rr->next_seq_num += 1;
				qn->prev_seq_num = qn->seq_num;
				qn->seq_num = rr->next_seq_num;
			}

			batch_residual_count--;

//			VSL(SLT_Debug, 0, "Batching loop back.");
		}

		// We need to advance to next queue
		if (ec > 0 || VTAILQ_EMPTY(&qn->q)) {
			/* Get next queue node before we potentially remove this one */
			tmp = VTAILQ_NEXT(qn, list);

			if (!VTAILQ_EMPTY(&qn->q)) { // exhausted quantum
				rr->next_seq_num += 1;
				qn->prev_seq_num = qn->seq_num;
				qn->seq_num = rr->next_seq_num;
			} else { // queue is empty
				ec = 0; // Reset excess counter

				/* Remove node from active queue */
				qn->active = false;
				VTAILQ_REMOVE(&rr->active_q, qn, list);
				n_active -= 1;
			}

			qn->ec = ec;
			max_ec = int64_max(ec, max_ec);
			rr_counter -= 1;

			gave_quantum = false;
			qn = tmp; // Advance to next queue
		}


	} else {
		v = NULL; // Slow down the scheduler
	}


	/* Store state for next call */
	rr->gave_quantum = gave_quantum;
	rr->next = qn;
	rr->n_active = n_active;
	rr->rr_counter = rr_counter;
	rr->max_ec = max_ec;
	rr->prev_max_ec = prev_max_ec;

//	VSL(SLT_Debug, 0, "rr_batch_dequeue ends.");
	return v;
}


//static void *
//local_rr_dequeue(struct rr *rr, uint64_t *seq_num) {
//	uint32_t rr_counter = rr->rr_counter;
//	uint32_t n_active = rr->n_active;
//	int64_t max_ec = rr->max_ec;
//	int64_t prev_max_ec = rr->prev_max_ec;
//	bool gave_quantum = rr->gave_quantum;
//	struct rr_qn *qn = rr->next;
//	struct rr_qn *tmp = NULL;
//	struct rr_vn *vn = NULL;
//	void *v = NULL;
//	int64_t ec = 0;
//
//	if (n_active == 0)
//		return NULL;
//
//	if (rr_counter == 0) {
//		rr_counter = n_active;
//		prev_max_ec = max_ec;
//		max_ec = 0;
//	}
//
//	/* qn may be NULL if this is the first call to dequeue
//	 * after rr was completely empty or if we hit the
//	 * head of the active queue.
//	 */
//	qn = rr->next;
//	if (!qn)
//		qn = VTAILQ_FIRST(&rr->active_q);
//
//	ec = qn->ec;
//	if (!gave_quantum) {
//		ec -= prev_max_ec;
//		gave_quantum = true;
//	}
//
//	if (rr->last_completed_seq_num >= qn->prev_seq_num) {
//		vn = VTAILQ_FIRST(&qn->q);
//		VTAILQ_REMOVE(&qn->q, vn, list);
//
//		ec += rr_cost(qn->key);
//
//		v = vn->v;
//		free(vn);
//		*seq_num = qn->seq_num;
//
//		VSL(SLT_Debug, 0, "New request dequeued");
//
//		// We need to advance to next queue
//		if (ec > 0 || VTAILQ_EMPTY(&qn->q)) {
//			/* Get next queue node before we potentially remove this one */
//			tmp = VTAILQ_NEXT(qn, list);
//
//			if (!VTAILQ_EMPTY(&qn->q)) { // exhausted quantum
//				rr->next_seq_num += 1;
//				qn->prev_seq_num = qn->seq_num;
//				qn->seq_num = rr->next_seq_num;
//			} else { // queue is empty
//				ec = 0; // Reset excess counter
//
//				/* Remove node from active queue */
//				qn->active = false;
//				VTAILQ_REMOVE(&rr->active_q, qn, list);
//				n_active -= 1;
//			}
//
//			qn->ec = ec;
//			max_ec = int64_max(ec, max_ec);
//			rr_counter -= 1;
//
//			gave_quantum = false;
//			qn = tmp; // Advance to next queue
//		}
//	} else {
//		v = NULL; // Slow down the scheduler
//	}
//
//
//	/* Store state for next call */
//	rr->gave_quantum = gave_quantum;
//	rr->next = qn;
//	rr->n_active = n_active;
//	rr->rr_counter = rr_counter;
//	rr->max_ec = max_ec;
//	rr->prev_max_ec = prev_max_ec;
//
//	return v;
//}


/*--------------------------------------------------------------------
 * This is the work function for worker threads in the pool.
 */

static void
Pool_Work_Thread(struct pool *pp, struct worker *wrk)
{
	struct worker *wrk2 = NULL;
	struct pool_task *tp = NULL, *tp2 = NULL;
	struct pool_task tpx, tps;
	int i, prio_lim, n = 2;
	uint64_t seq_num;

	CHECK_OBJ_NOTNULL(pp, POOL_MAGIC);
	wrk->pool = pp;
	while (1) {
		Lck_Lock(&pp->mtx);

		CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);

		WS_Reset(wrk->aws, 0);
		AZ(wrk->vsl);

		//if (pp->nidle < pool_reserve())
		//	prio_lim = TASK_QUEUE_RESERVE + 1;
		//else
		prio_lim = TASK_QUEUE_END;

		for (i = 0; i < prio_lim; i++) {
			tp = VTAILQ_FIRST(&pp->queues[i]);
			if (tp != NULL) {
				pp->lqueue--;
				VTAILQ_REMOVE(&pp->queues[i], tp, list);
				break;
			}
		}

		if (tp == NULL) {
//			VSL(SLT_Debug, 0, "Calling rr_dequeue");

//			tp = (struct pool_task*)local_rr_dequeue(pp->fair_queue, &seq_num);
			tp = (struct pool_task*)rr_batch_dequeue(pp->fair_queue, &seq_num);
			if (tp != NULL) {
				pp->lqueue--;
				((struct req*)tp->priv)->seq_num = seq_num;
				
				for (i = 0; i < n - 1; i++) {
					wrk2 = pool_getidleworker(pp, TASK_QUEUE_REQ);
					if (wrk2 == NULL)
						break;

//					VSL(SLT_Debug, 0, "Calling rr_dequeue in new worker");
//					tp2 = (struct pool_task*)local_rr_dequeue(pp->fair_queue, &seq_num);
					tp2 = (struct pool_task*)rr_batch_dequeue(pp->fair_queue, &seq_num);
					if (tp2 == NULL)
						break;

					pp->lqueue--;
					((struct req*)tp2->priv)->seq_num = seq_num;
					VTAILQ_REMOVE(&pp->idle_queue, &wrk2->task, list);
					pp->nidle--;
					AZ(wrk2->task.func);
					wrk2->task.func = tp2->func;
					wrk2->task.priv = tp2->priv;
					AZ(pthread_cond_signal(&wrk2->cond));
				}

			}
			
		}

		if ((tp == NULL && wrk->stats->summs > 0) ||
		    (wrk->stats->summs >= cache_param->wthread_stats_rate))
			pool_addstat(pp->a_stat, wrk->stats);

		if (tp != NULL) {
			wrk->stats->summs++;
		} else if (pp->b_stat != NULL && pp->a_stat->summs) {
			/* Nothing to do, push pool stats into global pool */
			tps.func = pool_stat_summ;
			tps.priv = pp->a_stat;
			pp->a_stat = pp->b_stat;
			pp->b_stat = NULL;
			tp = &tps;
		} else {
			/* Nothing to do: To sleep, perchance to dream ... */
			if (isnan(wrk->lastused))
				wrk->lastused = VTIM_real();
			wrk->task.func = NULL;
			wrk->task.priv = wrk;
			VTAILQ_INSERT_HEAD(&pp->idle_queue, &wrk->task, list);
			pp->nidle++;
			do {
				i = Lck_CondWait(&wrk->cond, &pp->mtx,
				    wrk->vcl == NULL ?  0 : wrk->lastused+60.);
				if (i == ETIMEDOUT)
					VCL_Rel(&wrk->vcl);
			} while (wrk->task.func == NULL);
			tpx = wrk->task;
			tp = &tpx;
			wrk->stats->summs++;
		}
		Lck_Unlock(&pp->mtx);

		if (tp->func == pool_kiss_of_death)
			break;

		do {
			memset(&wrk->task, 0, sizeof wrk->task);
			assert(wrk->pool == pp);
			tp->func(wrk, tp->priv);
			if (DO_DEBUG(DBG_VCLREL) && wrk->vcl != NULL)
				VCL_Rel(&wrk->vcl);
			tpx = wrk->task;
			tp = &tpx;
		} while (tp->func != NULL);

		/* cleanup for next task */
		wrk->seen_methods = 0;
	}
	wrk->pool = NULL;
}

/*--------------------------------------------------------------------
 * Create another worker thread.
 */

struct pool_info {
	unsigned		magic;
#define POOL_INFO_MAGIC		0x4e4442d3
	size_t			stacksize;
	struct pool		*qp;
};

static void *
pool_thread(void *priv)
{
	struct pool_info *pi;

	CAST_OBJ_NOTNULL(pi, priv, POOL_INFO_MAGIC);
	THR_Init();
	WRK_Thread(pi->qp, pi->stacksize, cache_param->workspace_thread);
	FREE_OBJ(pi);
	return (NULL);
}

static void
pool_breed(struct pool *qp)
{
	pthread_t tp;
	pthread_attr_t tp_attr;
	struct pool_info *pi;

	AZ(pthread_attr_init(&tp_attr));
	AZ(pthread_attr_setdetachstate(&tp_attr, PTHREAD_CREATE_DETACHED));

	/* Set the stacksize for worker threads we create */
	if (cache_param->wthread_stacksize != UINT_MAX)
		AZ(pthread_attr_setstacksize(&tp_attr,
		    cache_param->wthread_stacksize));

	ALLOC_OBJ(pi, POOL_INFO_MAGIC);
	AN(pi);
	AZ(pthread_attr_getstacksize(&tp_attr, &pi->stacksize));
	pi->qp = qp;

	if (pthread_create(&tp, &tp_attr, pool_thread, pi)) {
		VSL(SLT_Debug, 0, "Create worker thread failed %d %s",
		    errno, strerror(errno));
		Lck_Lock(&pool_mtx);
		VSC_C_main->threads_failed++;
		Lck_Unlock(&pool_mtx);
		VTIM_sleep(cache_param->wthread_fail_delay);
	} else {
		qp->dry = 0;
		qp->nthr++;
		Lck_Lock(&pool_mtx);
		VSC_C_main->threads++;
		VSC_C_main->threads_created++;
		Lck_Unlock(&pool_mtx);
		VTIM_sleep(cache_param->wthread_add_delay);
	}

	AZ(pthread_attr_destroy(&tp_attr));
}

/*--------------------------------------------------------------------
 * Herd a single pool
 *
 * This thread wakes up every thread_pool_timeout seconds, whenever a pool
 * queues and when threads need to be destroyed
 *
 * The trick here is to not be too aggressive about creating threads.  In
 * pool_breed(), we sleep whenever we create a thread and a little while longer
 * whenever we fail to, hopefully missing a lot of cond_signals in the meantime.
 *
 * Idle threads are destroyed at a rate determined by wthread_destroy_delay
 *
 * XXX: probably need a lot more work.
 *
 */

void*
pool_herder(void *priv)
{
	struct pool *pp;
	struct pool_task *pt;
	double t_idle;
	struct worker *wrk;
	double delay;
	int wthread_min;

	CAST_OBJ_NOTNULL(pp, priv, POOL_MAGIC);

	THR_SetName("pool_herder");
	THR_Init();

	while (!pp->die || pp->nthr > 0) {
		wthread_min = cache_param->wthread_min;
		if (pp->die)
			wthread_min = 0;

		/* Make more threads if needed and allowed */
		if (pp->nthr < wthread_min ||
		    (pp->dry && pp->nthr < cache_param->wthread_max)) {
			pool_breed(pp);
			continue;
		}

		delay = cache_param->wthread_timeout;
		assert(pp->nthr >= wthread_min);

		if (pp->nthr > wthread_min) {

			t_idle = VTIM_real() - cache_param->wthread_timeout;

			Lck_Lock(&pp->mtx);
			/* XXX: unsafe counters */
			VSC_C_main->sess_queued += pp->nqueued;
			VSC_C_main->sess_dropped += pp->sdropped;
			VSC_C_main->req_dropped += pp->rdropped;
			pp->nqueued = pp->sdropped = pp->rdropped = 0;

			wrk = NULL;
			pt = VTAILQ_LAST(&pp->idle_queue, taskhead);
			if (pt != NULL) {
				AN(pp->nidle);
				AZ(pt->func);
				CAST_OBJ_NOTNULL(wrk, pt->priv, WORKER_MAGIC);

				if (pp->die || wrk->lastused < t_idle ||
				    pp->nthr > cache_param->wthread_max) {
					/* Give it a kiss on the cheek... */
					VTAILQ_REMOVE(&pp->idle_queue,
					    &wrk->task, list);
					pp->nidle--;
					wrk->task.func = pool_kiss_of_death;
					AZ(pthread_cond_signal(&wrk->cond));
				} else {
					delay = wrk->lastused - t_idle;
					wrk = NULL;
				}
			}
			Lck_Unlock(&pp->mtx);

			if (wrk != NULL) {
				pp->nthr--;
				Lck_Lock(&pool_mtx);
				VSC_C_main->threads--;
				VSC_C_main->threads_destroyed++;
				Lck_Unlock(&pool_mtx);
				delay = cache_param->wthread_destroy_delay;
			} else if (delay < cache_param->wthread_destroy_delay)
				delay = cache_param->wthread_destroy_delay;
		}

		if (pp->die) {
			if (delay < 2)
				delay = 10e-3;
			else
				delay = 1;
			VTIM_sleep(delay);
			continue;
		}
		Lck_Lock(&pp->mtx);
		if (!pp->dry) {
			if (DO_DEBUG(DBG_VTC_MODE))
				delay = 0.5;
			(void)Lck_CondWait(&pp->herder_cond, &pp->mtx,
				VTIM_real() + delay);
		} else {
			/* XXX: unsafe counters */
			VSC_C_main->threads_limited++;
			pp->dry = 0;
		}
		Lck_Unlock(&pp->mtx);
	}
	return (NULL);
}
