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


## 文件系统 Overview

JOS文件系统是直接映射到内存空间`DISKMAP`到`DISKMAP + DISKSIZE`这块空间。故其支持的文件系统最大为3GB.

### IDE `ide.c`

文件系统底层`PIO`驱动放在`ide.c`中。注意在`IDE`中，是以硬件的角度来看待硬盘，其基本单位是`sector`，不是`block`。

- `bool	ide_probe_disk1(void)` 用于检测`disk1`是否存在。
- `voidide_set_disk(int diskno)` 用于设置目标磁盘。
- `ide_read ide_write` 用于磁盘读写。

### block cache `bc.c`

文件系统在内存中的映射是基于`block cache`的。以一个`block`为单位在内存中为其分配单元。注意在`bc`中，是以操作系统的角度来看待硬盘，其基本单位是`block`，不是`sector`。

- `void *diskaddr(uint32_t blockno)` 用于查找`blockno`在地址空间中的地址。

- `blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE` 用于查找`addr`对应文件系统中的`blockno`。

- `static void bc_pgfault(struct UTrapframe *utf)` 用于处理读取不在内存中而出现`page fault`的情况。这时需要从`file system`通过`PIO`读取到`block cache`(也就是内存中新分配的一页)中，并做好映射。

- `void flush_block(void *addr)` 用于写回硬盘，写回时清理`PTE_D`标记。

### file system `fs.c`

文件系统是基于刚才的`block cache`和底层`ide`驱动的。

#### bitmap 相关

`bitmap`每一位代表着一个`block`的状态，用位操作检查／设置`block`状态即可。

- `bool block_is_free(uint32_t blockno)` 用于check给定的`blockno`是否是空闲的。

- `void free_block(uint32_t blockno)` 设置对应位为0

- `int alloc_block(void)` 设置对应位为1

### 文件系统操作

- `void fs_init(void)` 初始化文件系统。检测`disk1`是否存在，检测`super block`和`bitmap block`。

- `static int file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)` 用于找到文件`f`的`fileno`个`block`的`blockno`。`alloc`用于控制当`f_indirect`不存在的时候，是否需要新申请一个`block`。

- `int file_get_block(struct File *f, uint32_t filebno, char **blk)` 用于找到文件`f`的`fileno`个`block`的地址。

- `static int dir_lookup(struct File *dir, const char *name, struct File **file)` 用于在`dir`下查找`name`这个文件。其遍历读取`dir`这个文件，并逐个判断其目录下每一个文件的名字是否相等。

- `static int dir_alloc_file(struct File *dir, struct File **file)` 在`dir`下新申请一个`file`。同样也是遍历所有的`dir`下的文件。找到第一个名字为空的文件，并把新的文件存在这里。

- `static int walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)` 用于从根目录获取`path`的文件，文件放在`pf`中，路径放在`pdir`中。如果找到了路径没有找到文件。最后的路径名放在`lastelem`中，最后的路径放在`pdir`中。


### 文件操作

- `int file_create(const char *path, struct File **pf)` 用于创建文件。

- `int file_open(const char *path, struct File **pf)` 打开文件。

- `ssize_t file_read(struct File *f, void *buf, size_t count, off_t offset)` 从`f`的`offset`读取`count`bytes的数据放入`buf`中。

- `int file_write(struct File *f, const void *buf, size_t count, off_t offset)` 与上面的类似。

- `static int file_free_block(struct File *f, uint32_t filebno)` 删除文件中的`filebno`

- `static void file_truncate_blocks(struct File *f, off_t newsize)` 缩短文件大小。

- `int file_set_size(struct File *f, off_t newsize)` 修改文件大小。

- `void file_flush(struct File *f)` 将文件写回硬盘

- `void fs_sync(void)` 将所有的文件写回硬盘

### 文件系统服务器 serv.c

- 服务器主要逻辑`umain`： 初始化文件系统，初始化服务器，开始接收请求。

- 服务器具体函数见上面实现。

- `int openfile_alloc(struct OpenFile **o)`用于服务器分配一个`openfile`结构体
  ​

### 文件描述符  fd.c

- `struct fd` 结构体

  ```c
  struct Fd {
  	int fd_dev_id;
  	off_t fd_offset;
  	int fd_omode;
  	union {
  		// File server files
  		struct FdFile fd_file;
  	};
  };
  ```
  其中`fd_file`用于发送的时候传入服务器对应的`fileid`


  包括了`fd_id` 文件读取的`offset`，读取模式以及`FdFile`

