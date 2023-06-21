#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcoro.h"
//still get segmentation fault
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
		coro_yield();
		quickSort(arr, pi + 1, high);
		coro_yield();
	}
}

static int
coroutine_func_f(void *context)
{
	struct coro *this = coro_this();
	char *name = context;
	printf("Started coroutine %s\n", name);
	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	printf("%s: yield\n", name);
	coro_yield();
	
	FILE* fptr = fopen(name, "r");
	int arr[20000];
	int i = 0;
	while (!feof(fptr)){
		fscanf(fptr, "%d", &arr[i]);
		i++;
	}
	fclose(fptr);
	
	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	printf("%s: yield\n", name);
	coro_yield();

	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	quickSort(arr, 0, i - 1);
	printf("%s: switch count after other function %lld\n", name, coro_switch_count(this));
	
	fptr = fopen(name, "w");
	for (int k = 0; k < i; ++k){
		fprintf(fptr, "%d ", arr[k]);
	}
	fclose(fptr);
	
	int stat = coro_status(this);
	free(name);
	return stat;
}

int
main(int argc, char **argv)
{
	/* Delete these suppressions when start using the args. */
	(void)argc;
	(void)argv;
	
	coro_sched_init();
	for (int i = 0; i < 3; ++i) {
		char file_name[32];
		sprintf(file_name, "test_%d", i);
		
		coro_new(coroutine_func_f, strdup(file_name));
	}
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	
	FILE* fptrs[3];
	for(int i = 0; i < 3; ++i){
		char file_name[32];
		sprintf(file_name, "test_%d", i);
		fptrs[i] = fopen(file_name, "r");
	}
	
	bool done = false;
	int arr[3];
	for (int i = 0; i < 3; ++i){
		if (!feof(fptrs[i])){
			fscanf(fptrs[i], "%d", &arr[i]);
		}
		else{
			arr[i] = -1;
		}
	}
	FILE* rez_file = fopen("rezult.txt", "w");
	while (!done){
		int min_num = -1;
		for (int i = 0; i < 3; ++i){
			if (arr[i] != -1){
				if ((min_num ==-1) || (arr[min_num] > arr[i]))
					min_num = i;
			}
		}
		if (min_num != -1){
			fprintf(rez_file, "%d ", arr[min_num]);
			if (!feof(fptrs[min_num])){
				fscanf(fptrs[min_num], "%d", &arr[min_num]);
			}
			else{
				arr[min_num] = -1;
			}
			continue;
		}
		done = true;
	}
	
	for(int i = 0; i < 3; ++i){
		remove(fptrs[i]);
	}
	fclose(rez_file);
	return 0;
}
