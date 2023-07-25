#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

void *f(){}

int main(){
  pthread_t thread;

  int N = 100000;
  struct timespec t1, t2;


  clock_gettime(CLOCK_MONOTONIC, &t1);
  for (int i = 0; i < N; ++i){
  	pthread_create(&thread, NULL, &f, NULL);
  	pthread_join(thread, NULL);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
  
  long int sec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_sec - t1.tv_sec - 1) : (t2.tv_sec - t1.tv_sec);
  long int nsec = (t2.tv_nsec - t1.tv_nsec) < 0 ? (t2.tv_nsec - t1.tv_nsec + 1000000000) : (t2.tv_nsec - t1.tv_nsec);
  
  printf("sec: %ld\nnsec: %ld\n", sec, nsec);
  printf("time per one (nsec): %f\n", (sec*1000000000.0 + nsec) / N);

  return 0;
}
