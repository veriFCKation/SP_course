#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

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
	enum task_status status;
	
	pthread_mutex_t task_mut;
	
	bool needs_detach;
	
	pthread_cond_t task_cond;
	
	struct thread_task *next;
	struct thread_task *prev;
};

struct thread_pool {
	pthread_t *threads;
	bool *busy; //thread now process some task?
	int thread_count;
	int max_count;

	pthread_mutex_t pool_mut;

	struct thread_task *task_queue; //created
	struct thread_task *queue_tail;
	int queue_count;
	
	int process_count; //pushed+running
	
	bool works;
};

struct meta_info{
	struct thread_pool *pool;
	int thread_indx;
};


void *thread_func(void *args){
	struct meta_info *meta = args;
	struct thread_pool *pool = meta->pool;
	int thread_indx = meta->thread_indx;
	free(meta);
	while (pool->works){
		struct thread_task *task = NULL;
		
		pthread_mutex_lock(&pool->pool_mut);
			if (pool->task_queue != NULL){ //take from queue
				task = pool->task_queue;
				pool->task_queue = pool->task_queue->next;
				pool->queue_count--;
				if (pool->task_queue == NULL) {pool->queue_tail = NULL;}
				task->next = NULL;
				task->prev = NULL;
			}
			
			if (task != NULL){ //put to processed
				pool->process_count++;
				pool->busy[thread_indx] = true; //now process busy
			}
		pthread_mutex_unlock(&pool->pool_mut);
		if (task == NULL) {continue;} 
		if (pool->works == false) {break;}
		
		pthread_mutex_lock(&task->task_mut);
			task->status = RUNNING;
		pthread_mutex_unlock(&task->task_mut);
		
		task->rez = task->function(task->arg); //run function
		
		pthread_mutex_lock(&pool->pool_mut);
			pool->process_count--; // delete from processed
			pool->busy[thread_indx] = false;
		pthread_mutex_unlock(&pool->pool_mut);
		
		pthread_mutex_lock(&task->task_mut);
			task->status = FINISHED;
			//pthread_cond_signal(&task->task_cond);
			pthread_cond_broadcast(&task->task_cond);
			
			if (task->needs_detach) {
				pthread_mutex_unlock(&task->task_mut);
				
				pthread_mutex_destroy(&task->task_mut);
				pthread_cond_destroy(&task->task_cond);
				free(task);
				continue;
			}
		pthread_mutex_unlock(&task->task_mut);
	}
	return 0;
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if (pool == NULL) {return -1;}
	if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {return TPOOL_ERR_INVALID_ARGUMENT;}
	
	struct thread_pool *new_pool = (struct thread_pool*)malloc(sizeof(struct thread_pool));
	new_pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * TPOOL_MAX_THREADS);
	new_pool->busy = (bool*)malloc(sizeof(bool) * TPOOL_MAX_THREADS);
	new_pool->thread_count = 0;
	new_pool->max_count = max_thread_count;
	new_pool->works = true;
	
	pthread_mutex_init(&new_pool->pool_mut, NULL);
	
	new_pool->task_queue = NULL;
	new_pool->queue_tail = NULL;
	new_pool->queue_count = 0;
	
	new_pool->process_count = 0;
	
	*pool = new_pool;
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
	if (pool == NULL) {return 0;}
	
	bool flag = false;
	pthread_mutex_lock(&pool->pool_mut);
		if (pool->process_count > 0) {flag = true;}
		
		if (pool->queue_count > 0) {flag = true;}
		
		if (flag){
			pthread_mutex_unlock(&pool->pool_mut);
			return TPOOL_ERR_HAS_TASKS;
		}
		
		pool->works = false;
	pthread_mutex_unlock(&pool->pool_mut);
	
	int status;
	for (int i = 0; i < pool->thread_count; ++i){
		status = pthread_join(pool->threads[i], NULL);
		if (status != 0) { printf("Thread #%d still alive, better call 47\n", i);} 
	}

	pthread_mutex_destroy(&pool->pool_mut);
	free(pool->threads);
	free(pool->busy);
	free(pool);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	if (pool == NULL || task == NULL) {return -1;}
	
	pthread_mutex_lock(&pool->pool_mut);
		if (pool->queue_count + pool->process_count >= TPOOL_MAX_TASKS) {
			pthread_mutex_unlock(&pool->pool_mut);
			return TPOOL_ERR_TOO_MANY_TASKS;
		}
		
		pthread_mutex_lock(&task->task_mut);
			task->status = PUSHED;
		pthread_mutex_unlock(&task->task_mut);
		
		pthread_mutex_lock(&task->task_mut); //add task to queue
			pool->queue_count++;
			if (pool->task_queue == NULL){
				pool->task_queue = task;
				pool->queue_tail = task;
			}
			else{
				if (pool->queue_tail != NULL){
					pool->queue_tail->next = task;
					task->prev = pool->queue_tail;
					pool->queue_tail = task;
				}
				else{ pool->queue_tail = task; }
			}
		pthread_mutex_unlock(&task->task_mut);
		
		bool exist_free = false;
		for (int i = 0; i < pool->thread_count; ++i){
			if (! pool->busy[i]){
				exist_free = true;
				break;
			}
		}
		
		if (!exist_free && pool->thread_count < pool->max_count){
			struct meta_info *meta = (struct meta_info*)malloc(sizeof(struct meta_info));
			meta->pool = pool;
			meta->thread_indx = pool->thread_count;
			pthread_create(&pool->threads[pool->thread_count], NULL, thread_func, (void*)meta);
			
			pool->busy[pool->thread_count] = false;
			pool->thread_count++;
		}
	pthread_mutex_unlock(&pool->pool_mut);
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	if (task == NULL){ return -1;}
	
	struct thread_task *new_task = calloc(1, sizeof(struct thread_task));
					// (struct thread_task*)malloc(sizeof(struct thread_task));
	
	new_task->function = function;
	new_task->arg = arg;
	new_task->rez = NULL;
	new_task->status = CREATED;
	
	pthread_mutex_init(&new_task->task_mut,NULL);
	
	new_task->next = NULL;
	new_task->prev = NULL;
	
	new_task->needs_detach = false;
	
	pthread_cond_init(&new_task->task_cond,NULL);
	
	*task = new_task;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	if (task == NULL) {return false;}
	return (task->status == FINISHED);
}

