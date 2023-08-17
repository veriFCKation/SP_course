#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
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

void time_add(struct timespec t, struct timespec* where){
	where->tv_sec = where->tv_sec + t.tv_sec;
	where->tv_nsec = where->tv_nsec + t.tv_nsec;
}

void time_plus(struct timespec t1, struct timespec t2, struct timespec* where){
	struct timespec diff = time_count(t1, t2);
	where->tv_sec = where->tv_sec + diff.tv_sec;
	where->tv_nsec = where->tv_nsec + diff.tv_nsec;
}

long int count_file(char* name){
	FILE* fptr = fopen(name, "r");
	long int i = 0;
	int buf;
	while (fscanf(fptr, "%d", &buf) != EOF){
		i++;
	}
	fclose(fptr);
	return i;
}

int read_file(int arr[], FILE* fptr){
	int i = 0;
	while (fscanf(fptr, "%d", &arr[i]) != EOF){
		i++;
	}
	return i;
}

void write_file(int arr[], int n, FILE* fptr){
	if (fptr == NULL || arr == NULL)
		printf("write_file prob\n");
	for (int i = 0; i < n; ++i){
		fprintf(fptr, "%d ", arr[i]);
	}
}

void merge(char* name_from, char* name_to){
	FILE* from = fopen(name_from, "r");
	FILE* to = fopen(name_to, "r");
	if (from == NULL || to == NULL){
		printf("file problem\n");
		return;
	}
	int i = 0, j = 0, k = 0;
	long int n, m, l;
	n = count_file(name_from);
	m = count_file(name_to);
	l = n + m;
	int* arr1 = malloc(n* sizeof(int));
	int* arr2 = malloc(m* sizeof(int));
	int* rez = malloc(l * sizeof(int));
	read_file(arr1, from);
	read_file(arr2, to);
	if (arr1 == NULL || arr2 == NULL || rez == NULL){
		printf("malloc problem\n");
		fclose(from); fclose(to);
		return;
	}
	
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
		rez[k] = arr2[j];
		k++;j++;
	}
	free(arr1);
	free(arr2);
	fclose(from);
	fclose(to);
	to = fopen(name_to, "w");
	if (to == NULL){
		printf("reopen problem\n");
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
struct timespec quickSort(int arr[], int low, int high){
	struct timespec t1, t2;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	if (low < high) {
		int pi = partition(arr, low, high);
		quickSort(arr, low, pi - 1);
		//coro_yield();
		quickSort(arr, pi + 1, high);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		coro_yield();
	}
	else{
		clock_gettime(CLOCK_MONOTONIC, &t2);
	}
	return time_count(t1, t2);
}
char* file_names[1000];
int file_num, curr_name = 0;


static int
coroutine_func_f(void *context)
{
	struct timespec t1, t2, full_time;
	int time_rez = clock_gettime(CLOCK_MONOTONIC, &t1);
	if (time_rez != 0)
		printf("time prob");
	struct coro *this = coro_this();
	char* str = context;
	int name;
	if (sscanf(str, "%d", &name) == 0)
		printf("sscanf problem\n");
	
	printf("Started coroutine coro_%d\n", name);
	printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
	printf("coro_%d: yield\n", name);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	time_plus(t1, t2, &full_time);
	coro_yield();
	clock_gettime(CLOCK_MONOTONIC, &t1);
	
	while (curr_name < file_num){
		char* file_name = file_names[curr_name];
		curr_name++;
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, &full_time);
		coro_yield();
		
		clock_gettime(CLOCK_MONOTONIC, &t1);
		FILE* fptr = fopen(file_name, "r");
		if (fptr == NULL)
			printf("open prob\n");
		
		long int len = count_file(file_name);
		int* arr = malloc(len * sizeof(int));
		read_file(arr, fptr);
		
		fclose(fptr);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, &full_time);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, &full_time);
		
		struct timespec qt = quickSort(arr, 0, len-1);
		time_add(qt, &full_time);
		
		clock_gettime(CLOCK_MONOTONIC, &t1);
		printf("coro_%d: switch count after other function %lld\n", name,
		       coro_switch_count(this));
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, &full_time);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		       
		fptr = fopen(file_name, "w");
		if (fptr == NULL)
			printf("reopen prob\n");
		write_file(arr, len, fptr);
		fclose(fptr);
		free(arr);
		
		if (curr_name >= file_num){
			break;
		}
		
		printf("coro_%d: switch count %lld\n", name, coro_switch_count(this));
		printf("coro_%d: yield\n", name);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		time_plus(t1, t2, &full_time);
		coro_yield();
		clock_gettime(CLOCK_MONOTONIC, &t1);
		curr_name++;
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	time_plus(t1, t2, &full_time);
	printf("coro_%d: full time of work is %ld sec %f msec, %lld switches\n", name, full_time.tv_sec, (full_time.tv_nsec / 1000000.0), coro_switch_count(this));
	return coro_status(this);
}

int
main(int argc, char** argv)
{
	struct timespec t1, t2, full_t;
	clock_gettime(CLOCK_MONOTONIC, &t1);

	int coroutine_num = atoi(argv[1]); //number of coroutines
	int diff = 2;
	if (coroutine_num == 0){
		coroutine_num = 3;
		diff = 1;
	}
	int arg_i = diff;
	file_num = argc - diff;
	while (arg_i != argc){
		file_names[arg_i-diff] = argv[arg_i];
		arg_i++;
	}
	coro_sched_init();
	
	/* Start several coroutines. */
	for (int i = 0; i < coroutine_num; ++i) {
		char coro_name[16];
		sprintf(coro_name, "%d_core", i);
		coro_new(coroutine_func_f, strdup(coro_name));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		if (coro_is_finished(c)){
			printf("Finished\n");
			coro_delete(c);
		}
	}
	/* All coroutines have finished. */

	//merging
	FILE* rez_file = fopen("rezult.txt", "w");
	fclose(rez_file);
	for (int indx = 2; indx < argc; ++indx){
		merge(argv[indx], "rezult.txt");
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	
	full_t = time_count(t1, t2);
	printf("Full time: sec: %ld msec: %f\n", full_t.tv_sec, full_t.tv_nsec / 1000000.0);
	
	return 0;
}
