# Lab 4: Preemptive Multitasking

> In part A you will add multiprocessor support to JOS, implement round-robin scheduling, and add basic environment management system calls (calls that create and destroy environments, and allocate/map memory).

Part A将支持最基本的多任务支持。采用RR调度算法，提供最基本的进程管理系统调用（可以支持创建或销毁进程，支持分配或映射内存）

> In part B, you will implement a Unix-like `fork()`, which allows a user-mode environment to create copies of itself.

Part B将实现`fork`

> Finally, in part C you will add support for inter-process communication (IPC), allowing different user-mode environments to communicate and synchronize with each other explicitly. You will also add support for hardware clock interrupts and preemption.

Part C将提供`IPC`，允许不同的用户进程进行同步或交流。同时支持硬件时钟中断与抢断式任务调度。

## Part A: Multiprocessor Support and Cooperative Multitasking

### Multiprocessor Support

> In an SMP system, each CPU has an accompanying local APIC (LAPIC) unit. The LAPIC units are responsible for delivering interrupts throughout the system. The LAPIC also provides its connected CPU with a unique identifier. In this lab, we make use of the following basic functionality of the LAPIC unit (in `kern/lapic.c`)

> 在SMP系统中， 每一个CPU中都有一个`APIC(LAPIC)`单元,LAPIC单元为系统提供中断. LAPIC也为其连接CPU提供了一个独特的标识符

> A processor accesses its LAPIC using memory-mapped I/O (MMIO). In MMIO, a portion of *physical* memory is hardwired to the registers of some I/O devices, so the same load/store instructions typically used to access memory can be used to access device registers.

### Exercise1

`mmio_map_region`是用于将`mmio`映射到`MMIOBASE`到`MMIOLIM`的函数。这里和前面写的映射差不多，注意的是这些页需要设置`cache disable`与`write-through`才能保证直接写到对应的`IO`口子上去。

```c
void *
mmio_map_region(physaddr_t pa, size_t size)
{
	size = ROUNDUP(size+PGOFF(pa), PGSIZE);
	pa = ROUNDDOWN(pa, PGSIZE);
	if (base + size >= MMIOLIM)
		panic("mmio_map_region error: size overflow");
	boot_map_region(kern_pgdir, base, size, pa, PTE_PCD | PTE_PWT | PTE_W);
	base += size;
	return (void *)(base - size);
}
```

### Application Processor Bootstrap

```c
mp_init();
lapic_init();
boot_aps();
```

每一个`cpu`的启动代码放在`/kern/mpentry.s`下。

`AP`以实模式启动，启动的地址为`XY00:0000`，其中`XY`式随着`STARTUP`的`IPI`一起传过来的`8-bit`数据。

该`entry`的代码与`boot.s`的类似，最初的`GDT`是直接写在该代码下，页表用的与`boot.s`一致的初始页表，分析见`lab1`

在`/kern/init.c`下，`mp_init`先将各种信息放入`cpus, bootcpu`并将`IMCR`设置到对应的模式（从`LAPIC`接受`int`）。

`mp floating pointer structure`可能会在以下三个位置

```c
// 1) in the first KB of the EBDA;
// 2) if there is no EBDA, in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
```

将查找`mp`表打包到`mpsearch`，将`mp`表检查与配置匹配打包到`mpconfig`。

`/kern/lapic.c`下`lapic_init`则是将`lapicaddr`映射到`MMIOBASE`。`lapicw`将向`MMIO`写的操作打包。对`lapic`进行设置。

再看`boot_aps`

`boot_aps`将`entry`的代码映射到`MPENTRY_PADDR(0x7000)`下。

对于每一个`cpu`分配其`kst`，通过`lapic`发送`IPI`，并等待这个核心起来后再唤醒下一个核心。

### Exercise 2

> Read `boot_aps()` and `mp_main()` in `kern/init.c`, and the assembly code in `kern/mpentry.S`. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of `page_init()` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated `check_page_free_list()` test (but might fail the updated `check_kern_pgdir()` test, which we will fix soon). 

所以只用在`page_init()`的时候，将`MPENTRY_PADDR`对应的页设置`pp_ref = 1`且不添加到`freelist`即可

