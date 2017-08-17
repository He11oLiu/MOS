# Lab 3: User Environments

**JOS 中的Environments = UNIX 下的Process**



## Part A: User Environments and Exception Handling

全局维护着三个变量

```c
struct Env *envs = NULL;		// All environments
struct Env *curenv = NULL;		// The current env
static struct Env *env_free_list;	// Free environment list
					// (linked by Env->env_link)
```

分别保存着全部进程数组，正在运行的进程，与处于空闲状态的进程的链表。

其中`Env`的结构体如下：

```c
struct Env {
	struct Trapframe env_tf;	// Saved registers
	struct Env *env_link;		// Next free Env
	envid_t env_id;			// Unique environment identifier
	envid_t env_parent_id;		// env_id of this env's parent
	enum EnvType env_type;		// Indicates special system environments
	unsigned env_status;		// Status of the environment
	uint32_t env_runs;		// Number of times environment has run

	// Address space
	pde_t *env_pgdir;		// Kernel virtual address of page dir
};
```

`Trapframe`结构体用来保存各种寄存器

```c
struct PushRegs {
	/* registers as pushed by pusha */
	uint32_t reg_edi;
	uint32_t reg_esi;
	uint32_t reg_ebp;
	uint32_t reg_oesp;		/* Useless */
	uint32_t reg_ebx;
	uint32_t reg_edx;
	uint32_t reg_ecx;
	uint32_t reg_eax;
} __attribute__((packed));

struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	/* below here defined by x86 hardware */
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	/* below here only when crossing rings, such as from user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
} __attribute__((packed));
```

`env_id`用来标示这个进程

`env_parent_id`用来构造家族树

`env_type`则保存这个进程的状态：

```
ENV_FREE:
	Indicates that the Env structure is inactive, and therefore on the env_free_list.
ENV_RUNNABLE:
	Indicates that the Env structure represents an environment that is waiting to run on the processor.
ENV_RUNNING:
	Indicates that the Env structure represents the currently running environment.
ENV_NOT_RUNNABLE:
	Indicates that the Env structure represents a currently active environment, but it is not currently ready to run: for example, because it is waiting for an interprocess communication (IPC) from another environment.
ENV_DYING:
	Indicates that the Env structure represents a zombie environment. A zombie environment will be freed the next time it traps to the kernel. We will not use this flag until Lab 4.
```

### Allocating the Environments Array

同上次一样，分配一个`sizeof(struct Env) * NENV`的空间给`envs`用于保存全部的进程信息。

```c
envs = boot_alloc(sizeof(struct Env) * NENV);
memset(envs, 0, sizeof(struct Env) * NENV);
```

并映射到线性地址`UENVS`上。查看`memorylayout`，其大小也是一个`PTSIZE`。

```c
boot_map_region(kern_pgdir, UENVS, PTSIZE, PADDR(envs), PTE_U | PTE_P);
```

### Creating and Running Environments

由于还没有`fs`，直接加载保存在`kern`中的`elf`

`env_init` 初始化`envs`，并加入到`freelist`里面。

```c
void env_init(void)
{
	int i;
	// Set up envs array
	envs[0].env_id = 0;
	envs[0].env_type = ENV_FREE;
	env_free_list = &envs[0];
	for (i = 1; i < NENV; i++)
	{
		envs[i - 1].env_link = &envs[i];
		envs[i].env_id = 0;
		envs[i].env_type = ENV_FREE;
	}
	envs[i].env_link = NULL;

	// Per-CPU part of the initialization
	env_init_percpu();
}
```



`env_setup_vm`则是重新建立这个进程的`pd`

其中高于`UTOP`的映射直接从`kern_pgdir`拷贝即可，保存着该`pd`的`PADDR(e->env_pgdir)`则又被映射到相同的`UVPT`下。

