#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct thread_task {
	thread_task_f function;
	void *arg;
	void *res;
	int status;//0-created, 1-in pool, 2-running, 3-finished, 4-joined
	struct thread_task* next;
	bool detach;
	pthread_mutex_t mutex;
	pthread_cond_t cond_var;

};

struct thread_pool {
	pthread_t threads[TPOOL_MAX_THREADS];
	int max_count;
	
	int thread_count, task_count;
	struct thread_task* task_q;
	pthread_mutex_t mutex;
	pthread_cond_t  cond_var;
	
};

void* thread_func(void* args){
	while(true){
		struct thread_pool *pool = args;
		pthread_mutex_lock(&pool->mutex);
		while(pool->task_q == NULL)
			pthread_cond_wait(&pool->cond_var, &pool->mutex);
		struct thread_task* task = pool->task_q;
		pool->task_q = pool->task_q->next;
		pthread_mutex_lock(&task->mutex);
		task->status = 2;
		pthread_mutex_unlock(&task->mutex);
		
		pthread_mutex_lock(&task->mutex);
		task->res = task->function(task->arg);
		task->status = 3;
		pthread_mutex_unlock(&task->mutex);
		
		pthread_mutex_lock(&task->mutex);
		if (task->detach){
			pthread_mutex_unlock(&task->mutex);
			thread_task_delete(task);
		}
		pthread_mutex_unlock(&task->mutex);
		
		pthread_mutex_unlock(&pool->mutex);
		
	}
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {  
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	
	struct thread_pool *tp = (struct thread_pool* )malloc(sizeof(struct thread_pool));
	tp->max_count = max_thread_count;
	tp->thread_count = 0; tp->task_count = 0;
	tp->task_q = NULL;
	for (int i = 0; i < max_thread_count; ++i){
		tp->threads[i] = 0;
	}
	*pool = tp;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->thread_count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (pool->task_count > 0){ return TPOOL_ERR_HAS_TASKS; }
	
	pthread_mutex_lock(&pool->mutex);
	for (int i = 0; i < pool->thread_count; ++i)
		pthread_join(pool->threads[i], NULL);
	free(pool->task_q);
	pthread_mutex_destroy(&pool->mutex);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{	
	pthread_mutex_lock(&pool->mutex);
	if (pool->task_count >= TPOOL_MAX_TASKS) { 
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	pool->task_count++;
	//printf("\n1-%d\n", (task->next==NULL));
	if (pool->task_q == NULL){
		pool->task_q = task;
	}
	else{
		pool->task_q->next = task;
	}
	task->status = 1;
	
	if (pool->task_count > pool->thread_count && 
		pool->thread_count < pool->max_count){
		for (int i = 0; i < pool->max_count; ++i){
			if (pool->threads[i] == 0){
				pthread_create(&pool->threads[i], NULL,thread_func,pool);
				pool->thread_count++;
				break;
			}
		}
	}
	//printf("\n2-%d\n", (pool->task_q->next==NULL));
	pthread_mutex_unlock(&pool->mutex);
	
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{	
	struct thread_task *tt = (struct thread_task*)malloc(sizeof(struct thread_task));
	tt->function = function; tt->arg = arg;
	tt->status = 0;
	tt->next = NULL;
	tt->detach = false;
	*task = tt;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return (task->status == 3);
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return (task->status == 2);
}

int
thread_task_join(struct thread_task *task, void **result)
{
	pthread_mutex_lock(&task->mutex);
	if (task->status < 1) { 
		pthread_mutex_unlock(&task->mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	if (task->status != 4){
		while (task->status < 3){
			pthread_cond_wait(&task->cond_var, &task->mutex);
		}
		task->status = 4;
	}
	*result = task->res;
	pthread_mutex_unlock(&task->mutex);
	return 0;
}

int
thread_task_delete(struct thread_task *task)
{
	if (task->status != 0 & task->status < 4) {return TPOOL_ERR_TASK_IN_POOL; }
	
	pthread_mutex_lock(&task->mutex);
	free(task->next);
	pthread_mutex_destroy(&task->mutex);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	pthread_mutex_lock(&task->mutex);
	if (task->status < 1){ 
		pthread_mutex_unlock(&task->mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED; 
	}
	task->detach = true;
	
	if (task->status >= 3){
		pthread_mutex_unlock(&task->mutex);
		thread_task_delete(task);
	}
	pthread_mutex_unlock(&task->mutex);
	return 0;
}

#endif

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