```c
	for (i = 1; i < npages_basemem; i++)
	{
		if (i == PGNUM(MPENTRY_PADDR))
		{
			pages[i].pp_ref = 1;
			pages[i].pp_link = NULL;
			continue;
		}
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
```

### Question

1. Compare `kern/mpentry.S` side by side with `boot/boot.S`. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel, what is the purpose of macro `MPBOOTPHYS`? Why is it necessary in `kern/mpentry.S` but not in `boot/boot.S`? In other words, what could go wrong if it were omitted in `kern/mpentry.S`? 
   Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

   > it uses MPBOOTPHYS to calculate absolute addresses of its symbols, rather than relying on the linker to fill them

   因为在`BSP`上已经在使用页模式，但是`ap`还没开启页模式，所以`ap`上要手动算其绝对地址。

#### Per-CPU State and Initialization

`/kern/cpu.h`中定义的接口`cpunum(void)`可以用来反馈当前的`cpu`的`id`。其是通过读取`lapic`的对应的`IO`接口实现的。

```c
int cpunum(void)
{
	if (lapic)
		return lapic[ID] >> 24;
	return 0;
}
```

而`thiscpu`的宏将其打包，直接返回了这个`cpu`的结构体。

```c
#define thiscpu (&cpus[cpunum()])
```

之后直接可以用过`thiscpu`获取当前调用该语句的`cpu`的信息。

- **Per-CPU kernel stack**

  由于有可能多个核心同时陷入内核态，所以不能使用一个内核栈。在`init`时已经分配了一个`percpu_kstacks`给单独的`cpu`使用，并传给`mpentry_kstack`，最后再`entry`中设置给`%esp`用作该`cpu`的内核栈。

  原来的内核栈从`KSTACKTOP`开始，新的`cpu`的栈在原来的栈的下面开始大小为`KSTKGAP`

  >  Exercise 3. Modify `mem_init_mp()` (in `kern/pmap.c`) to map per-CPU stacks starting at `KSTACKTOP`, as shown in `inc/memlayout.h`. The size of each stack is`KSTKSIZE` bytes plus `KSTKGAP` bytes of unmapped guard pages. Your code should pass the new check in `check_kern_pgdir()`

  这个只用修改`pmap.c`下的`mem_init`中间做的映射就好了。

  在原来映射内核的栈的地方，修改为对所有的`cpu`的内核栈进行映射。

  ```c
  for (i = 0; i < NCPU; i++)
  	boot_map_region(kern_pgdir, KSTACKTOP - KSTKSIZE - i * (KSTKSIZE + KSTKGAP), KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
  ```

- **Per-CPU TSS and TSS descriptor**

  > Exercise 4. The code in `trap_init_percpu()` (`kern/trap.c`) initializes the TSS and TSS descriptor for the BSP. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global `ts` variable any more.)

  需要修改`trap_init_percpu`设置`TSS`中`ss0`与`esp0`的值，用于从用户态切换到内核态需要的栈的改变。但是现在的每一个`cpu`的栈都不同，所以这里需要每个`cpu`维护一个自己的`TSS`并将其`SS0`与`ESP0`指到每一个`CPU`的内核栈上。

  这里要注意`segment selector`的低三位都是特殊位，需要移位操作：

  ```c
  // Initialize and load the per-CPU TSS and IDT
  void trap_init_percpu(void)
  {	
  	// Setup a TSS so that we get the right stack
  	// when we trap to the kernel.
  	int i = cpunum();
  	uintptr_t GD_TSSi;
  	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
  	thiscpu->cpu_ts.ts_ss0 = GD_KD;
  	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

  	// Initialize the TSS slot of the gdt.
  	// the bottom three bits are special
  	GD_TSSi = GD_TSS0 + (i << 3);
  	gdt[GD_TSSi >> 3] = SEG16(STS_T32A, (uint32_t)(&(thiscpu->cpu_ts)),
  							  sizeof(struct Taskstate) - 1, 0);
  	gdt[GD_TSSi >> 3].sd_s = 0;

  	// Load the TSS selector (like other segment selectors, the
  	// bottom three bits are special; we leave them 0)
  	ltr(GD_TSSi);

  	// Load the IDT
  	lidt(&idt_pd);
  }
  ```

  > LTR指令和STR指令是用来更改和读取任务寄存器的可见部分的。两条指令都有一个操作数，一个在内存中的或在通用寄存器中的16-位选择子。
  >
  > **LTR（Load task register）**加载一个选择子操作数到任务寄存器的可见部分，这个选择子必须指定一个在GDT中的TSS描述符。LTR也用TSS中的信息来加载任务寄存器的不可见部分。LTR是一条特权指令，只能当CPL是0时才能执行这条执令。LTR一般是当操作系统初始化过程执行的，用来初始化任务寄存器。以后，任务寄存器（TR）的内容由每次任务切换来改变。
  >
  > **STR（Store task register）**存储任务寄存器的可见部分到一个通用寄存器或者到一个内存的字内。STR不是特权指令。