```c
static int
env_setup_vm(struct Env *e)
{
	int i;
	struct PageInfo *p = NULL;
	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;
	e->env_pgdir = page2kva(p);
	p->pp_ref++; 
	// The VA space of all envs is identical above UTOP
	for (i = PDX(UTOP); i < NPDENTRIES; i++)
		e->env_pgdir[i] = kern_pgdir[i];
	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;
	return 0;
}
```



`region_alloc`分配一些空间给进程使用。这里传进来的`va`与`len`最好都`ROUND`到`PGSIZE`，这样就不用要求传进来的必须是`PGSIZE`的倍数了。

```c
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	uintptr_t va_start, va_end, i;
	struct PageInfo *pp;
	va_start = ROUNDDOWN(va, PGSIZE);
	va_end = ROUNDUP(va + len, PGSIZE);
	for (i = va_start; i < va_end; i += PGSIZE)
	{
		if (!(pp = page_alloc(0)))
			panic("Region_alloc error:allocation attempt fails");
		page_insert(e->env_pgdir, pp, i, PTE_W | PTE_U);
	}
}
```



`load_icode`则是从传进来的地址中分析`ELF`头并载入到指定的内存

同`\boot\main.c`类似，唯一不同的是在拷贝内容的时候，需要先将`pd`转换成`env`的`pd`，之后又转换回来。

```c
static void
load_icode(struct Env *e, uint8_t *binary)
{
	struct Elf *ELFENV = (struct Elf *)binary;
	struct Proghdr *ph, *eph;
	if (ELFENV->e_magic != ELF_MAGIC)
		panic("load_icode error: invalid ELF");
	ph = (struct Proghdr *)((uint8_t *)ELFENV + ELFENV->e_phoff);
	eph = ph + ELFENV->e_phnum;
	// load env's pd
	lcr3(PADDR(e->env_pgdir));
	for (; ph < eph; ph++)
	{
		//  You should only load segments with ph->p_type == ELF_PROG_LOAD.
		//  Each segment's virtual address can be found in ph->p_va
		//  and its size in memory can be found in ph->p_memsz.
		if (ph->p_type == ELF_PROG_LOAD)
		{
			// In regin_alloc, pages allocated have been set User R/W
			region_alloc(e, (void *)ph->p_va, ph->p_memsz);
			//  The ph->p_filesz bytes from the ELF binary, starting at
			//  'binary + ph->p_offset', should be copied to virtual address
			//  ph->p_va.  Any remaining memory bytes should be cleared to zero.
			memset((void *)ph->p_va, 0, ph->p_memsz);
			memmove((void *)ph->p_va, (uint8_t *)ELFENV + ph->p_offset, ph->p_filesz);
		}
	}

	//  You must also do something with the program's entry point,
	//  to make sure that the environment starts executing there.
	//  What?  (See env_run() and env_pop_tf() below.)
	e->env_tf.tf_eip = ELFENV->e_entry;

	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.
	region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);

	// reload kern's pd
	lcr3(PADDR(kern_pgdir));
}
```



`env_create`则是创建一个`env`

```c
void env_create(uint8_t *binary, enum EnvType type)
{
	struct Env *e;
	int r;
	if ((r = env_alloc(&e, 0)))
		panic("env_alloc error: %e", r);
	e->env_type = type;
	e->env_parent_id = 0;
	load_icode(e, binary);
}
```



最后`env_run`则是先设置各种`env`的参数，最后加载保存的`reg`（包括`cs` `ip`）就可以直接接着运行`env`的代码了。

```c
void env_run(struct Env *e)
{
	// 	If this is a context switch (a new environment is running):
	//	Set the current environment (if any) back to ENV_RUNNABLE
	if (curenv == NULL && curenv != e)
	{
		if (curenv != NULL && curenv->env_status == ENV_RUNNING)
			curenv->env_status = ENV_RUNNABLE;
		//	Set 'curenv' to the new environment
		curenv = e;
		//	Set its status to ENV_RUNNING,
		e->env_status = ENV_RUNNING;
		e->env_runs++;
		// switch to its address space
		lcr3(PADDR(e->env_pgdir));
	}
	// eip has been set in load_icode
	// restore the environment's registers
	env_pop_tf(&e->env_tf);

	panic("env_run not yet implemented");
}

```



