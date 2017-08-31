#include <inc/lib.h>

void lockenv();

void umain(int argc, char **argv)
{
    int eid;
    int ret;
    eid = fork();
    if (eid == 0)
    {
        int *trans;
        void *dstva;
        dstva = (void *)USTACKTOP;
        trans = dstva;
        ret = sys_ipc_recv(dstva);
        if (ret != 0)
            cprintf("error %e\n", ret);
        else
        {
            cprintf("Recv trans %d success!\n", *trans);
            // lockenv(&lock);
        }
    }
    else
    {
        struct spinlock *lock;
        int *trans;
        void *srcva = (void *)USTACKTOP;
        trans = srcva;
        
        *trans = 20;

        // spin_initlock(&lock);
        ret = sys_ipc_try_send(eid, 0, srcva, PTE_W | PTE_U | PTE_P);
        cprintf("send state %e\n", ret);
        // ?????????????????? how to use ipc .....
        while (ret == -E_IPC_NOT_RECV)
        {
            sys_yield();
            ret = sys_ipc_try_send(eid, 0, srcva, PTE_W | PTE_U | PTE_P);
            cprintf("send state %e\n", ret);
        }
        if (ret == 0)
            cprintf("Send trans %d success!\n", *trans);
        // lockenv(&lock);
    }
}

void lockenv(struct spinlock *lock)
{
    int i = 0, j = 0;
    envid_t id = sys_getenvid();
    cprintf("%d is going to get lock\n", id);
    spin_lock(lock);
    cprintf("%d get lock\n", id);
    for (i = 0; i < 10000; i++)
        for (j = 0; j < 1000; j++)
            ;
    cprintf("%d unlock\n", id);
    spin_unlock(lock);
}