- **Per-CPU current environment pointer**

  Since each CPU can run different user process simultaneously, we redefined the symbol `curenv` to refer to `cpus[cpunum()].cpu_env` (or `thiscpu->cpu_env`), which points to the environment *currently* executing on the *current*CPU (the CPU on which the code is running).

  通过宏定义已经解决

  ```c
  #define curenv (thiscpu->cpu_env)		// Current environment
  ```

  ​

- **Per-CPU system registers**

  All registers, including system registers, are private to a CPU. Therefore, instructions that initialize these registers, such as `lcr3()`, `ltr()`, `lgdt()`, `lidt()`, etc., must be executed once on each CPU. Functions `env_init_percpu()` and `trap_init_percpu()` are defined for this purpose.

  将每个`cpu`需要单独执行一遍的部分打包在`percpu()`中，在`mp_main`中调用这些方法。

  ```
  	env_init_percpu();
  	trap_init_percpu();
  ```

### Locking

#### Exercise 5

Apply the big kernel lock as described above, by calling `lock_kernel()` and `unlock_kernel()` at the proper locations.

首先看下`spinlock`的实现

```c
// Mutual exclusion lock.
struct spinlock {
	unsigned locked;       // Is the lock held?

#ifdef DEBUG_SPINLOCK
	// For debugging:
	char *name;            // Name of lock.
	struct CpuInfo *cpu;   // The CPU holding the lock.
	uintptr_t pcs[10];     // The call stack (an array of program counters)
	                       // that locked the lock.
#endif
};
```

可以发现这种设计，在`DEBUG`模式下，可以顺带保存获取的`CPU`，调用栈，和锁的名字来查错。

`big kernel lock`顶层包装了两个函数

```c
static inline void
lock_kernel(void)
{
	spin_lock(&kernel_lock);
}

static inline void
unlock_kernel(void)
{
	spin_unlock(&kernel_lock);

	// Normally we wouldn't need to do this, but QEMU only runs
	// one CPU at a time and has a long time-slice.  Without the
	// pause, this CPU is likely to reacquire the lock before
	// another CPU has even been given a chance to acquire it.
	asm volatile("pause");
}
```

其中`unlock`后`pause`的原因是，`QEMU`模拟多`CPU`的时候，给了每一个`CPU`一个非常长的`slice`，导致这个`cpu`老是有更多的机会获取锁，这里类似给其他`cpu`机会来获取锁。

再往下看具体`spin lock`的实现

```c
void
spin_lock(struct spinlock *lk)
{
#ifdef DEBUG_SPINLOCK
	if (holding(lk))
		panic("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
#endif

	// The xchg is atomic.
	// It also serializes, so that reads after acquire are not
	// reordered before it. 
	while (xchg(&lk->locked, 1) != 0)
		asm volatile ("pause");

	// Record info about lock acquisition for debugging.
#ifdef DEBUG_SPINLOCK
	lk->cpu = thiscpu;
	get_caller_pcs(lk->pcs);
#endif
}
```

最核心的`while(xchg(&lk->locked,1)!=0)` `xchg`是原子操作，保证了锁获取的原子性。

解锁同样也使用

```c
	// The xchg instruction is atomic (i.e. uses the "lock" prefix) with
	// respect to any other instruction which references the same memory.
	// x86 CPUs will not reorder loads/stores across locked instructions
	// (vol 3, 8.2.2). Because xchg() is implemented using asm volatile,
	// gcc will not reorder C statements across the xchg.
	xchg(&lk->locked, 0);
```

