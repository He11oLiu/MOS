# Lab 5: File system, Spawn and Shell

###File system preliminaries

> Our file system therefore **does not support the UNIX notions of file ownership or permissions. Our file system also currently does not support hard links, symbolic links, time stamps, or special device files like most UNIX file systems do.**

### On-Disk File System Structure

一般的`fs`一般包括两个`region`：`inode`和`data`

在`UNIX`中，一个`file`有一个 `inode`与之对应。

> A file's inode holds critical meta-data about the file such as its `stat` attributes and pointers to its data blocks.In JOS, sectors are 512 bytes each. File systems actually allocate and use disk storage in units of *blocks*. 

 有两个概念需要明确：

> - *sector size* is a property of the disk hardware
>
> - *block size* is an aspect of the operating system using the disk
>
>   **A file system's block size must be a multiple of the sector size of the underlying disk.**
>
>   Our file system will use a **block size of 4096 bytes**, conveniently matching the processor's page size.

### Superblocks

![](./img/disk.png)

Our file system will have exactly one superblock, which will always be at block 1 on the disk. Its layout is defined by `struct Super` in `inc/fs.h`.

```c
// File system super-block (both in-memory and on-disk)

#define FS_MAGIC	0x4A0530AE	// related vaguely to 'J\0S!'

struct Super {
	uint32_t s_magic;		// Magic number: FS_MAGIC
	uint32_t s_nblocks;		// Total number of blocks on disk
	struct File s_root;		// Root directory node
};
```

一般的`fs`一般会在磁盘上保存多个`superblock`，以防中间的一个`superblock`出错了之后，可以去其他地方查找到这个`superblock`，用来恢复。

### File Meta-data

The layout of the meta-data describing a file in our file system is described by `struct File` in `inc/fs.h`. This meta-data includes the file's name, size, type (regular file or directory), and pointers to the blocks comprising the file. As mentioned above, we do not have inodes, so this meta-data is stored in a **directory entry on disk**. Unlike in most "real" file systems, for simplicity we will use this one `File` structure to represent file meta-data as it appears *both on disk and in memory*.

![](./img/file.png)



```c
// Number of block pointers in a File descriptor
#define NDIRECT		10

struct File {
	char f_name[MAXNAMELEN];	// filename
	off_t f_size;			// file size in bytes
	uint32_t f_type;		// file type

	// Block pointers.
	// A block is allocated iff its value is != 0.
	uint32_t f_direct[NDIRECT];	// direct blocks
	uint32_t f_indirect;		// indirect block

	// Pad out to 256 bytes; must do arithmetic in case we're compiling
	// fsformat on a 64-bit machine.
	uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4];
} __attribute__((packed));	// required only on some 64-bit machines
```

其中的`f_direct`数组保存了文件的最开头的`10`个`blocks`，which we call the file's *direct* blocks.

对于小于`40kb`的文件，这就表示了这个结构体可以直接表示其所有的块。

对于更大的文件，我们另外分配了一个`block`来表示，称为`indirect block`，可以存放${4096}/{4}=1024$个`block pointer`，所以可以保存1023个blocks，稍微大于`4M`的文件。真正的系统，对于更大的文件，通常还有双重，甚至三重`indirect blocks`。

### Directories versus Regular Files

一个`file`在`JOS`的系统中可以分为

- 普通的文件
- 文件目录

这两种文件是通过`type`来进行区分的。

> The superblock in our file system contains a `File` structure (the `root` field in `struct Super`) that holds the meta-data for the file system's root directory.
>
> The contents of this directory-file is a sequence of `File` structures describing the files and directories located within the root directory of the file system



### Disk Access

> Giving **"I/O privilege"** to the file system environment is the only thing we need to do in order to **allow the file system to access these registers**.

所以内核可以通过控制`EFLAGS`里面的`IOPL`bits来一次性决定是否给用户权限获取`I/O`

####Exercise 1

 `i386_init` identifies the file system environment by passing the type `ENV_TYPE_FS` to your environment creation function, `env_create`. Modify `env_create`in `env.c`, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.

只用在`env_create`的时候加入对`type`的判断，并修改对应的`eflags`位即可

```c
	// If this is the file server (type == ENV_TYPE_FS) give it I/O privileges.
	if (type == ENV_TYPE_FS)
		e->env_tf.tf_eflags|=FL_IOPL_MASK;
```

> Challenge!
>
> Implement interrupt-driven IDE disk access, with or without DMA. You can decide whether to move the device driver into the kernel, keep it in user space along with the file system, or even (if you really want to get into the micro-kernel spirit) move it into a separate environment of its own.