### Handling Interrupts and Exceptions

#### Basics of Protected Control Transfer

Interrupt vs Exception

>  In Intel's terminology, an *interrupt* is a protected control transfer that is caused by an asynchronous event usually external to the processor, such as notification of external device I/O activity. An *exception*, in contrast, is a protected control transfer caused synchronously by the currently running code, for example due to a divide by zero or an invalid memory access.

1. **The Interrupt Descriptor Table.**
2. **The Task State Segment**

>  Since "kernel mode" in JOS is privilege level 0 on the x86, the processor uses the `ESP0` and `SS0` fields of the TSS to define the kernel stack when entering kernel mode. JOS doesn't use any other TSS fields.

#### Nested Exceptions and Interrupts

> The processor can take exceptions and interrupts both from kernel and user mode. **It is only when entering the kernel from user mode, however, that the x86 processor automatically switches stacks before pushing its old register state onto the stack and invoking the appropriate exception handler through the IDT.**

### Setting Up the IDT

根据`trap.h`中的定义

```c
#define T_DIVIDE     0		// divide error
#define T_DEBUG      1		// debug exception
#define T_NMI        2		// non-maskable interrupt
#define T_BRKPT      3		// breakpoint
#define T_OFLOW      4		// overflow
#define T_BOUND      5		// bounds check
#define T_ILLOP      6		// illegal opcode
#define T_DEVICE     7		// device not available
#define T_DBLFLT     8		// double fault
/* #define T_COPROC  9 */	// reserved (not generated by recent processors)
#define T_TSS       10		// invalid task switch segment
#define T_SEGNP     11		// segment not present
#define T_STACK     12		// stack exception
#define T_GPFLT     13		// general protection fault
#define T_PGFLT     14		// page fault
/* #define T_RES    15 */	// reserved
#define T_FPERR     16		// floating point error
#define T_ALIGN     17		// aligment check
#define T_MCHK      18		// machine check
#define T_SIMDERR   19		// SIMD floating point error
```

与

```
Table 9-7. Error-Code Summary

Description                       Interrupt     Error Code
Number

Divide error                       0            No
Debug exceptions                   1            No
Breakpoint                         3            No
Overflow                           4            No
Bounds check                       5            No
Invalid opcode                     6            No
Coprocessor not available          7            No
System error                       8            Yes (always 0)
Coprocessor Segment Overrun        9            No
Invalid TSS                       10            Yes
Segment not present               11            Yes
Stack exception                   12            Yes
General protection fault          13            Yes
Page fault                        14            Yes
Coprocessor error                 16            No
Two-byte SW interrupt             0-255         No
```

生成各种`entry`

```c
TRAPHANDLER_NOEC( ENTRY_DIVIDE, T_DIVIDE) 	/* divide error */
TRAPHANDLER_NOEC( ENTRY_DEBUG, T_DEBUG)  	/* debug exception */
TRAPHANDLER_NOEC( ENTRY_NMI, T_NMI)  		/* non-maskable interrupt */
TRAPHANDLER_NOEC( ENTRY_BRKPT, T_BRKPT)  	/* breakpoint */
TRAPHANDLER_NOEC( ENTRY_OFLOW, T_OFLOW)  	/* overflow */
TRAPHANDLER_NOEC( ENTRY_BOUND, T_BOUND)  	/* bounds check */
TRAPHANDLER_NOEC( ENTRY_ILLOP, T_ILLOP)  	/* illegal opcode */
TRAPHANDLER_NOEC( ENTRY_DEVICE, T_DEVICE)  	/* device not available */
TRAPHANDLER     ( ENTRY_DBLFLT, T_DBLFLT)  	/* double fault */
/* T_COPROC */  							/* reserved */
TRAPHANDLER     ( ENTRY_TSS, T_TSS)  		/* invalid task switch segment */
TRAPHANDLER     ( ENTRY_SEGNP, T_SEGNP)  	/* segment not present */
TRAPHANDLER     ( ENTRY_STACK, T_STACK)  	/* stack exception */
TRAPHANDLER     ( ENTRY_GPFLT, T_GPFLT)  	/* general protection fault */
TRAPHANDLER     ( ENTRY_PGFLT, T_PGFLT)  	/* page fault */
/* T_RES */  								/* reserved */
TRAPHANDLER_NOEC( ENTRY_FPERR, T_FPERR)  	/* floating point error */
TRAPHANDLER_NOEC( ENTRY_ALIGN, T_ALIGN)  	/* aligment check */
TRAPHANDLER_NOEC( ENTRY_MCHK, T_MCHK)  		/* machine check */
TRAPHANDLER_NOEC( ENTRY_SIMDERR, T_SIMDERR) /* SIMD floating point error */
```

