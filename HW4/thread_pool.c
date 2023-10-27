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
	
	struct thread_task *next;
	struct thread_task *prev;
	
	bool needs_detach;
	
	pthread_mutex_t cond_mut; //for finish signal
	pthread_cond_t task_cond;
};

struct thread_pool {
	pthread_t *threads;
	bool *busy; //thread now process some task?
	int thread_count;
	int max_count;

	pthread_mutex_t pool_mut;

	struct thread_task *task_queue; //created
	struct thread_task *queue_tail;
	
/*	struct thread_task *process_list; 
	struct thread_task *process_tail;
*/	int process_count; //pushed+running
	
	struct thread_task *finish_list; //finished+joined
	struct thread_task *finish_tail;
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
	while (true){
		printf(">>#%d came to start\n", thread_indx);
		struct thread_task *task = NULL;
		pthread_mutex_lock(&pool->pool_mut);
			if (pool->task_queue != NULL){ //taske from queue
				task = pool->task_queue;
				pool->task_queue = pool->task_queue->next;
				if (pool->task_queue = NULL) {pool->queue_tail = NULL;}
			}
			
			if (task != NULL){ //put to processed
				/*pthread_mutex_lock(&task->task_mut);
					task->next == NULL;
					if (pool->process_tail != NULL){
						pool->process_tail->next = task;
						task->prev = pool->process_tail;
						pool->process_tail = task;
					}
					else{
						task->prev = NULL;
						pool->process_list = task;
						pool->process_tail = task;
					}
				pthread_mutex_unlock(&task->task_mut);*/
				
				pool->process_count++;
				printf(">>#%d process_count + 1\n", thread_indx);
				
				pool->busy[thread_indx] = true; //now process busy
			}
		pthread_mutex_unlock(&pool->pool_mut);
		if (task == NULL) {continue;}
		printf(">>#%d process task\n", thread_indx);
		pthread_mutex_lock(&task->task_mut);
			task->status = RUNNING;
		pthread_mutex_unlock(&task->task_mut);
		
		pthread_mutex_lock(&task->task_mut);
			
			task->rez = task->function(task->arg); //run function
		
			task->status = FINISHED;
			//pthread_cond_signal(&task->task_cond);
			printf(">>#%d send signal\n", thread_indx);
			pthread_cond_broadcast(&task->task_cond);
			
			pthread_mutex_lock(&pool->pool_mut);
				pool->process_count--;
				printf(">>#%d process_count - 1\n", thread_indx);
				pool->busy[thread_indx] = false;
				
				task->next = NULL;
				if (pool->finish_tail != NULL){
					pool->finish_tail->next = task;
					task->prev = pool->finish_tail;
					pool->finish_tail = task;
				}
				else{
					task->prev = NULL;
					pool->finish_list = task;
					pool->finish_tail = task;
				}
				printf(">>#%d add task to finish\n", thread_indx);
			pthread_mutex_unlock(&pool->pool_mut);
		pthread_mutex_unlock(&task->task_mut);
		
/*		pthread_mutex_lock(&pool->pool_mut);
			pool->process_count--;
			printf(">>#%d process_count - 1\n", thread_indx);
			pool->busy[thread_indx] = false;
			
			/*pthread_mutex_lock(&task->task_mut); //delete from processed
				if (task->next != NULL){ task->next->prev = task->prev;}
				if (task->prev != NULL) {task->prev->next = task->next;}
				if (task == pool->process_list){
					pool->process_list = NULL;
					pool->process_tail = NULL;
				}
			pthread_mutex_unlock(&task->task_mut);*/
			
/*			pthread_mutex_lock(&task->task_mut);
				task->next = NULL;
				if (pool->finish_tail != NULL){
					pool->finish_tail->next = task;
					task->prev = pool->finish_tail;
					pool->finish_tail = task;
				}
				else{
					task->prev = NULL;
					pool->finish_list = task;
					pool->finish_tail = task;
				}
			pthread_mutex_unlock(&task->task_mut);
			printf(">>#%d add task to finish\n", thread_indx);
		pthread_mutex_unlock(&pool->pool_mut);*/
		printf(">>#%d release mutex\n", thread_indx);
		pthread_mutex_lock(&task->task_mut); //check on detach
			if (task->needs_detach){
				pthread_mutex_unlock(&task->task_mut);
				thread_task_delete(task);
				continue;
			}
		pthread_mutex_unlock(&task->task_mut);
		printf(">>#%d came to end\n", thread_indx);
	}
}


int count_just_task(struct thread_pool *pool){ //count task in queue
	int count = 0;
	pthread_mutex_lock(&pool->pool_mut);
		struct thread_task *curr = pool->task_queue;
		while (curr != NULL){
			count++;
			curr = curr->next;
		}
	pthread_mutex_unlock(&pool->pool_mut);
	return count;
}

int count_unjoined(struct thread_pool *pool){ //count already processed task
	int count = 0;
	pthread_mutex_lock(&pool->pool_mut);
		struct thread_task *curr = pool->finish_list;
		while (curr != NULL){
			if (curr->status != JOINED) {count++;}
			curr = curr->next;
		}
	pthread_mutex_unlock(&pool->pool_mut);
	return count;
}

