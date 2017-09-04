#ifndef JOS_INC_PRWLOCK_H
#define JOS_INC_PRWLOCK_H

#include <kern/spinlock.h>
#include <inc/types.h>
#include <inc/atomic.h>
#include <kern/cpu.h>

#define MAXPRWLock 100
#define TESTPRW

enum lock_status
{
    FREE = 0,
    LOCKED,
    PASS,
    PASSIVE
};

struct percpu_prwlock
{
    enum lock_status reader;
    atomic_t version;
};

typedef struct prwlock
{
    enum lock_status writer;
    struct percpu_prwlock lockinfo[NCPU];
    atomic_t active;
    atomic_t version;
} prwlock;

extern unsigned int prwlocknum;
extern prwlock *locklist[MAXPRWLock];

#ifdef TESTPRW
extern prwlock lock1;
#endif


void prw_initlock(prwlock *rwlk);
void prw_wrlock(prwlock *rwlk);
void prw_wrunlock(prwlock *rwlk);
void prw_rdlock(prwlock *rwlk);
void prw_rdunlock(prwlock *rwlk);
void prw_sched();

#endif /* JOS_INC_PRWLOCK_H */