注释这里说`xchg`在实现的时候，里面先用了`lock;`的汇编指令，保证了`x86 cpus`不会执行其后的指令，也就是**内存屏障**。

而`xchg`用的是`asm volatile`，故`gcc`也不会将指令在这个指令的周围进行优化，同样是编译里的**内存屏障**。

 然后，在合适的位置添加锁：

- In `i386_init()`, acquire the lock before the BSP wakes up the other CPUs. 

  ```c
  	// Acquire the big kernel lock before waking up APs
  	lock_kernel();
  	// Starting non-boot CPUs
  	boot_aps();
  ```

- In `mp_main()`, acquire the lock after initializing the AP, and then call `sched_yield()` to start running environments on this AP.

  ```c
  	// Now that we have finished some basic setup, call sched_yield()
  	// to start running processes on this CPU.  But make sure that
  	// only one CPU can enter the scheduler at a time!
  	//
  	lock_kernel();
  	sched_yield();
  ```

- In `trap()`, acquire the lock when trapped from user mode. To determine whether a trap happened in user mode or in kernel mode, check the low bits of the `tf_cs`.

  ```c
  	if ((tf->tf_cs & 3) == 3)
  	{
  		// Trapped from user mode.
  		// Acquire the big kernel lock before doing any
  		// serious kernel work.
  		lock_kernel();
  ```

- In `env_run()`, release the lock *right before* switching to user mode. Do not do that too early or too late, otherwise you will experience races or deadlocks.

  ```c
  	unlock_kernel();
  	// eip has been set in load_icode
  	// restore the environment's registers
  	env_pop_tf(&e->env_tf);
  ```

#### Question

It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

中断进`trap`之前需要`push`东西，虽然不能获取内核，但是其`push`的东西仍然会使得内核栈多东西。

> Challenge! The big kernel lock is simple and easy to use. Nevertheless, it eliminates all concurrency in kernel mode. Most modern operating systems use different locks to protect different parts of their shared state, an approach called *fine-grained locking*. Fine-grained locking can increase performance significantly, but is more difficult to implement and error-prone. If you are brave enough, drop the big kernel lock and embrace concurrency in JOS!
>
> It is up to you to decide the locking granularity (the amount of data that a lock protects). As a hint, you may consider using spin locks to ensure exclusive access to these shared components in the JOS kernel:
>
> - The page allocator.
> - The console driver.
> - The scheduler.
> - The inter-process communication (IPC) state that you will implement in the part C.



### Round-Robin Scheduling

现在做的调度还是非抢占的任务调度，所以设计了新的系统调用`sys_yield`，用户可以通过调用`sys_yield`，来陷入内核调用`sched_yield`进行调度。

#### Exercise 6

Implement round-robin scheduling in `sched_yield()` as described above. 

```c
// Choose a user environment to run and run it.
void sched_yield(void)
{
	struct Env *idle;
	int i, cur_i;
	if (!curenv)
	{
		for (i = 0; i < NENV; i++)
			if (envs[i].env_status == ENV_RUNNABLE)
				env_run(&envs[i]);
	}
	else
	{
		cur_i = ENVX(curenv->env_id);
		for (i = cur_i + 1; i < NENV + cur_i; i++)
			if (envs[i % NENV].env_status == ENV_RUNNABLE)
				env_run(&envs[i]);
		if (curenv->env_status == ENV_RUNNING)
			env_run(curenv);
	}

	// sched_halt never returns
	sched_halt();
}
```

Don't forget to modify `syscall()` to dispatch `sys_yield()`.

```
	case SYS_yield:
		return sys_yield();
```

Make sure to invoke `sched_yield()` in `mp_main`.

Modify `kern/init.c` to create three (or more!) environments that all run the program `user/yield.c`.

```
	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
```

然后发现原来`env_run`写错了... 修改后可以正确`shed`