而对于一个`trapframe`

```c
struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	/* below here defined by x86 hardware */
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	/* below here only when crossing rings, such as from user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
} __attribute__((packed));
```

在`TRAPHANDLER`中已经压了`tf_trapno`，在`_alltraps`只用压`p2,ds,p1,es`，再用`pushal`压入其余寄存器。最后需要将`GD_KD`载入到`ds`与`es`。

```assembly
/*
 * _alltraps:
 * push values to make the stack look like a struct Trapframe
 * load GD_KD into %ds and %es
 * pushl %esp to pass a pointer to the Trapframe as an argument to trap()
 * call trap (can trap ever return?) nope
 */
_alltraps:
	pushw $0
  	pushw %ds
  	pushw $0
  	pushw %es
	/* %eax->%ecx->%edx->%ebx->%esp->%ebp->%esi->%edi */
  	pushal
  	pushw $GD_KD
	popw  %ds
	pushw $GD_KD
	popw  %es
  	pushl %esp
  	call trap
```

在`trap.h`中声明所有的`trap entry`

```c
extern void ENTRY_DIVIDE();     /* divide error */
extern void ENTRY_DEBUG();      /* debug exception */
extern void ENTRY_NMI();        /* non-maskable interrupt */
extern void ENTRY_BRKPT();      /* breakpoint */
...
```

最后在`trap_init`中设置`IDT`

> Descriptor Privilege Level: Gate call protection. Specifies which privilege Level the calling Descriptor minimum should have. So hardware and CPU interrupts can be protected from being called out of userspace.

> `INT`指令允许用户模式下的程序发送`Interrupt signal`（中断向量从0到255）。因此初始化IDT必须非常小心。要防止用户模式下的程序通过INT指令访问关键的事件处理程序。设置`Interrupt gate Descriptor`和`Trap gate Descriptor`中DPL的值为0后，当程序尝试发送某个`Interrupt signal`时，CPU会检查CPL和DPL的值，假如CPL的值比DPL的值高（也就是CPL的优先级比DPL的低），这会触发`“General protection” exception`。
>
> 当然在某些情况，用户模式下的程序必须能触发一个可编程的`exception`。因此，必须设置对应的`Interrupt gate Descriptor`和`Trap gate Descriptor`中DPL的值为3。
>
> Intel提供三种`interrupt descriptors：Task、Interrupt、Trap Gate Descriptors`。因为Linux没有使用到`Task GateDescriptors`。所以IDT中断向量表只包含`Interrupt Gate Descriptors`和`Trap Gate Descriptors`。Linux定义了如下概念，术语跟Intel稍微有些不同。

| Interrupt gate | 一个Intel Interrupt gate（这个gate的DPL的值是0）不能被用户模式下的程序访问。Linux Interrupt gate指向的事件处理程序(Linux interrupt handlers)只能在内核模式下运行。 |
| -------------- | ---------------------------------------- |
| System gate    | 一个Inter trap gate（这个gate的DPL的值是3）可以被用户模式下的程序访问。四个Linux exception handlers对应着中断向量3、4、5、128(system gate)。所以4个汇编指令int 3、 into、bound、 int $0x80可以在用户模式下执行。 |
| Trap gate      | 一个Intel trap gate(这个gate的DPL的值是0)不能被用户模式下的程序访问。大部分的Linux exceptionhandlers对应着Trap gate。 |