- `int fd2num(struct Fd *fd)` 从`fd`获取其编号

- `char* fd2data(struct Fd *fd)` 从`fd`获取文件内容

- `int fd_alloc(struct Fd **fd_store)` 查找到第一个空闲的`fd`,并分配出去。

- `int fd_lookup(int fdnum, struct Fd **fd_store)` 为查找`fdnum`的fd，并放在`fd_store`中。

- `int fd_close(struct Fd *fd, bool must_exist)` 用于关闭并free一个fd

- `int dev_lookup(int dev_id, struct Dev **dev)` 获取不同的Device

- `int close(int fdnum)` 关闭`fd`

- `void close_all(void)` 关闭全部

- `int dup(int oldfdnum, int newfdnum)` `dup`不是简单的复制，而是要将两个`fd`的内容完全同步，其是通过虚拟内存映射做到的。

- `read(int fdnum, void *buf, size_t n)` 后面的与这个类似
  - 获取`fd`的`fd_dev_id`并根据其获取`dev`
  - 调用`dev`对应的`function`

- `int seek(int fdnum, off_t offset)` 用于设置`fd`的`offset`

- `int fstat(int fdnum, struct Stat *stat)` 获取文件状态。

  ```c
  struct Stat
  {
  	char st_name[MAXNAMELEN];
  	off_t st_size;
  	int st_isdir;
  	struct Dev *st_dev;
  };
  ```

- ​`int stat(const char *path, struct Stat *stat)` 获取路径状态。


具体关于文件描述符的设计见下图。

![](./fd.png)

下面就来详细看现有的三个`device`

### 文件系统读写 file.c

- 之前已经分析过`devfile_xx`的函数

- `static int fsipc(unsigned type, void *dstva)`用于给文件系统服务器发送`IPC`

- 这里是实例化了一个用于文件读取的`dev`

  ```c
  struct Dev devfile =
  {
  	.dev_id = 'f',
  	.dev_name = "file",
  	.dev_read = devfile_read,
  	.dev_close = devfile_flush,
  	.dev_stat = devfile_stat,
  	.dev_write = devfile_write,
  	.dev_trunc = devfile_trunc
  };
  ```


### 管道 pipe.c

#### 关于`pipe`

管道是一种把两个进程之间的标准输入和标准输出连接起来的机制，从而提供一种让多个进程间通信的方法，当进程创建管道时，每次都需要提供两个文件描述符来操作管道。其中一个对管道进行写操作，另一个对管道进行读操作。对管道的读写与一般的IO系统函数一致，使用write()函数写入数据，使用read()读出数据。

同刚才的`file`的操作类似，这里是对于`pipe`的操作。

```c
struct Dev devpipe =
	{
		.dev_id = 'p',
		.dev_name = "pipe",
		.dev_read = devpipe_read,
		.dev_write = devpipe_write,
		.dev_close = devpipe_close,
		.dev_stat = devpipe_stat,
};
```
pipe 的结构体如下

```c
struct Pipe
{
	off_t p_rpos;			   // read position
	off_t p_wpos;			   // write position
	uint8_t p_buf[PIPEBUFSIZ]; // data buffer
};
```


- `int pipe(int pfd[2])` 申请两个新的`fd`，映射到同一个虚拟地址上，一边`Read_only` 一边`Write_only`即可。

- ​`static ssize_t devpipe_read(struct Fd *fd, void *vbuf, size_t n)` 其从`fd`对应的`data`获取`pipe`。`p = (struct Pipe *)fd2data(fd);`然后从`pipe->buf`中读取内容。维护`p_rpos`。

- `static ssize_t devpipe_write(struct Fd *fd, const void *vbuf, size_t n)` 其从`fd`对应的`data`获取`pipe`。`p = (struct Pipe *)fd2data(fd);`然后向`pipe->buf`中写入内容。维护`p_wpos`。

### 屏幕输入输出 console.c 

```c
struct Dev devcons =
	{
		.dev_id = 'c',
		.dev_name = "cons",
		.dev_read = devcons_read,
		.dev_write = devcons_write,
		.dev_close = devcons_close,
		.dev_stat = devcons_stat};
```

实现直接调用`syscall`即可，和之前实现的`putchar`类似。