```
[00000000] new env 00001000
[00000000] new env 00001001
[00000000] new env 00001002
Hello, I am environment 00001000.
Hello, I am environment 00001001.
Hello, I am environment 00001002.
Back in environment 00001000, iteration 0.
Back in environment 00001001, iteration 0.
Back in environment 00001002, iteration 0.
Back in environment 00001000, iteration 1.
Back in environment 00001001, iteration 1.
Back in environment 00001002, iteration 1.
Back in environment 00001000, iteration 2.
Back in environment 00001001, iteration 2.
Back in environment 00001002, iteration 2.
Back in environment 00001000, iteration 3.
Back in environment 00001001, iteration 3.
Back in environment 00001002, iteration 3.
Back in environment 00001000, iteration 4.
All done in environment 00001000.
[00001000] exiting gracefully
[00001000] free env 00001000
Back in environment 00001001, iteration 4.
All done in environment 00001001.
[00001001] exiting gracefully
[00001001] free env 00001001
Back in environment 00001002, iteration 4.
All done in environment 00001002.
[00001002] exiting gracefully
[00001002] free env 00001002
No runnable environments in the system!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K> 
```

> If you use CPUS=1 at this point, all environments should successfully run. Setting CPUS larger than 1 at this time may result in a general protection fault, kernel page fault, or other unexpected interrupt once there are no more runnable environments due to unhandled timer interrupts (which we will fix below!).

#### Question

1. In your implementation of `env_run()` you should have called `lcr3()`. Before and after the call to `lcr3()`, your code makes references (at least it should) to the variable `e`, the argument to `env_run`. Upon loading the `%cr3` register, the addressing context used by the MMU is instantly changed. But a virtual address (namely `e`) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer `e` be dereferenced both before and after the addressing switch?

   `e`是在内核的，所有的`env`的内核部分的页表是直接复制的`kernpgdir`，所以他们两映射的地址是一样的。

2. Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

   我的`syscall`目前还是走的`trap`，在`trap()`中保存

   ```c
   // Copy trap frame (which is currently on the stack)
   // into 'curenv->env_tf', so that running the environment
   // will restart at the trap point.
   curenv->env_tf = *tf;
   // The trapframe on the stack should be ignored from here on.
   tf = &curenv->env_tf;
   ```

在这里完全理解了当前的`trap`相关的结构。

在`env_create`时调用的`load_icodes`使得所有的`env`均有分配自己的地址空间，并且分配了单独的大小为`PGSIZE`的栈，放在了对应的`env`的地址空间中的`USTACKTOP`。

然后发现在写`load_icodes`的时候没有初始化`tf`中的`esp`

```c
region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);
e->env_tf.tf_esp = USTACKTOP;
```

当每次`syscall`或者陷入`trap`的时候，都把`tf`丢到了这个栈里面。

`trap`里面没有切到内核的页表，所以在处理正常需要返回的`trap`的时候，直接从这个`tf`返回去。当处理需要`sched`的`case`的时候，会在`env_run`的时候切到那个`env`的页表。

- 若该`env`刚刚加载，`tf`是手动设置的。`iret`后栈就到了我们设置的`eip`与`esp`。
- 若该`env`是被`sched`走的话，`tf`是中断自己压的+`trap entry`时压的内容。最终`iret`会恢复`esp`的值，也就是恢复到陷入到`trap`之前。

> Challenge! 
>
> Add a less trivial scheduling policy to the kernel, such as a fixed-priority scheduler that allows each environment to be assigned a priority and ensures that higher-priority environments are always chosen in preference to lower-priority environments. If you're feeling really adventurous, try implementing a Unix-style adjustable-priority scheduler or even a lottery or stride scheduler. (Look up "lottery scheduling" and "stride scheduling" in Google.)
>
> Write a test program or two that verifies that your scheduling algorithm is working correctly (i.e., the right environments get run in the right order). It may be easier to write these test programs once you have implemented `fork()` and IPC in parts B and C of this lab.

> Challenge! 
>
> The JOS kernel currently does not allow applications to use the x86 processor's x87 floating-point unit (FPU), MMX instructions, or Streaming SIMD Extensions (SSE). Extend the `Env` structure to provide a save area for the processor's floating point state, and extend the context switching code to save and restore this state properly when switching from one environment to another. The `FXSAVE` and `FXRSTOR`instructions may be useful, but note that these are not in the old i386 user's manual because they were introduced in more recent processors. Write a user-level test program that does something cool with floating-point.

System Calls for Environment Creation