/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/rwlock.h>
#include <kern/prwlock.h>
#include <kern/graph.h>

static void boot_aps(void);
static void get_boot_info(void);

struct boot_info
{
	short scrnx, scrny;
	uint8_t *vram;
};

#ifdef TESTRW
// test reader-writer lock
dumbrwlock lock1;
dumbrwlock lock2;
#endif

void i386_init()
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	// Lab 2 memory management initialization functions
	mem_init();

	// get information from boot_info
	// must be used after mem_init()
	get_boot_info();

	// init graph
	graph_init();

	// user environment initialization functions
	env_init();
	trap_init();

	// multiprocessor initialization functions
	mp_init();
	lapic_init();

	// multitasking initialization functions
	pic_init();

	// Acquire the big kernel lock before waking up APs
	lock_kernel();

#ifdef TESTRW
	// test reader-writer lock
	rw_initlock(&lock1);
	rw_initlock(&lock2);

	dumb_wrlock(&lock1);
	cprintf("[rw] CPU %d gain writer lock1\n", cpunum());
	dumb_rdlock(&lock2);
	cprintf("[rw] CPU %d gain reader lock2\n", cpunum());

#endif

	// Starting non-boot CPUs
	boot_aps();

#ifdef TESTRW
	cprintf("[rw] CPU %d going to release writer lock1\n", cpunum());
	dumb_wrunlock(&lock1);
	cprintf("[rw] CPU %d going to release reader lock2\n", cpunum());
	dumb_rdunlock(&lock2);
#endif

	// Start fs.
	ENV_CREATE(fs_fs, ENV_TYPE_FS);

	// Touch all you want.
	ENV_CREATE(user_msh, ENV_TYPE_USER);

	// Should not be necessary - drains keyboard because interrupt has given up.
	kbd_intr();

#ifdef TESTPRW
	unlock_kernel();
	prw_initlock(&lock1);
	prw_wrlock(&lock1);
	prw_wrunlock(&lock1);
	prw_rdlock(&lock1);
	cprintf("====%d Gain Reader Lock====\n", cpunum());
	lapic_ipi_dest(3, DEBUGPRW);
	for (int i = 0; i < 10000; i++)
		asm volatile("pause");
	prw_rdunlock(&lock1);
	cprintf("====%d release Reader Lock====\n", cpunum());
	lock_kernel();
#endif

	// Schedule and run the first user environment!
	sched_yield();
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
static void
boot_aps(void)
{
	extern unsigned char mpentry_start[], mpentry_end[];
	void *code;
	struct CpuInfo *c;

	// Write entry code to unused memory at MPENTRY_PADDR
	code = KADDR(MPENTRY_PADDR);
	memmove(code, mpentry_start, mpentry_end - mpentry_start);

	// Boot each AP one at a time
	for (c = cpus; c < cpus + ncpu; c++)
	{
		if (c == cpus + cpunum()) // We've started already.
			continue;

		// Tell mpentry.S what stack to use
		mpentry_kstack = percpu_kstacks[c - cpus] + KSTKSIZE;
		// Start the CPU at mpentry_start
		lapic_startap(c->cpu_id, PADDR(code));
		// Wait for the CPU to finish some basic setup in mp_main()
		while (c->cpu_status != CPU_STARTED)
			;
	}
}

// Setup code for APs
void mp_main(void)
{
	// We are in high EIP now, safe to switch to kern_pgdir
	lcr3(PADDR(kern_pgdir));
	cprintf("[MP] CPU %d starting\n", cpunum());

	lapic_init();
	env_init_percpu();
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up

#ifdef TESTRW
	// reader-writer lock test
	dumb_rdlock(&lock1);
	cprintf("[rw] %d l1\n", cpunum());
	asm volatile("pause");
	dumb_rdunlock(&lock1);
	cprintf("[rw] %d unl1\n", cpunum());

	dumb_wrlock(&lock2);
	cprintf("[rw] %d l2\n", cpunum());
	asm volatile("pause");
	cprintf("[rw] %d unl2\n", cpunum());
	dumb_wrunlock(&lock2);
#endif

	// Now that we have finished some basic setup, call sched_yield()
	// to start running processes on this CPU.  But make sure that
	// only one CPU can enter the scheduler at a time!
	//
	lock_kernel();
	cprintf("[MP] CPU %d sched\n", cpunum());
	sched_yield();
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on= unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void _panic(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	asm volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic on CPU %d at %s:%d: ", cpunum(), file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void _warn(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}

static void get_boot_info(void)
{
	struct boot_info *info = (struct boot_info *)(KADDR(0x0ff0));
	// Init Graph info
	graph.scrnx = info->scrnx;
	graph.scrny = info->scrny;
	graph.vram = info->vram;
}