# 被动读写锁的理解与实现

本文主要为读论文[*Scalable Read-mostly Synchronization Using Passive Reader-Writer Locks*](http://ipads.se.sjtu.edu.cn/lib/exe/fetch.php?media=publications:atc14-paper-liu.pdf)的记录，并且尝试将其在`JOS`上实现。

## 论文阅读记录

### 研究背景

- 单核性能提升遇到瓶颈 转向多核提升性能
- 单核主要为计算密集型模型，多核主要为并行化模型
- 并行化模型面临的问题：多个处理器共享数据结构，需要同步原语来保证其一致性
- 更底层：内存一致性 然而会导致可扩展性问题
  - Strict Consistency 		Memory Barrier
  - Sequential Consistency      TSO

### 可扩展性 Scalable

提到可扩展性，不得不提`Amdahl's law`

```
S(latency)(s) = 1/((1-p)+p/s)
```

其中`1-p`极为不可以并行的部分，而对于一个处理器，造成(1-P)部分有以下几种原因：

- 内存屏障时等待之前的指令执行完
- MESI模型中等待获取最新值
- 等待其他处理器释放锁
- 多核之间的通讯带宽受限，阻塞

###关于读写锁

- 读读不阻塞，读写阻塞
- 适合读数据频率远大于写数据频率的应用
- 单核上的实现思路：读者锁不冲突，当需要加写者锁的时候需要等所有的读者锁释放。利用一个读者计数器来实现。
- 多核上最直观的实现思路：每个核上保存自己的读者锁，写者需要等到所有的读者锁全部释放了才能开始获取锁。

### 现有的`RWLock`所做的尝试

#### BRLOCK C-SNZI

- 读者申请自己核上的锁
- 当只有读者时，由于只是访问自己的核上的锁，所以有良好的扩展性
- 写者需要获取所有核上的互斥锁，恒定延迟，与处理器数量有关。
- SNZI利用树进行了一定优化

#### RCU

- 弱化语义，可能会读到脏的数据（逻辑矛盾）
- 读者无锁，写着读者可以同时进行
- 先写值再改指针
- 写者开销大，要处理旧的数据
- 垃圾回收(无抢占调度一圈)

![](./7.png)





### Bounded staleness 短内存可见性

所谓短内存可见性，也就是在很短的时间周期内，由于每个核上面的单独`cache`非常的小，很大几率会被替换掉，从而能看到最新的数据。下面是具体的图表

![](./8.png)



### PRWLock的设计思路

- 在短时间内各个处理器均可以看到最新的版本号
- 利用TSO架构的优势，版本控制隐式表示退出CS区域
- 并不完全依赖于短时间可见，对于特殊情况，保证一致性，利用IPI要求进行Report IPI消息传递开销较小，且可以相互掩盖。
- 两种模式 支持调度（睡眠与抢占）

### PRWLock 的两种模式

#### Passive Mode

- 用于处理没有经过调度的读者
- 共享内存陈旧化
- 弱控制，通过版本控制隐式反馈+少数情况IPI

#### Active Mode (用于支持睡眠与调度类似BRLock)

- 用于处理被调度过的进程（睡眠／抢占）
- 通过维护active检测数量
- 强控制，主动监听
- 主动等待

###PRWLock流程视频：

[优酷视频](http://v.youku.com/v_show/id_XMjk5MzQ1NzE2NA==.html?spm=a2hzp.8244740.0.0)



### PRWLock的正确性 

- 写者发现读者获取了最新的版本变量时，由于TSO的特性，也一定看到了写者上的写锁，确信其不会再次进入临界区。
- 对于需要较长时间才能看到最新的版本号或没有读者期望获取读者锁提供了IPI来降低等待时间，避免无限等待。



### PRWLock 内核中减少IPIs

- 锁域(Lock Domain)用于表示一个锁对应的处理器范围
- 若上下文切换到了其他的进程，就没必要管这个核的锁了
- 锁域的上下线可以避免一些没有必要的一致性检测
- 注意利用内存屏障来保证一致性



### PRWLock 用户态实现

由于在用户态有以下两个特点

- 用户态不能随时关闭抢占(Preemption)
- 用户态不能发送核间中断(IPI)

所以`PRWLock`在用户态实现的思路如下：

- 利用抢断标记位避免特殊窗口时被抢断
- 写者必须陷入内核态发送IPI



### PRWLock 性能分析

#### 读者

- 读者之间无内存屏障 （无关联）
- 锁域上下线本来就是极少的操作，用来改善性能的，所以其中的内存屏障影响不大
- 对于长CS区的读者，与传统一样

#### 写者

- IPI只要几百个cycle 本身也要等待
- 多个写者可以直接把锁传递



### 总结

- 利用了短内存写全局可见时间
- 利用了TSO的特性设计的版本控制来隐式维护语义
- 利用IPI来保证特殊情况
- 利用两种模式支持调度
- 读者之间无关联（内存屏障），提升读者性能
- PWAKE 分布式唤醒，提高了唤醒并行性





## 移植`PRWLock`到`JOS`

### `JOS`的核间中断实现

#### 关于`Local APIC`

> 在一个基于`APIC`的系统中，每一个核心都有一个`Local APIC`，`Local APIC`负责处理`CPU`中特定的中断配置。还有其他事情，它包含了`Local Vector Table(LVT)`负责配置事件中断。
>
> 此外，还有一个CPU外面的`IO APIC`(例如`Intel82093AA`）的芯片组，并且提供基于多处理器的中断管理，在多个处理器之间，实现静态或动态的中断触发路由。
>
> `Inter-Processor Interrupts（IPIs)`是一种由`Local APIC`触发的中断，一般可以用于多CPU间调度之类的使用
>
> 想要开启`Local APIC`接收中断，则需要设置`Spurious Interrupt Vector Register`的第8位即可。
>
> 使用`APIC Timer`的最大好处是每个cpu内核都有一个定时器。相反`PIT(Programmable Interval Timer)`就不这样，`PIT`是共用的一个。
>
> - 周期触发模式
>
>   周期触发模式中，程序设置一个”初始计数“寄存器（Initial Count），同时Local APIC会将这个数复制到”当前计数“寄存器（Current Count）。Local APIC会将这个数（当前计数）递减，直到减到0为止，这时候将触发一个IRQ（可以理解为触发一次中断），与此同时将当前计数恢复到初始计数寄存器的值，然后周而复始的执行上述逻辑。可以使用这种方法通过Local APIC实现定时按照一定时间间隔触发中断的功能。
>
> - 一次性触发模式
>
>   同之前一样，但是不会恢复到初始计数。
>
> - `TSC-Deadline Modie`
>
>   `cpu`的时间戳到达`deadline`的时候会触发`IRQ`
>
> [来源Blog](http://blog.csdn.net/borisjineman/article/details/51050049)

> 每个本地`APIC`都有 32 位的寄存器，一个内部时钟，一个本地定时设备以及为本地中断保留的两条额外的 IRQ 线 LINT0 和 LINT1。所有本地 APIC 都连接到 I/O APIC，形成一个多级 APIC 系统。
>
> Intel x86架构提供LINT0和LINT1两个中断引脚，他们通常与Local APIC相连，用于接收Local APIC传递的中断信号，另外，当Local APIC被禁用的时候，LINT0和LINT1即被配置为INTR和NMI管脚，即外部IO中断管脚和非屏蔽中断管脚。
>
> [来源博客](http://rock3.info/blog/tag/apic/)

![](./ioapic.gif)



> The local APIC registers are memory mapped to an address that can be found in the MP/MADT tables. Make sure you map these to virtual memory if you are using paging. Each register is 32 bits long, and expects to written and read as a 32 bit integer. Although each register is 4 bytes, they are all aligned on a 16 byte boundary. 

再一次详细查看`JOS`中的核间中断的实现方式，

由于这段映射，设置了`nocache`和直写的特性，便于对于`IO`的操作。

```c
void lapic_init(void)
{
	if (!lapicaddr)
		return;
	// lapicaddr is the physical address of the LAPIC's 4K MMIO
	// region.  Map it in to virtual memory so we can access it.
	lapic = mmio_map_region(lapicaddr, 4096);
	// Enable local APIC; set spurious interrupt vector.
	lapicw(SVR, ENABLE | (IRQ_OFFSET + IRQ_SPURIOUS));

	// The timer repeatedly counts down at bus frequency
	// from lapic[TICR] and then issues an interrupt.
	// If we cared more about precise timekeeping,
	// TICR would be calibrated using an external time source.
	lapicw(TDCR, X1);
	lapicw(TIMER, PERIODIC | (IRQ_OFFSET + IRQ_TIMER));
	lapicw(TICR, 10000000);

	// Leave LINT0 of the BSP enabled so that it can get
	// interrupts from the 8259A chip.
	//
	// According to Intel MP Specification, the BIOS should initialize
	// BSP's local APIC in Virtual Wire Mode, in which 8259A's
	// INTR is virtually connected to BSP's LINTIN0. In this mode,
	// we do not need to program the IOAPIC.
	if (thiscpu != bootcpu)
		lapicw(LINT0, MASKED);

	// Disable NMI (LINT1) on all CPUs
	lapicw(LINT1, MASKED);

	// Disable performance counter overflow interrupts
	// on machines that provide that interrupt entry.
	if (((lapic[VER] >> 16) & 0xFF) >= 4)
		lapicw(PCINT, MASKED);

	// Map error interrupt to IRQ_ERROR.
	lapicw(ERROR, IRQ_OFFSET + IRQ_ERROR);

	// Clear error status register (requires back-to-back writes).
	lapicw(ESR, 0);
	lapicw(ESR, 0);

	// Ack any outstanding interrupts.
	lapicw(EOI, 0);

	// Send an Init Level De-Assert to synchronize arbitration ID's.
	lapicw(ICRHI, 0);
	lapicw(ICRLO, BCAST | INIT | LEVEL);
	while (lapic[ICRLO] & DELIVS)
		;

	// Enable interrupts on the APIC (but not on the processor).
	lapicw(TPR, 0);
}
```

`lapic_init`将`LAPIC`映射到`lapicaddr`地址上，并且初始化`LAPIC`各种中断参数。

```c
// Local APIC registers, divided by 4 for use as uint32_t[] indices.
#define ID (0x0020 / 4)	// ID
```

这里的宏定义为`/4`是因为`MMIO`映射到`MMIOaddr`，保存在`volatile uint32_t *lapic;`中。这个单位是`uint32_t`，故所有的地址均`/4`

下面来看一下主要的`APIC Registers`

- EOI Register

  Write to the register with offset 0xB0 using the value 0 to signal an end of interrupt. A non-zero values causes a general protection fault.

  ```c
  #define EOI (0x00B0 / 4)   // EOI
  // Acknowledge interrupt.
  void lapic_eoi(void)
  {
  	if (lapic)
  		lapicw(EOI, 0);
  }
  ```

- Local Vector Table Registers

  There are some special interrupts that the processor and LAPIC can generate themselves. While external interrupts are configured in the I/O APIC, these interrupts must be configured using registers in the LAPIC. The most interesting registers are: 

  `0x320 = lapic timer`

  `0x350 = lint0`

  `0x360 = lint1`

  See the Intel SDM vol 3 for more info.

  | Bits 0-7                      | The vector number                    |
  | ----------------------------- | ------------------------------------ |
  | Bit 8-11 (reserved for timer) | 100b if NMI                          |
  | Bit 12                        | Set if interrupt pending.            |
  | Bit 13 (reserved for timer)   | Polarity, set is low triggered       |
  | Bit 14 (reserved for timer)   | Remote IRR                           |
  | Bit 15 (reserved for timer)   | trigger mode, set is level triggered |
  | Bit 16                        | Set to mask                          |
  | Bits 17-31                    | Reserved                             |

  `JOS`在这里只保留了`BSP`的`LINT0`用于接受`8259A`的中断，其他的`LINT0`与`LINT1`非屏蔽中断，均设置为`MASKED`

  ```c
  	// Leave LINT0 of the BSP enabled so that it can get
  	// interrupts from the 8259A chip.
  	//
  	// According to Intel MP Specification, the BIOS should initialize
  	// BSP's local APIC in Virtual Wire Mode, in which 8259A's
  	// INTR is virtually connected to BSP's LINTIN0. In this mode,
  	// we do not need to program the IOAPIC.
  	if (thiscpu != bootcpu)
  		lapicw(LINT0, MASKED);

  	// Disable NMI (LINT1) on all CPUs
  	lapicw(LINT1, MASKED);
  ```

- Spurious Interrupt Vector Register

  The offset is 0xF0. The low byte contains the number of the spurious interrupt. As noted above, you should probably set this to 0xFF. To enable the APIC, set bit 8 (or 0x100) of this register. If bit 12 is set then EOI messages will not be broadcast. All the other bits are currently reserved.

  ```c
  	// Enable local APIC; set spurious interrupt vector.
  	lapicw(SVR, ENABLE | (IRQ_OFFSET + IRQ_SPURIOUS));
  ```

- Interrupt Command Register

  The interrupt command register is made of two 32-bit registers; one at 0x300 and the other at 0x310. 

  ```c
  #define ICRHI (0x0310 / 4)  // Interrupt Command [63:32]
  #define ICRLO (0x0300 / 4) 	// Interrupt Command [31:0]
  ```

  It is used for sending interrupts to different processors. 

  The interrupt is issued when 0x300 is written to, but not when 0x310 is written to. Thus, to send an interrupt command one should first write to 0x310, then to 0x300.

  需要先写`ICRHI`，然后在写`ICRLO`的时候就会产生中断。

  At 0x310 there is one field at bits 24-27, which is local APIC ID of the target processor (for a physical destination mode).

  ```c
  lapicw(ICRHI, apicid << 24);
  ```

  给`ICRHI`中断目标核心的`local APIC ID`。这里的`apicid`是在`MP Floating Pointer Structure`读的时候顺序给的`cpu_id`。

  Here is how 0x300 is structured:

  | Bits 0-7   | The vector number, or starting page number for SIPIs |
  | ---------- | ---------------------------------------- |
  | Bits 8-10  | The destination mode. 0 is normal, 1 is lowest priority, 2 is SMI, 4 is NMI, 5 can be INIT or INIT level de-assert, 6 is a SIPI. |
  | Bit 11     | The destination mode. Clear for a physical destination, or set for a logical destination. If the bit is clear, then the destination field in 0x310 is treated normally. |
  | Bit 12     | Delivery status. Cleared when the interrupt has been accepted by the target. You should usually wait until this bit clears after sending an interrupt. |
  | Bit 13     | Reserved                                 |
  | Bit 14     | Clear for INIT level de-assert, otherwise set. |
  | Bit 15     | Set for INIT level de-assert, otherwise clear. |
  | Bits 18-19 | Destination type. If this is > 0 then the destination field in 0x310 is ignored. 1 will always send the interrupt to the itself, 2 will send it to all processors, and 3 will send it to all processors aside from the current one. It is best to avoid using modes 1, 2 and 3, and stick with 0. |
  | Bits 20-31 | Reserved                                 |

  **ICRLO**的分布比较重要

  - 其中目标模式有(`8-10`)

    ```c
    #define INIT 0x00000500		// INIT/RESET
    #define STARTUP 0x00000600 	// Startup IPI
    ```


  - 其中发送模式有(`18~19`)

    ```c
    #define SELF  0x00040000  // Send to self
    #define BCAST 0x00080000  // Send to all APICs, including self.
    #define OTHERS 0x000C0000 // Send to all APICs, excluding self.
    ```

    不设置的话则为发送给`0x310 ICRHI`制定的核心。

综上，打包了一个`IPI`发送的接口，

```c
void lapic_ipi(int vector)
{
	lapicw(ICRLO, OTHERS | FIXED | vector);
	while (lapic[ICRLO] & DELIVS)
		;
}
```

用于发送`IPI`与`IPI ACK`均是利用MMIO直接对相应地址书写，比较简单。

这里测试一下，先设置`trap`中的`IPI`中断

```c
#define T_PRWIPI	20		// IPI report for PRWLock
void prw_ipi_report(struct Trapframe *tf)
{
	cprintf("%d in ipi report\n",cpunum());
}
```

在`trap_dispatch`中加入对这个中断的分发

```c
	case T_PRWIPI:
		prw_ipi_report(tf);
		break;
```

最后在`init`的时候用`bsp`发送`IPI`给所有其他核心

```c
	lapic_ipi(T_PRWIPI);
```

设置`QEMU`模拟4个核心来测试`IPI`是否正确

```shell
1 in ipi report
3 in ipi report
2 in ipi report
```

非`BSP`可以正确的接受`IPI`并进入中断处理历程。

针对向具体核发送`IPI`，抽象函数`lapic_ipi_dest`

```c
void lapic_ipi_dest(int dest, int vector)
{
	lapicw(ICRHI, dest << 24);
	lapicw(ICRLO, FIXED | vector);
	while (lapic[ICRLO] & DELIVS)
		;
}
```






### `JOS`实现传统内核态读写锁

```c
typedef struct dumbrwlock {
	struct spinlock lock;
    atomic_t readers;
}dumbrwlock;

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
            asm volatile("pause");
    }
}

void dumb_rdunlock(dumbrwlock *rwlk)
{
    atomic_dec(&rwlk->readers);
}
```

然后发现一个比较大的问题，`JOS`没有实现原子操作，先实现原子操作再进行下面的尝试。

### `JOS` 实现原子操作

仿造`linux 2.6`内核，实现原子操作

```c
#ifndef JOS_INC_ATOMIC_H_
#define JOS_INC_ATOMIC_H_

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

#include <inc/types.h>

#define LOCK "lock ; "

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */
typedef struct
{
    volatile int counter;
} atomic_t;

#define ATOMIC_INIT(i) \
    {                  \
        (i)            \
    }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically reads the value of @v.
 */
#define atomic_read(v) ((v)->counter)

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 * 
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v, i) (((v)->counter) = (i))

/**
 * atomic_add - add integer to atomic variable
 * @i: integer value to add
 * @v: pointer of type atomic_t
 * 
 * Atomically adds @i to @v.
 */
static __inline__ void atomic_add(int i, atomic_t *v)
{
    __asm__ __volatile__(
        LOCK "addl %1,%0"
        : "=m"(v->counter)
        : "ir"(i), "m"(v->counter));
}

/**
 * atomic_sub - subtract the atomic variable
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v.
 */
static __inline__ void atomic_sub(int i, atomic_t *v)
{
    __asm__ __volatile__(
        LOCK "subl %1,%0"
        : "=m"(v->counter)
        : "ir"(i), "m"(v->counter));
}

/**
 * atomic_sub_and_test - subtract value from variable and test result
 * @i: integer value to subtract
 * @v: pointer of type atomic_t
 * 
 * Atomically subtracts @i from @v and returns
 * true if the result is zero, or false for all
 * other cases.
 */
static __inline__ int atomic_sub_and_test(int i, atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
        LOCK "subl %2,%0; sete %1"
        : "=m"(v->counter), "=qm"(c)
        : "ir"(i), "m"(v->counter)
        : "memory");
    return c;
}

/**
 * atomic_inc - increment atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1.
 */
static __inline__ void atomic_inc(atomic_t *v)
{
    __asm__ __volatile__(
        LOCK "incl %0"
        : "=m"(v->counter)
        : "m"(v->counter));
}

/**
 * atomic_dec - decrement atomic variable
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1.
 */
static __inline__ void atomic_dec(atomic_t *v)
{
    __asm__ __volatile__(
        LOCK "decl %0"
        : "=m"(v->counter)
        : "m"(v->counter));
}

/**
 * atomic_dec_and_test - decrement and test
 * @v: pointer of type atomic_t
 * 
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, or false for all other
 * cases.
 */
static __inline__ int atomic_dec_and_test(atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
        LOCK "decl %0; sete %1"
        : "=m"(v->counter), "=qm"(c)
        : "m"(v->counter)
        : "memory");
    return c != 0;
}

/**
 * atomic_inc_and_test - increment and test 
 * @v: pointer of type atomic_t
 * 
 * Atomically increments @v by 1
 * and returns true if the result is zero, or false for all
 * other cases.
 */
static __inline__ int atomic_inc_and_test(atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
        LOCK "incl %0; sete %1"
        : "=m"(v->counter), "=qm"(c)
        : "m"(v->counter)
        : "memory");
    return c != 0;
}

/**
 * atomic_add_negative - add and test if negative
 * @v: pointer of type atomic_t
 * @i: integer value to add
 * 
 * Atomically adds @i to @v and returns true
 * if the result is negative, or false when
 * result is greater than or equal to zero.
 */
static __inline__ int atomic_add_negative(int i, atomic_t *v)
{
    unsigned char c;

    __asm__ __volatile__(
        LOCK "addl %2,%0; sets %1"
        : "=m"(v->counter), "=qm"(c)
        : "ir"(i), "m"(v->counter)
        : "memory");
    return c;
}

/**
 * atomic_add_return - add and return
 * @v: pointer of type atomic_t
 * @i: integer value to add
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __inline__ int atomic_add_return(int i, atomic_t *v)
{
    int __i;
    /* Modern 486+ processor */
    __i = i;
    __asm__ __volatile__(
        LOCK "xaddl %0, %1;"
        : "=r"(i)
        : "m"(v->counter), "0"(i));
    return i + __i;
}

static __inline__ int atomic_sub_return(int i, atomic_t *v)
{
    return atomic_add_return(-i, v);
}

#define atomic_inc_return(v) (atomic_add_return(1, v))
#define atomic_dec_return(v) (atomic_sub_return(1, v))

/* These are x86-specific, used by some header files */
#define atomic_clear_mask(mask, addr)               \
    __asm__ __volatile__(LOCK "andl %0,%1"          \
                         :                          \
                         : "r"(~(mask)), "m"(*addr) \
                         : "memory")

#define atomic_set_mask(mask, addr)                \
    __asm__ __volatile__(LOCK "orl %0,%1"          \
                         :                         \
                         : "r"(mask), "m"(*(addr)) \
                         : "memory")

#endif
```

然后在内核中对读写锁的功能进行测试。

**遇到两个问题**

- 一个是` asm volatile("pause");`容易死在那个循环里面，不会重新换到这个`CPU`中，在`DEBUG`的时候发现在前后加上`cprintf`其就会顺利换回来。

  ```c
          while (rwlk->lock.locked)
          {
              cprintf("");
              asm volatile("pause");
          }
  ```

- 另一个是设计内核中的测试

  - 多核上的输出可能会并行化，要减短输出内容。
  - 在用户空间的锁分享目前不好做，linux是基于文件的。
  - 故设计了两个锁来进行测试

  一个是`CPU 0`的`writer`锁，一个是`reader`锁。

  ```c
  	// test reader-writer lock
  	rw_initlock(&lock1);
  	rw_initlock(&lock2);

  	dumb_wrlock(&lock1);
  	cprintf("[rw] CPU %d gain writer lock1\n", cpunum());
  	dumb_rdlock(&lock2);
  	cprintf("[rw] CPU %d gain reader lock2\n", cpunum());

  	// Starting non-boot CPUs
  	boot_aps();

  	cprintf("[rw] CPU %d going to release writer lock1\n", cpunum());
  	dumb_wrunlock(&lock1);	
  	cprintf("[rw] CPU %d going to release reader lock2\n", cpunum());
  	dumb_rdunlock(&lock2);
  ```

  对于每个核上，分别获取`lock1`的读着锁与`lock2`的写者锁。添加`asm volatile("pause");`是想让其他核模拟上线来检测各种情况。

  ```c
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
  ```

  在给`QEMU`四核参数`CPUS=4`的时候下的运行情况如下：

```shell
[rw] CPU 0 gain writer lock1
[rw] CPU 0 gain reader lock2
[MP] CPU 1 starting
[MP] CPU 2 starting
[MP] CPU 3 starting
[rw] CPU 0 going to release writer lock1
[rw] CPU 0 going to release reader lock2
[rw] 1 l1
[rw] 2 l1
[rw] 3 l1
[rw] 2 unl1
[rw] 2 l2
[rw] 3 unl1
[rw] 1 unl1
[rw] 2 unl2
[MP] CPU 2 sched
[rw] 3 l2
[rw] 3 unl2
[rw] 1 l2
[MP] CPU 3 sched
[rw] 1 unl2
[MP] CPU 1 sched
```

可以观察到一旦`CPU0`释放了`lock1`的写者锁，所有的核均可以获得`lock1`的读者锁。而后`CPU2`获得了`lock2`的写者锁后，其他核上线，`CPU3`与`CPU1`只是释放了`lock1`，无法获得`lock2`，只有等`CPU2`释放了`lock2`才能获取。

这与期望的读写锁的功能是一致的。至此普通读写锁的实现完成。

### `JOS`实现`PRWLock`