`T_OFLOW,T_BRKPT,T_BOUND` 允许软件中断 `dpl`需要设置为`3`

```c
	SETGATE(idt[T_DIVIDE], 0, GD_KT, ENTRY_DIVIDE, 0);
	SETGATE(idt[T_DEBUG], 0, GD_KT, ENTRY_DEBUG, 0);
	SETGATE(idt[T_NMI], 0, GD_KT, ENTRY_NMI, 0);
	SETGATE(idt[T_BRKPT], 0, GD_KT, ENTRY_BRKPT, 3);
	SETGATE(idt[T_OFLOW], 0, GD_KT, ENTRY_OFLOW, 3);
	SETGATE(idt[T_BOUND], 0, GD_KT, ENTRY_BOUND, 3);
	SETGATE(idt[T_ILLOP], 0, GD_KT, ENTRY_ILLOP, 0);
	SETGATE(idt[T_DEVICE], 0, GD_KT, ENTRY_DEVICE, 0);
	SETGATE(idt[T_DBLFLT], 0, GD_KT, ENTRY_DBLFLT, 0);
	SETGATE(idt[T_TSS], 0, GD_KT, ENTRY_TSS, 0);
	SETGATE(idt[T_SEGNP], 0, GD_KT, ENTRY_SEGNP, 0);
	SETGATE(idt[T_STACK], 0, GD_KT, ENTRY_STACK, 0);
	SETGATE(idt[T_GPFLT], 0, GD_KT, ENTRY_GPFLT, 0);
	SETGATE(idt[T_PGFLT], 0, GD_KT, ENTRY_PGFLT, 0);
	SETGATE(idt[T_FPERR], 0, GD_KT, ENTRY_FPERR, 0);
	SETGATE(idt[T_ALIGN], 0, GD_KT, ENTRY_ALIGN, 0);
	SETGATE(idt[T_MCHK], 0, GD_KT, ENTRY_MCHK, 0);
	SETGATE(idt[T_SIMDERR], 0, GD_KT, ENTRY_SIMDERR, 0);
```



> Challenge! You probably have a lot of very similar code right now, between the lists of `TRAPHANDLER` in `trapentry.S` and their installations in `trap.c`. Clean this up. Change the macros in `trapentry.S` to automatically generate a table for `trap.c` to use. Note that you can switch between laying down code and data in the assembler by using the directives `.text` and `.data`.

修改`TRAPHANDLER`与`TRAPHANDLER_NOEC`

```assembly
#define TRAPHANDLER(name, num)									\
	.text;														\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */				\
	name:			/* function starts here */					\
	pushl $(num);												\
	jmp _alltraps;												\
	.data;														\
	.long name;

#define TRAPHANDLER_NOEC(name, num)								\
	.text;														\
	.globl name;												\
	.type name, @function;										\
	.align 2;													\
	name:														\
	pushl $0;													\
	pushl $(num);												\
	jmp _alltraps;												\
	.data;														\
	.long name;
```

并将所有的`handler`映射到`traphandlers`的`table`下

```assembly
.data
.globl traphandlers
traphandlers:
TRAPHANDLER_NOEC(traphandler0, 0)
TRAPHANDLER_NOEC(traphandler1, 1)
TRAPHANDLER_NOEC(traphandler2, 2)
```

声明外部

```c
extern long traphandlers[];
```

则`trap_init`可以写为

```c
void trap_init(void)
{
	extern struct Segdesc gdt[];
	int i;
	int dpl = 0;
	for (i = 0; i < 17; i++)
	{
		dpl = (i == 3 || i == 4 || i == 5 || i == 128) ? 3 : 0;
		SETGATE(idt[i], 0, GD_KT, traphandlers[i], dpl);
	}
	// Per-CPU setup
	trap_init_percpu();
}
```

