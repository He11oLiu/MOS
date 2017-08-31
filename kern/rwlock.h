#ifndef JOS_INC_RWLOCK_H
#define JOS_INC_RWLOCK_H

#include <kern/spinlock.h>
#include <inc/types.h>
#include <inc/atomic.h>

typedef struct dumbrwlock {
	struct spinlock lock;
    atomic_t readers;
}dumbrwlock;

void rw_initlock(dumbrwlock *rwlk);
void dumb_wrlock(dumbrwlock *rwlk);
void dumb_wrunlock(dumbrwlock *rwlk);
void dumb_rdlock(dumbrwlock *rwlk);
void dumb_rdunlock(dumbrwlock *rwlk);

#endif