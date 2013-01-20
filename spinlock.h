/*
 * A ticketing spinlock implementation for user space similar to that used by the linux kernel
 * A fair implementation that ensures that locks are granted based on the
 * order in which the requests arrived instead of being random
 */

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define NUM_THREADS (65535)

#if NUM_THREADS < 256
#define MAX_THREADS (255)
    typedef unsigned short __ticketpair;
    typedef unsigned char __tickets;
#else
#define MAX_THREADS (65535)
    typedef unsigned int __ticketpair;
    typedef unsigned short __tickets;
#endif

#define __TICKET_SHIFT ( sizeof(__tickets) * 8)

#define __SPIN_LOCK_INITIALIZER { {0} }

/*
 * Atomic exchange and add. 
 * Atomically gets the current tail or ticket number into src 
 * before incrementing our ticket number in the line
 */

#define __XADD(src, dst) ({                         \
    __typeof__ (src) __ret = (src);                 \
    switch (sizeof(*(dst))) {                       \
    case 1:                                         \
        asm __volatile__("lock xaddb %b0, %1\n"     \
                         :"+r"(__ret), "+m"(*(dst)) \
                         : :"memory","cc");         \
    break;                                          \
    case 2:                                         \
        asm __volatile__("lock xaddw %w0, %1\n"     \
                         :"+r"(__ret), "+m"(*(dst)) \
                         : :"memory", "cc");        \
    break;                                          \
    case 4:                                         \
        asm __volatile__("lock xaddl %0, %1\n"      \
                         :"+r"(__ret), "+m"(*(dst)) \
                         : :"memory", "cc");        \
        break;                                      \
    case 8:                                         \
        asm __volatile__("lock xaddq %q0, %1\n"     \
                         :"+r"(__ret), "+m"(*(dst)) \
                         : :"memory", "cc");        \
        break;                                      \
    default:                                        \
        assert(0);                                  \
    }                                               \
    __ret;                                          \
})

/*
 * Atomic compare and exchange. 
 * Compare old ticket value in rax with current in dst. memory.
 * If equal, exchange src in dst. memory and return old in rax
 * Else return current value in rax. 
 */

#define __CMPXCHG(src, dst, old) ({                             \
    __typeof__ (*(dst)) __ret;                                  \
    switch(sizeof(*(dst))) {                                    \
    case 1:                                                     \
        __asm__ __volatile__("lock cmpxchgb %2, %1\n"           \
                             :"=a"(__ret), "+m"(*(dst))         \
                             :"r"(src), "0"(old) : "memory");   \
        break;                                                  \
    case 2:                                                     \
        __asm__ __volatile__("lock cmpxchgw %2, %1\n"           \
                             :"=a"(__ret), "+m"(*(dst))         \
                             :"r"(src), "0"(old) : "memory");   \
        break;                                                  \
    case 4:                                                     \
        __asm__ __volatile__("lock cmpxchgl %2, %1\n"           \
                             :"=a"(__ret), "+m"(*(dst))         \
                             :"r"(src), "0"(old) : "memory");   \
        break;                                                  \
    case 8:                                                     \
        __asm__ __volatile__("lock cmpxchgq %2, %1\n"           \
                             :"=a"(__ret), "+m"(*(dst))         \
                             :"r"(src), "0"(old) : "memory");   \
        break;                                                  \
    default:                                                    \
        assert(0);                                              \
    }                                                           \
    __ret;                                                      \
})
    
#define __ADD(src, dst) ({                              \
    __typeof__ (*(dst)) __ret = (src);                  \
    switch(sizeof(*(dst))) {                            \
    case 1:                                             \
        asm __volatile__("lock addb %b1, %0\n"          \
                         :"+m"(*(dst))                  \
                         :"ri"(src) : "memory","cc");   \
    break;                                              \
    case 2:                                             \
        asm __volatile__("lock addw %w1, %0\n"          \
                         :"+m"(*(dst))                  \
                         :"ri"(src) : "memory", "cc");  \
        break;                                          \
    case 4:                                             \
        asm __volatile__("lock addl %1, %0\n"           \
                         :"+m"(*(dst))                  \
                         :"ri"(src) : "memory", "cc");  \
        break;                                          \
    case 8:                                             \
        asm __volatile__("lock addq %q1, %0\n"          \
                         :"+m"(*(dst))                  \
                         :"ri"(src) : "memory", "cc");  \
        break;                                          \
    default:                                            \
        assert(0);                                      \
    }                                                   \
    __ret;                                              \
})

#define __RELAX() do { asm __volatile__("rep;nop\n" : : :"memory"); } while(0)

#define __ACCESS_ONCE(m) ( * (volatile __typeof__(m) *) &(m) )

struct spinlock {
    union {
        __ticketpair ticket_pair;
        struct tickets { __tickets head, tail; } tickets;
    };
};

static __inline__ void spin_lock_init(struct spinlock *lock)
{
    *lock = (struct spinlock)__SPIN_LOCK_INITIALIZER;
}

/*
 * Atomically note down the current tail/ticket-owner before incrementing
 * and wait till our turn comes. ( i.e till head == tail )
 */
static __inline__ void spin_lock(struct spinlock *lock)
{
    register struct tickets inc = { .tail = 1 };
    inc = __XADD(inc, &lock->tickets);
    do
    {
        if(inc.head == inc.tail) break;
        //loop
        __RELAX();
        inc.head = __ACCESS_ONCE(lock->tickets.head);
    } while(1);

#ifdef DEBUG
    printf("Thread [%lld] got the lock\n", (unsigned long long)pthread_self());
#endif

}

/*
 * Increment the head of the ticket. 
 * Spinner checks the current head against his ticket number
 */
static __inline__ void spin_unlock(struct spinlock *lock)
{
    __ADD(1, &lock->tickets.head);
}

/*
 * Try lock. Return 0 on failure. 1 on success.
 */

static __inline__ int spin_trylock(struct spinlock *lock)
{
    struct spinlock __old, __new;
    __old.tickets  = __ACCESS_ONCE(lock->tickets);
    if(__old.tickets.head != __old.tickets.tail) return 0;
    /*
     * Now increment the ticket number (high order ticket/tail byte/s) for the lock 
     * and compare if the old lock changed with someone grabbing it before us
     */
    __new.ticket_pair = __old.ticket_pair + (1 << __TICKET_SHIFT); 
    return __CMPXCHG(__new.ticket_pair, &lock->ticket_pair, __old.ticket_pair) == __old.ticket_pair;
}

#ifdef __cplusplus
}
#endif

#endif
