#ifndef JOS_LIB_SPINLOCK_H
#define JOS_LIB_SPINLOCK_H

#include <inc/types.h>

// Mutual exclusion lock.
struct spinlock {
	unsigned locked;       // Is the lock held?
};

void __spin_initlock(struct spinlock *lk, char *name);
void spin_lock(struct spinlock *lk);
void spin_unlock(struct spinlock *lk);
#define spin_initlock(lock)   __spin_initlock(lock, #lock)

#endif
