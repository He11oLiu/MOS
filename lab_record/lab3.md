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