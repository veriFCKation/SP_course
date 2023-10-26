#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>

enum task_status{
	CREATED = 1,
	PUSHED = 2,
	RUNNING = 3,
	FINISHED = 4,
	JOINED = 5,
};

struct thread_task {
	thread_task_f function;
	void *arg;
	void *rez;

	/* PUT HERE OTHER MEMBERS */
	struct thread_task *next;
	struct thread_task *prev;
	
	enum task_status status;
	
	pthread_mutex_t task_mut;
	pthread_mutex_t cond_mut;
	pthread_cond_t task_cond;
	
	bool needs_detach;
};

struct thread_pool {
	pthread_t *threads;
	int thread_count;
	int max_count;

	/* PUT HERE OTHER MEMBERS */
	struct thread_task *task_queue;
	struct thread_task *queue_tail;
	int task_count;
	
	pthread_mutex_t pool_mut;
	int *work;
};

void *thread_func(void * args){
	struct thread_pool *pool = args;
	while (true){
		struct thread_task *task = NULL;
		pthread_mutex_lock(&pool->pool_mut);
			if (pool->task_queue != NULL){
				task = pool->task_queue;
				pool->task_queue = pool->task_queue->next;
				pool->task_count--;
				if (pool->task_queue == NULL){ pool->queue_tail = NULL;}
				if (pool->task_count <= 0) {
					pool->task_queue = NULL;
					pool->queue_tail = NULL;
				}
				pthread_mutex_lock(&task->task_mut);
					task->status = RUNNING;
				pthread_mutex_unlock(&task->task_mut);
//				printf("- tc = %d\n", pool->task_count);
			}
		pthread_mutex_unlock(&pool->pool_mut);
		if (task == NULL) {continue;}
		
		task->rez =  task->function(task->arg);
		
		pthread_mutex_lock(&task->task_mut);
			task->status = FINISHED;
			pthread_cond_signal(&task->task_cond);
		pthread_mutex_unlock(&task->task_mut);
		
		pthread_mutex_lock(&task->task_mut);
			if (task->needs_detach){
				pthread_mutex_unlock(&task->task_mut);
				thread_task_delete(task);
				continue;
			}
		pthread_mutex_unlock(&task->task_mut);
	}
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS || pool == NULL){
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	struct thread_pool *new_pool = (struct thread_pool*)malloc(sizeof(struct thread_pool));
	new_pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * TPOOL_MAX_THREADS);
	new_pool->task_queue = NULL;
	new_pool->queue_tail = NULL;
	new_pool->thread_count = 0;
	new_pool->max_count = max_thread_count;
	new_pool->task_count = 0;
	int good = 1;
	new_pool->work = &good;
	pthread_mutex_init(&new_pool->pool_mut, NULL);
	
	*pool = new_pool;
	return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	if (pool == NULL) {return -1;}
	return pool->thread_count;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (pool == NULL) {return 0;}
	pthread_mutex_lock(&pool->pool_mut);
		if (pool->task_count > 0) {
			pthread_mutex_unlock(&pool->pool_mut);
			return TPOOL_ERR_HAS_TASKS;
		}
//		printf("task_count = %d\n", pool->task_count);
		int bad = 0;
		pool->work = &bad;
	pthread_mutex_unlock(&pool->pool_mut);
	int val;
	for (int i = 0; i < pool->thread_count; ++i){
//		printf("s");
		int status = pthread_cancel(pool->threads[i]);
//		printf(" = %d\n", status);
		//int status = pthread_join(pool->threads[i], (void **)&val);
	}
	pthread_mutex_destroy(&pool->pool_mut);
	free(pool->threads);
	free(pool);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	if (pool == NULL || task == NULL) {return -1;}
	if (task->status > PUSHED && task->status < JOINED) {return -1;}
	if (pool->task_count == TPOOL_MAX_TASKS) {return TPOOL_ERR_TOO_MANY_TASKS;}
	pthread_mutex_lock(&pool->pool_mut);
		pool->task_count++;
		task->status = PUSHED;
		if (pool->queue_tail != NULL){
			pool->queue_tail->next = task;
			task->prev = pool->queue_tail;
			pool->queue_tail = task;
		}
		else{
			pool->task_queue = task;
			pool->queue_tail = task;
		}
	pthread_mutex_unlock(&pool->pool_mut);
	usleep(100); //give task chance to be picked
	pthread_mutex_lock(&task->task_mut);
//		printf("task->status %d\n", task->status);
		if (task->status < RUNNING){
			pthread_mutex_lock(&pool->pool_mut);
				if (pool->thread_count < pool->max_count){
//					printf("----%d create new thread\n", pool->thread_count);
					int indx = pool->thread_count;
					pthread_create(&pool->threads[indx], NULL, thread_func, (void *)pool);
					pool->thread_count++;
				}
			pthread_mutex_unlock(&pool->pool_mut);
		}
	pthread_mutex_unlock(&task->task_mut);
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	struct thread_task *new_task = (struct thread_task*)malloc(sizeof(struct thread_task));
	new_task->function = function;
	new_task->arg = arg;
	new_task->prev = NULL;
	new_task->next = NULL;
	new_task->needs_detach = false;
	new_task->status = CREATED;
	pthread_mutex_init(&new_task->task_mut, NULL);
	pthread_mutex_init(&new_task->cond_mut, NULL);
	pthread_cond_init(&new_task->task_cond, NULL);
	
	*task = new_task;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	bool rezult;
	pthread_mutex_lock((pthread_mutex_t *)&task->task_mut);
		rezult = (task->status == FINISHED);
	pthread_mutex_unlock((pthread_mutex_t *)&task->task_mut);
	return rezult;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	bool rezult;
	pthread_mutex_lock((pthread_mutex_t *)&task->task_mut);
		rezult = (task->status == RUNNING);
	pthread_mutex_unlock((pthread_mutex_t *)&task->task_mut);
	return rezult;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if (task == NULL) {return -1;}
	if (task->status < PUSHED || task->status == JOINED) {return TPOOL_ERR_TASK_NOT_PUSHED; }
	if (thread_task_is_finished(task)){
		*result = task->rez;
		task->status = JOINED;
		return 0;
	}
	int wait = pthread_cond_wait(&task->task_cond, &task->cond_mut);
	*result = task->rez;
	task->status = JOINED;
	return 0;
}



int
thread_task_delete(struct thread_task *task)
{
	if (task == NULL) {return -1;}
	if (task->status >= PUSHED && task->status != JOINED) {return TPOOL_ERR_TASK_IN_POOL;}
	
	pthread_mutex_destroy(&task->task_mut);
	pthread_mutex_destroy(&task->cond_mut);
	pthread_cond_destroy(&task->task_cond);
	return 0;
}


#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	if (task == NULL) {return -1;}
	if (task->status < PUSHED) {return TPOOL_ERR_TASK_NOT_PUSHED;}
	pthread_mutex_lock(&task->task_mut);
		if (task->status >= FINISHED){
			pthread_mutex_unlock(&task->task_mut);
			return thread_task_delete(task);
		}
		task->needs_detach = true;
	pthread_mutex_unlock(&task->task_mut);
	return 0;
}

#endif

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