int count_all_tasks(struct thread_pool *pool){
	int count = count_unjoined(pool);
	printf("====unjoined = %d\n", count);
	count = count + count_just_task(pool);
	printf("====queue + unj = %d\n", count);
	pthread_mutex_lock(&pool->pool_mut);
		count = count + pool->process_count;
	pthread_mutex_unlock(&pool->pool_mut);
	printf("====queue+unj + proces = %d\n", count);
	return count;
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
	
	pthread_mutex_init(&new_pool->pool_mut, NULL);
	
	new_pool->task_queue = NULL;
	new_pool->queue_tail = NULL;
	
/*	new_pool->process_list = NULL;
	new_pool->process_tail = NULL;
*/	new_pool->process_count = 0;
	
	new_pool->finish_list = NULL;
	new_pool->finish_tail = NULL;
	
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
	if (pool == NULL) {return -1;}
	
	if (count_all_tasks(pool) > 0) {return TPOOL_ERR_HAS_TASKS;}
	
	pthread_mutex_lock(&pool->pool_mut);
		int status;
		for (int i = 0; i < pool->thread_count; ++i){
			status = pthread_cancel(pool->threads[i]);
			if (status != 0) { printf("Thread #%d still alive, better call 47\n", i);} 
		}
	pthread_mutex_unlock(&pool->pool_mut);
	
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
	if (count_all_tasks(pool) >= TPOOL_MAX_TASKS) {return TPOOL_ERR_TOO_MANY_TASKS;}
	printf("before push-> %d tasks\n", count_all_tasks(pool));
	pthread_mutex_lock(&pool->pool_mut);
		printf("b1\n");
		pthread_mutex_lock(&task->task_mut);
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
			printf("pushed\n");
		pthread_mutex_unlock(&task->task_mut);
		
		printf("b2\n");
		bool exist_free = false;
		for (int i = 0; i < pool->thread_count; ++i){
			if (! pool->busy[i]){
				exist_free = true;
				break;
			}
		}
		if (!exist_free && pool->thread_count < pool->max_count){
			printf("create new thread for push\n");
			pool->busy[pool->thread_count] = false;
			struct meta_info *meta = (struct meta_info*)malloc(sizeof(struct meta_info));
			meta->pool = pool;
			meta->thread_indx = pool->thread_count;
			pthread_create(&pool->threads[pool->thread_count], NULL, thread_func, (void*)meta);
			pool->thread_count++;
		}
		printf("b3\n");
	pthread_mutex_unlock(&pool->pool_mut);
	printf("after push-> %d tasks\n", count_all_tasks(pool));
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	if (task == NULL){ return -1;}
	
	struct thread_task *new_task = (struct thread_task*)malloc(sizeof(struct thread_task));
	
	new_task->function = function;
	new_task->arg = arg;
	new_task->rez = NULL;
	new_task->status = CREATED;
	
	pthread_mutex_init(&new_task->task_mut,NULL);
	
	new_task->next = NULL;
	new_task->prev = NULL;
	
	new_task->needs_detach = false;
	
	pthread_mutex_init(&new_task->cond_mut,NULL); //for finish signal
	pthread_cond_init(&new_task->task_cond,NULL);
	
	*task = new_task;
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	bool res = false;
	pthread_mutex_lock((pthread_mutex_t*)&task->task_mut);
		if (task->status == FINISHED) {res = true;}
	pthread_mutex_unlock((pthread_mutex_t*)&task->task_mut);
	return res;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	bool res = false;
	pthread_mutex_lock((pthread_mutex_t*)&task->task_mut);
		if (task->status == RUNNING) {res = true;}
	pthread_mutex_unlock((pthread_mutex_t*)&task->task_mut);
	return res;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if (task == NULL) {return -1;}
	printf("1\n");
	if (task->status < PUSHED) {return TPOOL_ERR_TASK_NOT_PUSHED;}
	printf("2\n");
	if (task->status == FINISHED){
		pthread_mutex_lock(&task->task_mut);
			task->status = JOINED;
			*result = task->rez;
		pthread_mutex_unlock(&task->task_mut);
	}
	printf("3\n");
	int wait = pthread_cond_wait(&task->task_cond, &task->cond_mut);
	printf("4 %d\n", wait);
	pthread_mutex_lock(&task->task_mut);
		task->status = JOINED;
		*result = task->rez;
	pthread_mutex_unlock(&task->task_mut);
	return 0;
}

int
thread_task_delete(struct thread_task *task)
{
	if (task == NULL) {return -1;}
	if (task->status != JOINED && task->status != CREATED) {return TPOOL_ERR_TASK_IN_POOL;}
	
	pthread_mutex_lock(&task->task_mut); //delete this task from (any) list
		if (task->next != NULL) {task->next->prev = task->prev;}
		if (task->prev != NULL) {task->prev->next = task->next;}
	pthread_mutex_unlock(&task->task_mut);
	
	pthread_mutex_destroy(&task->task_mut);
	pthread_mutex_destroy(&task->cond_mut);
	pthread_cond_destroy(&task->task_cond);
	free(task);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
