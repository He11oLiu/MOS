#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/cpu.h>
#include <kern/prwlock.h>
#include <kern/sched.h>

#define TESTPRW
/* only list 8 cases as NCPU = 8 */
unsigned int binlist[] = {
     0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,0x100};

unsigned int prwlocknum = 0;
prwlock *locklist[MAXPRWLock];

#ifdef TESTPRW
prwlock lock1;
#endif

void prw_initlock(prwlock *rwlk)
{
    int i = 0;
    rwlk->writer = FREE;
    for (i = 0; i < NCPU; i++)
    {
        rwlk->lockinfo[i].reader = FREE;
        atomic_set(&rwlk->lockinfo[i].version, 0);
    }
    atomic_set(&rwlk->active, 0);
    atomic_set(&rwlk->version, 0);
    locklist[prwlocknum++] = rwlk;
}

void prw_wrlock(prwlock *rwlk)
{
    int newVersion;
    int id = 0;
    unsigned int corewait = 0;
    if (rwlk->writer == PASS)
        return;
    rwlk->writer = LOCKED;
    newVersion = atomic_inc_return(&rwlk->version);
    for (id = 0; id < ncpu; id++)
    {
#ifdef TESTPRW
        cprintf("CPU %d Ver %d\n", id, atomic_read(&rwlk->lockinfo[id].version));
#endif
        if (id != cpunum() && atomic_read(&rwlk->lockinfo[id].version) != newVersion)
        {
            lapic_ipi_dest(id, PRWIPI);
            corewait |= binlist[id];
#ifdef TESTPRW
            cprintf("send ipi %d\n", id);
#endif
        }
    }
    for (id = 0; id < ncpu; id++)
    {
        if (corewait & binlist[id])
        {
            while (atomic_read(&rwlk->lockinfo[id].version) != newVersion)
                asm volatile("pause");
        }
    }
    while (atomic_read(&rwlk->active) != 0)
    {
        lock_kernel();
        sched_yield();
    }
}

void prw_wrunlock(prwlock *rwlk)
{
    rwlk->writer = FREE;
}

void prw_rdlock(prwlock *rwlk)
{
    struct percpu_prwlock *st;
    int lockversion;
    st = &rwlk->lockinfo[cpunum()];
    st->reader = PASSIVE;
    while (rwlk->writer != FREE)
    {
        st->reader = FREE;
        lockversion = atomic_read(&rwlk->version);
        atomic_set(&st->version, lockversion);
        while (rwlk->writer != FREE)
            asm volatile("pause");
        st = &rwlk->lockinfo[cpunum()];
        st->reader = PASSIVE;
    }
}

void prw_rdunlock(prwlock *rwlk)
{
    struct percpu_prwlock *st;
    int lockversion;
    st = &rwlk->lockinfo[cpunum()];
    if (st->reader == PASSIVE)
        st->reader = FREE;
    else
        atomic_dec(&rwlk->active);
    lockversion = atomic_read(&rwlk->version);
    atomic_set(&st->version, lockversion);
}

void prw_sched(prwlock *rwlk)
{
    struct percpu_prwlock *st;
    int lockversion;
    st = &rwlk->lockinfo[cpunum()];
    if (st->reader == PASSIVE)
    {
        atomic_inc(&rwlk->active);
        st->reader = FREE;
    }
    lockversion = atomic_read(&rwlk->version);
    atomic_set(&st->version, lockversion);
}