bool
thread_task_is_running(const struct thread_task *task)
{
	if (task == NULL) {return false;}
	return (task->status == RUNNING);
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if (task == NULL && result == NULL) {return -1;}
	pthread_mutex_lock(&task->task_mut);
		if (task->status < PUSHED) {
			pthread_mutex_unlock(&task->task_mut);
			return TPOOL_ERR_TASK_NOT_PUSHED;
		}
		
		while (task->status < FINISHED) {pthread_cond_wait(&task->task_cond, &task->task_mut);}
		
		task->status = JOINED;
		*result = task->rez;
		
	pthread_mutex_unlock(&task->task_mut);
	return 0;
}

int
thread_task_delete(struct thread_task *task)
{
	if (task == NULL || task->needs_detach) {return 0;}
	
	pthread_mutex_lock(&task->task_mut);
		if (task->status != JOINED && task->status != CREATED) {
			pthread_mutex_unlock(&task->task_mut);
			return TPOOL_ERR_TASK_IN_POOL;
		}
		
		if (task->next != NULL) {task->next->prev = task->prev;}
		if (task->prev != NULL) {task->prev->next = task->next;}
	pthread_mutex_unlock(&task->task_mut);
	
	pthread_mutex_destroy(&task->task_mut);
	pthread_cond_destroy(&task->task_cond);
	free(task);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	if (task == NULL) {return 0;}
	
	pthread_mutex_lock(&task->task_mut);
		if (task->status < PUSHED) {
			pthread_mutex_unlock(&task->task_mut);
			return TPOOL_ERR_TASK_NOT_PUSHED;
		}
		task->needs_detach = true;
		
		if (task->status == FINISHED){
			pthread_mutex_unlock(&task->task_mut);
			
			pthread_mutex_destroy(&task->task_mut);
			pthread_cond_destroy(&task->task_cond);
			free(task);
			return 0;
		}
	pthread_mutex_unlock(&task->task_mut);
	return 0;
}

#endif
