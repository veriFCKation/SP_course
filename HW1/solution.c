#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "libcoro.h"

struct timespec time_count(struct timespec t1, struct timespec t2){
	long int sec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_sec - t1.tv_sec - 1) :
							(t2.tv_sec - t1.tv_sec);
	long int nsec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_nsec - t1.tv_nsec + 1000000000) : 
							(t2.tv_nsec - t1.tv_nsec);
	struct timespec rez;
	rez.tv_sec = sec; rez.tv_nsec = nsec;
	return rez;
}

void time_plus(struct timespec t1, struct timespec t2, struct timespec where){
	struct timespec diff = time_count(t1, t2);
	where.tv_sec = where.tv_sec + diff.tv_sec;
	where.tv_nsec = where.tv_nsec + diff.tv_nsec;
}

int read_file(int arr[], FILE* fptr){
	int i = 0;
	while (fscanf(fptr, "%d", &arr[i]) != EOF){
		i++;
	}
	return i;
}

int write_file(int arr[], int n, FILE* fptr){
	for (int i = 0; i < n; ++i){
		fprintf(fptr, "%d ", arr[i]);
	}
}

void merge(char* name_from, char* name_to){
	FILE* from = fopen(name_from, "r");
	FILE* to = fopen(name_to, "r");
	if (from == NULL || to == NULL){
		printf("file problem");
		return;
	}
	int* arr1 = malloc(40000 * sizeof(int));
	int* arr2 = malloc(40000 * sizeof(int));
	int* rez = malloc(80000 * sizeof(int));
	int i = 0, j = 0, k = 0, n, m;
	if (arr1 == NULL || arr2 == NULL || rez == NULL){
		printf("malloc problem");
		fclose(from); fclose(to);
		return;
	}
	n = read_file(arr1, from);
	m = read_file(arr2, to);
	
	while(i < n && j < m){
		if (arr1[i] <= arr2[j]){
			rez[k] = arr1[i];
			i++;
		}
		else{
			rez[k] = arr2[j];
			j++;
		}
		k++;
	}
	while(i < n){
		rez[k] = arr1[i];
		k++;i++;
	}
	while(j < m){
		rez[k] = arr1[j];
		k++;j++;
	}
	free(arr1);
	free(arr2);
	fclose(from);
	remove(name_from);
	fclose(to);
	to = fopen(name_to, "w");
	if (to == NULL){
		printf("reopen problem");
		return;
	}
	write_file(rez, k, to);
	free(rez);
	fclose(to);
}

void swap(int* a, int* b){
	int t = *a;
	*a = *b;
	*b = t;
}
int partition(int arr[], int low, int high){
	int pivot = arr[high];
	int i = (low - 1);
	for (int j = low; j <= high - 1; j++) {
		if (arr[j] < pivot) {
			i++;
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[high]);
	return (i + 1);
}
static void quickSort(int arr[], int low, int high){
	if (low < high) {
		int pi = partition(arr, low, high);
		quickSort(arr, low, pi - 1);
		//coro_yield();
		quickSort(arr, pi + 1, high);
		coro_yield();
	}
}

char* file_names[1000];
int file_num, curr_name = 0;
pthread_mutex_t mutex;
struct timespec coro_ts[20];


static int
coroutine_func_f(void *context)
{
	struct timespec t1, t2;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	struct coro *this = coro_this();
	int name = (int*)context;
	printf("Started coroutine coro_%d\n", name);
	printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
	printf("coro_%d: yield\n", name);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	time_plus(t1, t2, coro_ts[name]);
	coro_yield();
	clock_gettime(CLOCK_MONOTONIC, &t1);
	
	while (__atomic_fetch_add(&curr_name, 1,__ATOMIC_SEQ_CST) < file_num){
		pthread_mutex_lock(&mutex);
		if (curr_name >= file_num){ 
			//this->is_finished = true;
			clock_gettime(CLOCK_MONOTONIC, &t2);
			time_plus(t1, t2, coro_ts[name]); 
			break;
		}
		char* file_name = file_names[--curr_name];
		curr_name++;
		pthread_mutex_unlock(&mutex);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, coro_ts[name]);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		
		FILE* fptr = fopen(file_name, "r");
		int* arr = malloc(40000 * sizeof(int));
		int len = read_file(arr, fptr);
		fclose(fptr);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, coro_ts[name]);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		///
		quickSort(arr, 0, len-1);
		printf("coro_%d: switch count after other function %lld\n", name,
		       coro_switch_count(this));
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, coro_ts[name]);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		       
		fptr = fopen(file_name, "w");
		write_file(arr, len, fptr);
		fclose(fptr);
		free(arr);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, coro_ts[name]);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
	}
	return coro_status(this);
}

int
main(int argc, char** argv)
{
	struct timespec t1, t2, full_t;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	const int coroutine_num = atoi(argv[1]); //number of coroutines
	int arg_i = 2;
	file_num = argc - 2;
	while (arg_i != argc){
		file_names[arg_i-2] = argv[arg_i++];
	}
	coro_sched_init();
	/* Start several coroutines. */
	for (int i = 0; i < coroutine_num; ++i) {
		/*char coro_name[16];
		sprintf(coro_name, "coro_%d", i);*/
		coro_ts[i].tv_sec = 0; coro_ts[i].tv_nsec = 0;
		coro_new(coroutine_func_f, strdup(i));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		if (coro_is_finished(c)){
			printf("Finished with %d", coro_status(c));
			coro_delete(c);
		}
	}
	/* All coroutines have finished. */

	//merging
	for (int indx = 3; indx < argc; ++indx){
		merge(argv[indx-1], argv[indx]);
	}
	rename(argv[argc-1], "result.txt");
	clock_gettime(CLOCK_MONOTONIC, &t2);
	
	full_t = time_count(t1, t2);
	printf("Full time: sec: %ld nsec: %ld\n", full_t.tv_sec, full_t.tv_nsec);
	for (int i = 0; i < coroutine_num; ++i) {
		printf("coro_%d: sec: %ld nsec: %ld\n", i, coro_ts[i].tv_sec, coro_ts[i].tv_nsec);
	}
	
	return 0;
}
