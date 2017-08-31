# JOS Overview

## 工程结构 

```shell
.
├── GNUmakefile
├── boot
│   ├── Makefrag
│   ├── boot.S			# 初始化系统，开启保护模式
│   ├── main.c			# 读取ELF头，拷贝程序，开始执行
│   └── sign.pl
├── conf
│   ├── env.mk
│   └── lab.mk
├── fs
│   ├── Makefrag
│   ├── bc.c
│   ├── fs.c
│   ├── fs.h
│   ├── fsformat.c
│   ├── ide.c
│   ├── lorem
│   ├── motd
│   ├── newmotd
│   ├── script
│   ├── serv.c
│   ├── test.c
│   ├── testshell.key
│   └── testshell.sh
├── inc
│   ├── COPYRIGHT
│   ├── args.h
│   ├── assert.h
│   ├── elf.h
│   ├── env.h
│   ├── error.h
│   ├── fd.h
│   ├── fs.h
│   ├── kbdreg.h
│   ├── lib.h
│   ├── memlayout.h
│   ├── mmu.h
│   ├── partition.h
│   ├── stab.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── string.h
│   ├── syscall.h
│   ├── trap.h
│   ├── types.h
│   └── x86.h
├── kern
│   ├── COPYRIGHT
│   ├── Makefrag
│   ├── console.c
│   ├── console.h
│   ├── cpu.h
│   ├── entry.S
│   ├── entrypgdir.c
│   ├── env.c
│   ├── env.h
│   ├── init.c
│   ├── kclock.c
│   ├── kclock.h
│   ├── kdebug.c
│   ├── kdebug.h
│   ├── kernel.ld
│   ├── lapic.c
│   ├── monitor.c
│   ├── monitor.h
│   ├── mpconfig.c
│   ├── mpentry.S
│   ├── picirq.c
│   ├── picirq.h
│   ├── pmap.c
│   ├── pmap.h
│   ├── printf.c
│   ├── sched.c
│   ├── sched.h
│   ├── spinlock.c
│   ├── spinlock.h
│   ├── syscall.c
│   ├── syscall.h
│   ├── trap.c
│   ├── trap.h
│   └── trapentry.S
├── lib
│   ├── Makefrag
│   ├── args.c
│   ├── console.c
│   ├── entry.S
│   ├── exit.c
│   ├── fd.c
│   ├── file.c
│   ├── fork.c
│   ├── fprintf.c
│   ├── ipc.c
│   ├── libmain.c
│   ├── pageref.c
│   ├── panic.c
│   ├── pfentry.S
│   ├── pgfault.c
│   ├── pipe.c
│   ├── printf.c
│   ├── printfmt.c
│   ├── readline.c
│   ├── spawn.c
│   ├── string.c
│   ├── syscall.c
│   └── wait.c
├── mergedep.pl
```



## kern

```shell
./kern
├── COPYRIGHT
├── Makefrag
├── console.c						# 控制台操作抽象
├── console.h
├── cpu.h							# 多CPU支持
├── entry.S
├── entrypgdir.c
├── env.c
├── env.h
├── init.c							# kern 主函数 调用初始化系统，唤醒多核系统并初始化
├── kclock.c
├── kclock.h
├── kdebug.c
├── kdebug.h
├── kernel.ld
├── lapic.c
├── monitor.c
├── monitor.h
├── mpconfig.c						# 读取多核信息
├── mpentry.S
├── picirq.c
├── picirq.h
├── pmap.c
├── pmap.h
├── printf.c
├── sched.c
├── sched.h
├── spinlock.c
├── spinlock.h
├── syscall.c
├── syscall.h
├── trap.c
├── trap.h
└── trapentry.S
```



### init

```c
void i386_init(void);			// 内核初始化主函数
static void boot_aps(void);		// 唤醒其他核
void mp_main(void);				// 非boot核初始化入口
void _warn(const char *file, int line, const char *fmt, 
           ...);				// 不进入monitor的panic 版本
```

其中多核部分详细分析见`/lab_report/lab4 partA`。



### console

```c
void cons_init(void);
int cons_getc(void);
void kbd_intr(void); 		// irq 1
void serial_intr(void); 	// irq 4
```

`console.c`与`console.h`主要是对于具体设备驱动的包装。

详见`/lab_report/lab1 Part3`。



### cpu mpconfig 

```c
extern struct CpuInfo cpus[NCPU];
extern int ncpu;
extern struct CpuInfo *bootcpu;
extern physaddr_t lapicaddr;
extern unsigned char percpu_kstacks[NCPU][KSTKSIZE];
int cpunum(void);
void mp_init(void);			// 读取多核信息
void lapic_init(void);		// 核间中断获取
void lapic_startap(uint8_t apicid, uint32_t addr);	// 
void lapic_eoi(void);
void lapic_ipi(int vector);
```

这几个文件均与多核支持有关。

与多核有关的信息包含在`MP Floating Pointer Structure`中，`mpconfig`文件中主要任务就是从存储器中读出这些信息，判断有多少可以用的`AP`，保存在`cpus`的结构体中，找出`boot cpu`。



