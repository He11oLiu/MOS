
#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/rwlock.h>


void rw_initlock(dumbrwlock *rwlk)
{
    spin_initlock(&rwlk->lock);
    rwlk->readers.counter = 0;
}

void dumb_wrlock(dumbrwlock *rwlk)
{
    spin_lock(&rwlk->lock);
    while (rwlk->readers.counter > 0)
        asm volatile("pause");
}

void dumb_wrunlock(dumbrwlock *rwlk)
{
    spin_unlock(&rwlk->lock);
}

void dumb_rdlock(dumbrwlock *rwlk)
{
    while (1)
    {
        atomic_inc(&rwlk->readers);
        if (!rwlk->lock.locked)
            return;
        atomic_dec(&rwlk->readers);
        while (rwlk->lock.locked)
        {
            cprintf("");
            asm volatile("pause");
        }
    }
}

void dumb_rdunlock(dumbrwlock *rwlk)
{
    atomic_dec(&rwlk->readers);
}