// A simple producer/consumer program using semaphores and threads
//
// Adapted from Greg Andrews' textbook website
// http://www.cs.arizona.edu/people/greg/mpdbook/programs/
//

// compile: gcc -lpthread prodcom.c -o prodcom
// run:     ./prodcom numIters  (eg ./prodcom 20)

 
#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <cstdlib>
#include <atomic>
#define SHARED 1

void *Producer(void *);  // the two threads
void *Consumer(void *);

sem_t empty, full;       // the global semaphores
int data;                // shared buffer
int numIters;

// main() :    read command line and create threads, then
//             print result when the threads have quit

std::atomic <int> A;
std::atomic<int> B;
std::atomic<int> C;

int main(int argc, char *argv[]) {
  // thread ids and attributes
  pthread_t pid, cid;  
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  numIters = atoi(argv[1]);
  sem_init(&empty, SHARED, 1);  // sem empty = 1
  sem_init(&full, SHARED, 0);   // sem full = 0 

  printf("running for %d iterations\n",numIters);
  printf("main started\n");
  pthread_create(&pid, &attr, Producer, NULL);
  pthread_create(&cid, &attr, Consumer, NULL);
  pthread_join(pid, NULL);
  pthread_join(cid, NULL);
  printf("main done\n");
}

// deposit 1, ..., numIters into the data buffer
void *Producer(void *arg) {
  int produced;
  printf("Producer created\n");
  for (produced = 1; produced <= numIters; produced++) {
    
    int expected = 0;
    while (!A.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
    }     
     
     data = produced;
     printf("Producer \t Value \t %d  Data \t %d \n", produced, data);
    // Unlock A
    A.store(0, std::memory_order_release);
    
    //sem_wait(&empty);
    //data = produced;
    //sem_post(&full);
  }
}

// fetch numIters items from the buffer and sum them
void *Consumer(void *arg) {
  int total = 0, consumed;
  printf("Consumer created\n");
  for (consumed = 0; consumed < numIters; consumed++) {
    
    int expected = 0;
    while (!A.compare_exchange_weak(expected, 1, std::memory_order_acquire)) {
        expected = 0;
    }
     
     
     total = total+data;
     printf("Consumer \t Total \t %d Data \t %d \n", total, data);
    // Unlock A
    A.store(0, std::memory_order_release);

    //sem_wait(&full);
    //total = total+data;
    //sem_post(&empty);
  }
  printf("after %d iterations, the total is %d (should be %d)\n", numIters, total, numIters*(numIters+
1)/2);
}

//https://stackoverflow.com/questions/19774778/when-is-it-necessary-to-use-use-the-flag-stdlib-libstdc


//How to put barriers in x86 progrm.
//#C++ x86 Fences
//https://preshing.com/20130922/acquire-and-release-fences/