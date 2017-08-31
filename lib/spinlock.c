// Mutual exclusion spin locks.

#include <inc/lib.h>
#include <inc/spinlock.h>
#include <inc/x86.h>

void
__spin_initlock(struct spinlock *lk, char *name)
{
	lk->locked = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
spin_lock(struct spinlock *lk)
{
	while (xchg(&lk->locked, 1) != 0)
		asm volatile ("pause");
}

// Release the lock.
void
spin_unlock(struct spinlock *lk)
{
	xchg(&lk->locked, 0);
}