### The Block Cache

`JOS`支持`3G`的文件系统，从`0x1000000`到`0xD0000000`

```c
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}
```

#### Exercise 2

Implement the `bc_pgfault` and `flush_block` functions in `fs/bc.c`.`bc_pgfault` is a page fault handler, just like the one your wrote in the previous lab for copy-on-write fork, except that its job is to load pages in from the disk in response to a page fault. When writing this, keep in mind that

-  `addr` may not be aligned to a block boundary and 
-  `ide_read` operates in sectors, not blocks.

The `flush_block` function should write a block out to disk *if necessary*.  `flush_block`shouldn't do anything

-  if the block isn't even in the block cache (that is, the page isn't mapped) 
-  if it's not dirty

We will use the VM hardware to keep track of whether a disk block has been modified since it was last read from or written to disk. To see whether a block needs writing, we can just look to see if the `PTE_D` "dirty" bit is set in the `uvpt`entry.

 (The `PTE_D` bit is set by the processor in response to a write to that page; see 5.2.4.3 in [chapter 5](http://pdos.csail.mit.edu/6.828/2011/readings/i386/s05_02.htm) of the 386 reference manual.) 

After writing the block to disk, `flush_block` should clear the `PTE_D` bit using `sys_page_map`.

首先来看一下`ide_read`

```c
int ide_read(uint32_t secno, void *dst, size_t nsecs)
{
	int r;
	assert(nsecs <= 256);
	ide_wait_ready(0);
	outb(0x1F2, nsecs);
	outb(0x1F3, secno & 0xFF);
	outb(0x1F4, (secno >> 8) & 0xFF);
	outb(0x1F5, (secno >> 16) & 0xFF);
	outb(0x1F6, 0xE0 | ((diskno & 1) << 4) | ((secno >> 24) & 0x0F));
	outb(0x1F7, 0x20); // CMD 0x20 means read sector
	for (; nsecs > 0; nsecs--, dst += SECTSIZE)
	{
		if ((r = ide_wait_ready(1)) < 0)
			return r;
		insl(0x1F0, dst, SECTSIZE / 4);
	}
	return 0;
}
```

发现其就是利用`PIO`进行读取磁盘的内容。

然后实现`bc_pgfault`

```c
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *)utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void *)DISKMAP || addr >= (void *)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
			  utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	addr = ROUNDDOWN(addr, PGSIZE);
	if ((r = sys_page_alloc((envid_t)0, addr, PTE_P | PTE_U | PTE_W)) < 0)
		panic("in bc_pgfault, sys_page_alloc: %e", r);
	ide_read(blockno * BLKSECTS, addr, BLKSECTS);

	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}
```

实现`flush`

```c
void flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;
	if (addr < (void *)DISKMAP || addr >= (void *)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);
	addr = ROUNDDOWN(addr, PGSIZE);
	if (va_is_mapped(addr) && va_is_dirty(addr))
	{
		if ((r = ide_write(blockno * BLKSECTS, addr, BLKSECTS)) < 0)
			panic("in flush_block, ide_write: %e", r);
		if ((r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL)) < 0)
			panic("in flush_block, sys_page_map: %e", r);
	}
}
```

其中对这个页是否是`dirty`或者`maped`的检查，是利用`uvpd`检查对应表示位。

```c
// Is this virtual address mapped?
bool va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}
```

其中的 `PTE_D` 是又硬件自己设置的。

The `fs_init` function in `fs/fs.c` is a prime example of how to use the block cache. 

After initializing the block cache, it simply stores pointers into the disk map region in the`super` global variable. 

After this point, we can simply read from the `super` structure as if they were in memory and our page fault handler will read them from disk as necessary.

```c
void fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

	// Find a JOS disk.  Use the second IDE disk (number 1) if available
	if (ide_probe_disk1())
		ide_set_disk(1);
	else
		ide_set_disk(0);
	bc_init();

	// Set "super" to point to the super block.
	super = diskaddr(1);
	check_super();

	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);
	check_bitmap();
}
```

- 然后查看`ide.c`，其维护了一个全局变量`diskno`，用于在输出`IO`指令的时候确定指令内容。其提供了`switch`来更改`diskno`的值。

- 其中`bc_init()`如下

  ```c
  void bc_init(void)
  {
  	struct Super super;
  	set_pgfault_handler(bc_pgfault);
  	check_bc();

  	// cache the super block by reading it once
  	memmove(&super, diskaddr(1), sizeof super);
  }
  ```

  其先初始化了`pf_handler`，然后`check_bc`只是将第一块内容读出来，改写，再写入，再读出来测试`block cache`。最后将`block 1`的`super block`读到`cache`中。

- 其后，设置了`super`和`bitmap`

> Challenge!
>
> The block cache has no eviction policy. Once a block gets faulted in to it, it never gets removed and will remain in memory forevermore. Add eviction to the buffer cache. Using the `PTE_A` "accessed" bits in the page tables, which the hardware sets on any access to a page, you can track approximate usage of disk blocks without the need to modify every place in the code that accesses the disk map region. Be careful with dirty blocks.

### The Block Bitmap

After `fs_init` sets the `bitmap` pointer, we can treat `bitmap` as a packed array of bits, one for each block on the disk. See, for example, `block_is_free`, which simply checks whether a given block is marked free in the bitmap.

#### Exercise 3

Use `free_block` as a model to implement `alloc_block` in `fs/fs.c`, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with `flush_block`, to help file system consistency.

先看`block_is_free`

```c
// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool block_is_free(uint32_t blockno)
{
	if (super == 0 || blockno >= super->s_nblocks)
		return 0;
	if (bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}
```

其通过位操作，查看对应`bitmap`位是否设置。

```c
int alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.
	uint32_t blockno;
	for (blockno = 0; blockno < super->s_nblocks; blockno++)
	{
		if (block_is_free(blockno))
		{
			// set bitmap
			bitmap[blockno / 32] &= ~(1 << (blockno % 32));
			// flush bitmap
			flush_block(&bitmap[blockno / 32]);
			return blockno;
		}
	}
	return -E_NO_DISK;
}
```



### File Operations

We have provided a variety of functions in `fs/fs.c` to implement the basic facilities you will need to interpret and manage `File` structures, scan and manage the entries of directory-files, and walk the file system from the root to resolve an absolute pathname. Read through *all* of the code in `fs/fs.c` and make sure you understand what each function does before proceeding.

#### Exercise 4

Implement `file_block_walk` and `file_get_block`. `file_block_walk` maps from a block offset within a file to the pointer for that block in the `struct File` or the indirect block, very much like what `pgdir_walk` did for page tables. `file_get_block` goes one step further and maps to the actual disk block, allocating a new one if necessary.

`file_block_walk`比较坑

```c
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
	int r;
	//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT)
	if (filebno >= NDIRECT + NINDIRECT)
		return -E_INVAL;
	else if (filebno < NDIRECT)
		*ppdiskbno = &(f->f_direct[filebno]);
	else
	{
		// if indirect block not exist
		if (!f->f_indirect)
		{
			//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
			//		alloc was 0.
			if (!alloc)
				return -E_NOT_FOUND;
			//	-E_NO_DISK if there's no space on the disk for an indirect block.
			if ((r = alloc_block()) < 0)
				return r;
			memset(diskaddr(r), 0, BLKSIZE);
			f->f_indirect = r;
			// flush back to disk
			flush_block(diskaddr(r));
		}
		*ppdiskbno = &(((uint32_t *)(diskaddr(f->f_indirect)))[filebno - NDIRECT]);
	}
	return 0;
}
```

其中的`f_direct`是直接保存编号，而`f_indirect`则是`indirect`的编号，先要转成地址，再去那个地址拿编号，所以这里先要`diskaddr`转一下。

利用刚写的`walk`来实现`file_get_block`

```c
int file_get_block(struct File *f, uint32_t filebno, char **blk)
{
	uint32_t *ppdiskbno;
	int r;
	if ((r = file_block_walk(f, filebno, &ppdiskbno, 1)) != 0)
		return r;
	if (!*ppdiskbno)
		if ((*ppdiskbno = alloc_block()) < 0)
			return *ppdiskbno;
	*blk = diskaddr(*ppdiskbno);
	return 0;
}
```



> Challenge! The file system is likely to be corrupted if it gets interrupted in the middle of an operation (for example, by a crash or a reboot). Implement soft updates or journalling to make the file system crash-resilient and demonstrate some situation where the old file system would get corrupted, but yours doesn't.



### The file system interface

利用`IPC`使得其他的`enviroments`可以调用刚才写的那些功能，其大致流程如下：

```
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```

先看左边的分支。从最底部看起，上个`lab`刚刚写的`ipc_send`，经过包装成专门应对`filesystem`的`ipc`。

 ```c
static int
fsipc(unsigned type, void *dstva)
{
	static envid_t fsenv;
	if (fsenv == 0)
		fsenv = ipc_find_env(ENV_TYPE_FS);

	static_assert(sizeof(fsipcbuf) == PGSIZE);

	if (debug)
		cprintf("[%08x] fsipc %d %08x\n", thisenv->env_id, type, *(uint32_t *)&fsipcbuf);

	ipc_send(fsenv, type, &fsipcbuf, PTE_P | PTE_W | PTE_U);
	return ipc_recv(NULL, dstva, NULL);
}
 ```

首先找到`filesystem`的`server`的`env_id`，然后发送请求，最后接受反馈。

请求的详细内容均保存在`fsipcbuf`中，在调用`fsipc`发送请求之前，需要对这个结构体里面的详细内容设置清楚。

`fsipcbuf`是一个`union`类型，其包含了不同请求所需要发送内容的结构体。

然后看上面一层的`devfile_xx`

```c
devfile_xx {
  //设置fsipcbuf;
  fsipc(request,dstva);
  //处理结果;
}
```

完成`devfile_write`

```c
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	// Make an FSREQ_WRITE request to the file system server.  Be
	// careful: fsipcbuf.write.req_buf is only so large, but
	// remember that write is always allowed to write *fewer*
	// bytes than requested.
	int r;

	fsipcbuf.write.req_fileid = fd->fd_file.id;
	fsipcbuf.write.req_n = n;
	memmove(fsipcbuf.write.req_buf, buf, n);
	if ((r = fsipc(FSREQ_WRITE, NULL)))
		return r;
	assert(r <= n);
	assert(r <= PGSIZE);
	return r;
}
```

然后再看服务器一端的：

首先分析服务器端的数据结构：

- 每一个盘上的`struct file`是服务器独享的
- 每一个打开的文件均有一个`struct fd`，这个里面包含了文件偏移指针等。**所有的`fd`均保存在`FILEVA+i*PGSIZE`上。**
- 服务器用一个综合的`OpenFile`结构体将这两者打包起来。

服务器保存着`MAXOPEN`个`OpenFile`结构体在`opentab`里面用于保存打开文件的状态。

服务器将客户的请求映射到`fsreq 0x0ffff000`的地方。

服务器的主要框架和正常服务器类似，都一直在循环接受客户发来的请求。服务器在`handlers`里面分发`req`（open参数比较多，不能打包）

#### Exercise 5

Implement `serve_read` in `fs/serv.c`.

这个函数比较神奇，其传入的参数是`union`类型的，读完参数之后，就把那个地方用于读入的缓存覆盖了。

```c
int serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;
	struct OpenFile *o;
	int r;

	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);
	// First, use openfile_lookup to find the relevant open file.
	// On failure, return the error code to the client with ipc_send.
	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;
	// Second, call the relevant file system function (from fs/fs.c).
	// On failure, return the error code to the client.
	return file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset);
}
```



#### Exercise 6

Implement `serve_write` in `fs/serv.c` and `devfile_write` in `lib/file.c`.

和上面类似

```c
int serve_write(envid_t envid, struct Fsreq_write *req)
{
	struct OpenFile *o;
	int r;
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);
	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;
	return file_write(o->o_file, req->req_buf,req->req_n,o->o_fd->fd_offset);
}
```

客户端那边上面已经顺手写了。

来看下`file_read`怎么做的了

```c
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	if (offset >= f->f_size)
		return 0;

	count = MIN(count, f->f_size - offset);

	for (pos = offset; pos < offset + count;)
	{
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}
	return count;
}
```

发现其没有碰`offset`，或许是需要我们自己来动？

好吧，再加上改`offset`的

```c
o->o_fd->fd_offset += r;
```



# Spawning Processes

#### Exercise 7

 `spawn` relies on the new syscall `sys_env_set_trapframe` to initialize the state of the newly created environment. Implement `sys_env_set_trapframe` in `kern/syscall.c` (don't forget to dispatch the new system call in `syscall()`).

和其他的`syscall`类似，

```c
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret != 0)
		return ret;
	// Remember to check whether the user has supplied us with a good
	// address!
	if (tf->tf_eip >= UTOP)
		panic("in sys_env_set_trapframe: tf->tf_eip wrong!\n");
	e->env_tf = *tf;
	e->env_tf.tf_cs |= 3;
	e->env_tf.tf_eflags |= FL_IF;
	return 0;
}
```

实现了再去看下作者的`spawn`是怎么实现的，其程序流程基本如下：

- 打开程序文件
- 读`ELF`头
- 利用`sys_exofork`来创建新的`env`
- 设置`child`的`trapframe`
- 初始化用户程序的栈，保存`argc`与`argv`，两个空`word`，用于给`umain`传值。
- 映射所有的`segment`
- 拷贝所有的`library`
- 设置`trapframe`
- 设置新`env`的`state`

发现里面有故意坑人的

```c
// child_tf.tf_eflags |= FL_IOPL_3; // devious: see user/faultio.c
```

不让`child`拥有`IO`权限啊… 故意在这里设置一下。

> Challenge! Implement Unix-style `exec`.

> Challenge! Implement `mmap`-style memory-mapped files and modify `spawn` to map pages directly from the ELF image when possible.

### Sharing library state across fork and spawn

#### Exercise 8

Change `duppage` in `lib/fork.c` to follow the new convention. If the page table entry has the `PTE_SHARE` bit set, just copy the mapping directly. (You should use `PTE_SYSCALL`, not `0xfff`, to mask out the relevant bits from the page table entry. `0xfff` picks up the accessed and dirty bits as well.)

直接在`dup page`的时候加上对于`PTE_SHARE`的判断即可。

```c
	else if (uvpt[pn] & PTE_SHARE)
	{
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL)) < 0)
			panic("sys_page_map: %e\n", r);
	}
```

`dup page`是对于`fork`的，而`spawn`也要相同的工作。

Likewise, implement `copy_shared_pages` in `lib/spawn.c`. It should loop through all page table entries in the current process (just like `fork` did), copying any page mappings that have the `PTE_SHARE` bit set into the child process.

```c
static int
copy_shared_pages(envid_t child)
{
	uintptr_t addr;
	int r;
	int pte_flag = PTE_P | PTE_U | PTE_SHARE;
	for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE)
	{
		if ((uvpd[PDX(addr)] & PTE_P) &&
			(uvpt[PGNUM(addr)] & (pte_flag)) == (pte_flag))
		{
			if ((r = sys_page_map((envid_t)0, (void *)addr, child, (void *)addr,
								  uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
				panic("in copy_shared_pages sys_page_map: %e\n", r);
		}
	}
	return 0;
}
```

自此，这些`shared page`，可以由`parents`或者`child`写。

结果调试出错....加了各种`tag`之后，发现，在`dup`的时候，需要吧这个`share`的check放在第一个，否则有可能出现直接因为标记了`PTE_W`进入第一个分支的情况。(其实我觉得应该是`PTE_W`和`PTE_COW`都标记了再`COW`否则就直接拷贝)

### The keyboard interface

```c
	case IRQ_OFFSET + IRQ_KBD:
		kbd_intr();
		return;
	case IRQ_OFFSET + IRQ_SERIAL:
		serial_intr();
		return;
```

用我们`lab1`中的`intr`即可实现`keyboard`的`Interupt`

#### Exercise 10

The shell doesn't support I/O redirection. It would be nice to run sh <script instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for < to  `user/sh.c`.

在对应的`case`写即可

```c
			if ((fd = open(t, O_RDONLY)) < 0)
			{
				cprintf("Error open %s fail: %e", t, fd);
				exit();
			}
			if (fd != 0)
			{
				dup(fd, 0);
				close(fd);
			}
			break;
```



> Challenge! Add more features to the shell. Possibilities include (a few require changes to the file system too):
>
> - backgrounding commands (`ls &`)
> - multiple commands per line (`ls; echo hi`)
> - command grouping (`(ls; echo hi) | cat > out`)
> - environment variable expansion (`echo $hello`)
> - quoting (`echo "a | b"`)
> - command-line history and/or editing
> - tab completion
> - directories, cd, and a PATH for command-lookup.
> - file creation
> - ctl-c to kill the running environment



## 文件系统总结

- 底层与磁盘有关的丢给`ide_xx`来实现，因为要用到`IO`中断，要给对应的权限
- 通过`bc_pgfault`来实现缺页自己映射，利用`flush_block`来回写磁盘
- 然后通过分`block`利用`block cache`实现对于磁盘的数据读入内存或者写回磁盘
- 再上面一层的`file_read`与`file_write`，均是对于`blk`的操作。
- 再上面就是文件系统服务器，通过调用`file_read`实现功能了。
- 客户端通过对需求打包，发送`IPC`给文件系统服务器，即可实现读／写文件的功能。

## 文件系统&文件描述符 Overview

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

![](./img/fd.png)

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