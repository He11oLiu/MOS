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