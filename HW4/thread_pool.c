#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>

struct thread_task {
	thread_task_f function;
	void *arg;
	void* rez;
	pthread_mutex_t t_mutex;
	int task_status = 0;//1-in pool, 2-joined, 3-running, 4-finished
	struct thread_task *next_task = NULL;
	pthread_cond_t cond_var;
	bool need_detach = false;
};

struct thread_pool {
	pthread_t *threads[TPOOL_MAX_THREADS];
	
	int max_thread_count;
	
	int thread_count = 0, task_count = 0;
	struct thread_task *task_q = NULL;
	pthread_mutex_t p_mutex;
	pthread_cond_t p_cond_var;
};

void thread_func(struct thread_pool *pool){
	while(1){
		pthread_mutex_lock(&pool->p_mutex);
		while(pool->task_q == NULL)
			pthread_cond_wait(&pool->p_cond_var, &pool->p_mutex);
		struct thread_task*ttask = pool->task_q;
		pool->task_q = pool->task_q->next_task;
		pthread_muthex_unlock(&pool->p_mutex);
		
		pthread_mutex_lock(&ttask->t_mutex);
		ttask->task_status = 3;
		pthread_mutex_unlock(&ttask->t_mutex);
		
		ttask->rez = ttask->function(task->arg);
		
		pthread_mutex_lock(&ttask->t_mutex);
		ttask->task_status = 4;
		pthread_mutex_unlock(&ttask->t_mutex);
		
		pthread_mutex_lock(&ttask->t_mutex);
		if (ttask->need_detach){
			pthread_cond_destroy(&ttask->cond_var);
			pthread_mutex_unlock(&ttask->t_mutex);
			pthread_mutex_destroy(&ttask->t_mutex);
			free(ttask);
		}
		pthread_mutex_unlock(&ttask->t_mutex);
	}
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{	
	if (max_thread_count <= 0 ||
	max_thread_count > TPOOL_MAX_THREADS) {  return TPOOL_ERR_INVALID_ARGUMENT;}
	
	struct thread_pool tp = (struct thread_pool*)malloc(sizeof(struct thread_pool));
	tp.max_thread_count = max_thread_count;
	for (int i = 0; i < max_thread_count; ++i)
		tp->threads[i] = NULL;
	
	*pool = *tp;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	if (pool == NULL) {return TPOOL_ERR_INVALID_ARGUMENT;}
	
	return pool->thread_count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (pool == NULL) {return TPOOL_ERR_INVALID_ARGUMENT;}
	
	pthread_mutex_lock(&pool->p_mutex);
	if (pool->task_count != 0) {
		pthread_mutex_unlock(&pool->p_mutex);
		return TPOOL_ERR_HAS_TASKS;
	}
	for (int i = 0; i < pool->thread_count; ++i)
		pthread_join(pool->threads[i], NULL);
	free(pool->threads);
	pthread_mutex_unlock(&pool->p_mutex);
	pthread_mutex_destroy(&pool->p_mutex);
	free(pool);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	pthread_mutex_lock(&pool->p_mutex);
	if (pool->task_count == TPOOL_MAX_TASKS) {
		pthread_mutex_unlock(&pool->p_mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	pool->task_count++;
	task->task_status = 1;
	if (pool->task_q == NULL){
		pool->task_q = task;
	}
	else{
		pool->task_q->next_task = task;
	}
	if (pool->thread_count <= pool->max_tread_count){
		int thread_rez = pthread_create(&pool->threads[pool->thread_count], NULL, thread_func, pool);
		pool->thread_count++;
	}
	pthread_mutex_unlock(&pool->p_mutex);
	
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	struct thread_task tt = (struct thread_task*)malloc(struct thread_task);
	tt.function = function;
	tt.arg = arg;
	
	*task = *tt;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return (task->task_status == 4);
}

bool
thread_task_is_running(const struct thread_task *task)
{
	return (task->task_status == 3);
}

int
thread_task_join(struct thread_task *task, void **result)
{
	pthread_mutex_lock(&task->t_mutex);
	if (task->task_status != 1) {
		pthread_mutex_unlock(&task->t_mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	if (task->task_status != 2){
		while(!(task->task_statuc == 3 || task->task_statuc == 4)){
			pthread_cond_wait(&task->cond_var, &task->t_mutex);
		}
		task->task_status = 2;
		*result -> task->rez;
	}
	pthread_mutex_unlock(&task->t_mutex);
	return 0;
}

int
thread_task_delete(struct thread_task *task)
{
	pthread_mutex_lock(&task->t_mutex);
	if (task->task_status == 1) {
		pthread_mutex_unlock(&task->t_mutex);
		return TPOOL_ERR_TASK_IN_POOL;
	}
	pthread_cond_destroy(&task->cond_var);
	pthread_mutex_unlock(&task->t_mutex);
	pthread_mutex_destroy(&task->t_mutex);
	free(task);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	pthread_mutex_lock(&task->t_mutex);
	if (task->task_status == 1) {
		pthread_mutex_unlock(&task->t_mutex);
		return TPOOL_ERR_TASK_IN_POOL;
	}
	task->need_detach = true;
	pthread_mutex_unlock(&task->t_mutex);
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

