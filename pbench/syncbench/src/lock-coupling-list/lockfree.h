#include <pthread.h>
// /#include <atomic.h>

#define AO_t size_t
#define DIMP_INLINE static inline

#  define NL                             1
#  define EL                             0
#  define TX_START(type)                 /* nothing */
#  define TX_LOAD(addr)                  (*(addr))
#  define TX_STORE(addr, val)            (*(addr) = (val))
#  define TX_END                         /* nothing */
#  define FREE(addr, size)               free(addr)
#  define MALLOC(size)                   malloc(size)
#  define TM_CALLABLE                    /* nothing */
#  define TM_ARGDECL_ALONE               /* nothing */
#  define TM_ARGDECL                     /* nothing */
#  define TM_ARG                         /* nothing */
#  define TM_ARG_ALONE                   /* nothing */
#  define TM_ARG_LAST                    /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_STARTUP()                   /* nothing */
#  define TM_SHUTDOWN()                  /* nothing */
#  define TM_THREAD_ENTER()              /* nothing */
#  define TM_THREAD_EXIT()               /* nothing */

#define DIMP_nop_full()     __asm__ __volatile__("mfence" : : : "memory");    


typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;

static inline
void barrier_init(barrier_t *b, int n)
{
	pthread_cond_init(&b->complete, NULL);
	pthread_mutex_init(&b->mutex, NULL);
	b->count = n;
	b->crossing = 0;
}

static inline
void barrier_cross(barrier_t *b)
{
	pthread_mutex_lock(&b->mutex);
	/* One more thread through */
	b->crossing++;
	/* If not all here, wait */
	if (b->crossing < b->count) {
		pthread_cond_wait(&b->complete, &b->mutex);
	} else {
		pthread_cond_broadcast(&b->complete);
		/* Reset for next time */
		b->crossing = 0;
	}
	pthread_mutex_unlock(&b->mutex);
}



/* Returns nonzero if the comparison succeeded. */
DIMP_INLINE int 
DIMP_compare_and_swap_full(volatile AO_t *addr, AO_t old, AO_t new_val)
{  
 // printf("atomic\n");
# ifdef AO_USE_SYNC_CAS_BUILTIN
  __asm__ __volatile__("sfence" : : : "memory");
  __asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
    char result;
    __asm__ __volatile__("sfence" : : : "memory");
    __asm__ __volatile__("lfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchgq %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    //__asm__ __volatile__("sfence" : : : "memory");
    return (int) result;
# endif
}


//Acquire RMW Acquire
DIMP_INLINE int
DIMP_compare_and_swap_acq(volatile AO_t *addr, AO_t old, AO_t new_val)
{

# ifdef AO_USE_SYNC_CAS_BUILTIN
  printf("built in acquire \n");
  //__asm__ __volatile__("sfence" : : : "memory");
  __asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
    printf("not a built in acquire \n");
     char result;
    __asm__ __volatile__("lfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchg %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    // __asm__ __volatile__("mfence" : : : "memory");
     return (int) result;
#endif
}
//https://stackoverflow.com/questions/51598339/achieve-gcc-cas-function-for-version-4-1-2-and-earlier/51632252#51632252
//RMW Release
DIMP_INLINE int
DIMP_compare_and_swap_rel(volatile AO_t *addr, AO_t old, AO_t new_val)
{
 # ifdef AO_USE_SYNC_CAS_BUILTIN
  __asm__ __volatile__("sfence" : : : "memory");
  //__asm__ __volatile__("lfence" : : : "memory");
    return (int)__sync_bool_compare_and_swap(addr, old, new_val);
   // __asm__ __volatile__("sfence" : : : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
# else
    char result;
    __asm__ __volatile__("sfence" : : : "memory");
    __asm__ __volatile__("lock; cmpxchg %3, %0; setz %1"
                         : "=m" (*addr), "=a" (result)
                         : "m" (*addr), "r" (new_val), "a" (old) : "memory");
    //__asm__ __volatile__("mfence" : : : "memory");
     return (int) result;
#endif
}

#define ATOMIC_CAS_WEAK_ACQ(a, e, v)    (DIMP_compare_and_swap_acq(( AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_CAS_WEAK_REL(a, e, v)    (DIMP_compare_and_swap_rel(( AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_CAS_WEAK_FULL(a, e, v)    (DIMP_compare_and_swap_full(( AO_t *)(a), (AO_t)(e), (AO_t)(v)))


DIMP_INLINE int
DIMP_pthread_spin_lock (int *lock)
{
  int val = 0;
    //https://code.woboq.org/userspace/glibc/nptl/pthread_spin_lock.c.html
    //https://code.woboq.org/userspace/glibc/nptl/
  do
    {

      do
        {
          /* TODO Back-off.  */
          //atomic_spin_nop ();
          //__asm__ __volatile__("lfence" : : : "memory");
          //DIMP_nop_full();
          //val = *(int *)lock;
          //val = (int)(*lock);
          //printf("A");

          //int *val_ptr = lock;
          //val = *val_ptr;
          val = *lock;
          DIMP_nop_full();
          //printf("B");
        }        
        
      while (val != 0);
       //sprintf("B");
     // printf("C");
      /* We need acquire memory order here for the same reason as mentioned
         for the first try to lock the spinlock.  */
    }
  while (!ATOMIC_CAS_WEAK_ACQ(lock, 0, 1));
  return 0;
}

//#include "pthreadP.h"
//#include <atomic.h>
DIMP_INLINE int
DIMP_pthread_spin_unlock (int *lock)
{
  /* The atomic_store_release synchronizes-with the atomic_exchange_acquire
     or atomic_compare_exchange_weak_acquire in pthread_spin_lock /
     pthread_spin_trylock.  */
  //atomic_store_release (lock, 0);
  __asm__ __volatile__("sfence" : : : "memory"); //Release semantics
  *lock= (int)0;
  DIMP_nop_full(); //mfence is a release
  
  return 0;
}

DIMP_INLINE int
DIMP_pthread_spin_init (int *lock, int pshared)
{
  /* Relaxed MO is fine because this is an initializing store.  */
  //atomic_store_relaxed (lock, 0);
  *lock=(int)0;
  return 0;
}

DIMP_INLINE int
DIMP_pthread_spin_destroy (int *lock)
{
  /* Nothing to do.  */
  return 0;
}
