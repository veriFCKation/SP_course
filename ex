#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

pthread_t thread[3];
pthread_mutex_t mutex;
int tN = 3;
long int counter = 0, cN = 10000000;


void *lock_func(){
	for (int i = 0; i < cN; ++i){
		pthread_mutex_lock(&mutex);
		counter = counter + 1;
		pthread_mutex_unlock(&mutex);
	}
}

int main(){
  
  struct timespec t1, t2;
  
  clock_gettime(CLOCK_MONOTONIC, &t1);
  for (int i = 0; i < tN; ++i){
  	pthread_create(&thread[i], NULL, *lock_func, NULL);
  	pthread_join(thread[i], NULL);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
  
  long int sec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_sec - t1.tv_sec - 1) : (t2.tv_sec - t1.tv_sec);
  long int nsec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_nsec - t1.tv_nsec + 1000000000) : (t2.tv_nsec - t1.tv_nsec);
  
  printf("sec: %ld\nnsec: %ld\n", sec, nsec);
  printf("time per one (nsec): %f\n", (sec*1000000000.0 + nsec) / cN);
  printf("thread #: %d\ncounter: %ld\n", tN, counter);

  return 0;
}
