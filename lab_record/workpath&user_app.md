# User Application

本文将主要关注用户程序，并补全内核功能。本文主要包括以下用户应用程序：

```shell
ls		list directory contents
pwd		return working directory name
mkdir	make directories
touch	change file access and modification times(we only support create file)
cat		concatenate and print files
shell
```



## list directory contents

### 读文件

由于写到这里第一次在用户空间读取文件，简要记录一下读取文件的过程。

首先是文件结构，在lab5中设计文件系统的时候设计的，保存在`struct File`中，用户可以根据此结构体偏移来找具体的信息。

再是`fsformat`中提供的与文件系统相关的接口。这里用到了`readn`。其只是对于`read`的一层包装。

### 功能实现

回到`ls`本身的逻辑上。`ls` 主要是读取`path`文件，并将其下所有的文件名全部打印出来。

##return working directory name

由于之前写的`JOS`中每个进程没有写工作目录。这里再加上工作目录。

在`struct env`中加入工作目录，添加后`env`如下：

```c
struct Env {
	struct Trapframe env_tf;	// Saved registers
	struct Env *env_link;		// Next free Env
	envid_t env_id;			// Unique environment identifier
	envid_t env_parent_id;		// env_id of this env's parent
	enum EnvType env_type;		// Indicates special system environments
	unsigned env_status;		// Status of the environment
	uint32_t env_runs;		// Number of times environment has run
	int env_cpunum;			// The CPU that the env is running on

	// Address space
	pde_t *env_pgdir;		// Kernel virtual address of page dir

	// Exception handling
	void *env_pgfault_upcall;	// Page fault upcall entry point

	// IPC
	bool env_ipc_recving;		// Env is blocked receiving
	void *env_ipc_dstva;		// VA at which to map received page
	uint32_t env_ipc_value;		// Data value sent to us
	envid_t env_ipc_from;		// envid of the sender
	int env_ipc_perm;		// Perm of page mapping received

	// work path
	char workpath[MAXPATH];
};
```

由于`env`对于用户是不可以写的，所以要添加新的`syscall`，进入内核态改。

```c
enum {
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,
	SYS_env_destroy,
	SYS_page_alloc,
	SYS_page_map,
	SYS_page_unmap,
	SYS_exofork,
	SYS_env_set_status,
	SYS_env_set_trapframe,
	SYS_env_set_pgfault_upcall,
	SYS_yield,
	SYS_ipc_try_send,
	SYS_ipc_recv,
	SYS_getcwd,
	SYS_chdir,
	NSYSCALLS
};
```

由于`JOS`中用户其实可以读`env`中的内容，所以`getcwd`就不陷入内核态了，直接读取就好。

新建`dir.c`用于存放与目录有关的函数，实现`getcwd`

```c
char *getcwd(char *buffer, int maxlen)
{
	if(!buffer || maxlen < 0)
		return NULL;
	return strncpy((char *)buffer,(const char*)thisenv->workpath,maxlen);
}
```

而对于修改目录，必须要陷入内核态了，新加`syscall`。

```c
int sys_chdir(const char *path)
{
	return syscall(SYS_chdir, 0, (uint32_t)path, 0, 0, 0, 0);
}
```

刚才的`dir.c`中加入用户接口

```c
// change work path
// Return 0 on success, 
// Return < 0 on error. Errors are:
//	-E_INVAL *path not exist or not a path
int chdir(const char *path)
{
	int r;
	struct Stat st;
	if ((r = stat(path, &st)) < 0)
		return r;
	if(!st.st_isdir)
		return -E_INVAL;
	return sys_chdir(path);
}
```

然后去内核添加功能

```c
// change work path
// return 0 on success.
static int
sys_chdir(const char * path)
{
	strcpy((char *)curenv->workpath,path);
	return 0;
}
```

最后实现`pwd`

```c
#include <inc/lib.h>

void umain(int argc, char **argv)
{
    char path[200];
    if(argc > 1)
        printf("%s : too many arguments\n",argv[0]);
    else
        printf("%s\n",getcwd(path,200));
}
```

## make directories

发现`JOS`给我们预留了标识位`O_MKDIR`，由于与普通的`file_create`不一样，当有同名的文件存在的时候，但其不是目录的情况下，我们仍然可以创建，所以新写了函数

```c
int dir_create(const char *path, struct File **pf)
{
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

	if (((r = walk_path(path, &dir, &f, name)) == 0) &&
		f->f_type == FTYPE_DIR)
		return -E_FILE_EXISTS;
	if (r != -E_NOT_FOUND || dir == 0)
		return r;
	if ((r = dir_alloc_file(dir, &f)) < 0)
		return r;

	// fill struct file
	strcpy(f->f_name, name);
	f->f_type = FTYPE_DIR;

	*pf = f;
	file_flush(dir);
	return 0;
}
```

然后在`serve_open`下建立新的分支

```c
	// create dir
	else if (req->req_omode & O_MKDIR)
	{
		if ((r = dir_create(path, &f)) < 0)
		{
			if (!(req->req_omode & O_EXCL) && r == -E_FILE_EXISTS)
				goto try_open;
			if (debug)
				cprintf("file_create failed: %e", r);
			return r;
		}
	}
```

在`dir.c`下提供`mkdir`函数

```c
// make directory
// Return 0 on success,
// Return < 0 on error. Errors are:
//	-E_FILE_EXISTS directory already exist
int mkdir(const char *dirname)
{
	char cur_path[MAXPATH];
	int r;
	getcwd(cur_path, MAXPATH);
	strcat(cur_path, dirname);
	if ((r = open(cur_path, O_MKDIR)) < 0)
		return r;
	close(r);
	return 0;
}
```

最后提供用户程序

```c
#include <inc/lib.h>
#define MAXPATH 200

void umain(int argc, char **argv)
{
    int r;

    if (argc != 2)
    {
        printf("usage: mkdir directory\n");
        return;
    }
    if((r = mkdir(argv[1])) < 0)
        printf("%s error : %e\n",argv[0],r);
}
```

## Create file

创建文件直接利用`open`中的`O_CREAT`选项即可。

```c
#include <inc/lib.h>
#define MAXPATH 200

void umain(int argc, char **argv)
{
    int r;
    char *filename;
    char pathbuf[MAXPATH];
    if (argc != 2)
    {
        printf("usage: touch filename\n");
        return;
    }
    filename = argv[1];
    if (*filename != '/')
        getcwd(pathbuf, MAXPATH);
    strcat(pathbuf, filename);
    if ((r = open(pathbuf, O_CREAT)) < 0)
        printf("%s error : %e\n", argv[0], r);
    close(r);
}
```

## cat

这个只需要修改好支持工作路径即可

```c
#include <inc/lib.h>

char buf[8192];

void cat(int f, char *s)
{
	long n;
	int r;

	while ((n = read(f, buf, (long)sizeof(buf))) > 0)
		if ((r = write(1, buf, n)) != n)
			panic("write error copying %s: %e", s, r);
	if (n < 0)
		panic("error reading %s: %e", s, n);
}

void umain(int argc, char **argv)
{
	int f, i;
	char *filename;
	char pathbuf[MAXPATH];

	binaryname = "cat";
	if (argc == 1)
		cat(0, "<stdin>");
	else
		for (i = 1; i < argc; i++)
		{
			filename = argv[1];
			if (*filename != '/')
				getcwd(pathbuf, MAXPATH);
			strcat(pathbuf, filename);
			f = open(pathbuf, O_RDONLY);
			if (f < 0)
				printf("can't open %s: %e\n", argv[i], f);
			else
			{
				cat(f, argv[i]);
				close(f);
			}
		}
}
```

## SHELL

写`Shell`的时候发现问题：之前没有解决`fork`以及`spawn`时候的子进程的工作路径的问题。所有再一次修改了系统调用，将系统调用`sys_chdir`修改为能够设定指定进程的工作目录的系统调用。

```c
int sys_env_set_workpath(envid_t envid, const char *path);
```

修改对应的内核处理：

```c
// change work path
// return 0 on success.
static int
sys_env_set_workpath(envid_t envid, const char *path)
{
	struct Env *e;
	int ret = envid2env(envid, &e, 1);
	if (ret != 0)
		return ret;
	strcpy((char *)e->workpath, path);
	return 0;
}
```

这样就会`fork`出来的子进程继承父亲的工作路径。

在`shell`中加入`built-in`功能，为未来扩展`shell`功能提供基础

```c
int builtin_cmd(char *cmdline)
{
    int ret;
    int i;
    char cmd[20];
    for (i = 0; cmdline[i] != ' ' && cmdline[i] != '\0'; i++)
        cmd[i] = cmdline[i];
    cmd[i] = '\0';
    if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
        exit();
    if (!strcmp(cmd, "cd"))
    {
        ret = do_cd(cmdline);
        return 1;
    }
    return 0;
}

int do_cd(char *cmdline)
{
    char pathbuf[BUFSIZ];
    int r;
    pathbuf[0] = '\0';
    cmdline += 2;
    while (*cmdline == ' ')
        cmdline++;
    if (*cmdline == '\0')
        return 0;
    if (*cmdline != '/')
    {
        getcwd(pathbuf, BUFSIZ);
    }
    strcat(pathbuf, cmdline);
    if ((r = chdir(pathbuf)) < 0)
        printf("cd error : %e\n", r);
    return 0;
}
```

修改`<` 与 `>` 支持当前工作路径

```c
case '<': // Input redirection
            // Grab the filename from the argument list
            if (gettoken(0, &t) != 'w')
            {
                cprintf("syntax error: < not followed by word\n");
                exit();
            }
            // Open 't' for reading as file descriptor 0
            // (which environments use as standard input).
            // We can't open a file onto a particular descriptor,
            // so open the file as 'fd',
            // then check whether 'fd' is 0.
            // If not, dup 'fd' onto file descriptor 0,
            // then close the original 'fd'.

            if (t[0] != '/')
                getcwd(argv0buf, MAXPATH);
            strcat(argv0buf, t);
            if ((fd = open(argv0buf, O_RDONLY)) < 0)
            {
                cprintf("Error open %s fail: %e", argv0buf, fd);
                exit();
            }
            if (fd != 0)
            {
                dup(fd, 0);
                close(fd);
            }
            break;

        case '>': // Output redirection
            // Grab the filename from the argument list
            if (gettoken(0, &t) != 'w')
            {
                cprintf("syntax error: > not followed by word\n");
                exit();
            }
            if (t[0] != '/')
                getcwd(argv0buf, MAXPATH);
            strcat(argv0buf, t);
            if ((fd = open(argv0buf, O_WRONLY | O_CREAT | O_TRUNC)) < 0)
            {
                cprintf("open %s for write: %e", argv0buf, fd);
                exit();
            }
            if (fd != 1)
            {
                dup(fd, 1);
                close(fd);
            }
            break;
```





## 创建硬盘镜像

- 利用`mmap`映射到内存，对内存读写。

  ```c
  	if ((diskmap = mmap(NULL, nblocks * BLKSIZE, PROT_READ | PROT_WRITE,
  						MAP_SHARED, diskfd, 0)) == MAP_FAILED)
  		panic("mmap %s: %s", name, strerror(errno));
  ```

  从`diskmap`开始，大小为`nblocks * BLKSIZE`

- `alloc`用于分配空间，移动`diskpos`

```c
void *
alloc(uint32_t bytes)
{
	void *start = diskpos;
	diskpos += ROUNDUP(bytes, BLKSIZE);
	if (blockof(diskpos) >= nblocks)
		panic("out of disk blocks");
	return start;
}
```

- 块 123 在初始化的时候分配

  ```c
  	alloc(BLKSIZE);
  	super = alloc(BLKSIZE);
  	super->s_magic = FS_MAGIC;
  	super->s_nblocks = nblocks;
  	super->s_root.f_type = FTYPE_DIR;
  	strcpy(super->s_root.f_name, "/");

  	nbitblocks = (nblocks + BLKBITSIZE - 1) / BLKBITSIZE;
  	bitmap = alloc(nbitblocks * BLKSIZE);
  	memset(bitmap, 0xFF, nbitblocks * BLKSIZE);
  ```

- `writefile`用于申请空间，写入磁盘

  ```c
  void writefile(struct Dir *dir, const char *name)
  {
  	int r, fd;
  	struct File *f;
  	struct stat st;
  	const char *last;
  	char *start;

  	if ((fd = open(name, O_RDONLY)) < 0)
  		panic("open %s: %s", name, strerror(errno));
  	if ((r = fstat(fd, &st)) < 0)
  		panic("stat %s: %s", name, strerror(errno));
  	if (!S_ISREG(st.st_mode))
  		panic("%s is not a regular file", name);
  	if (st.st_size >= MAXFILESIZE)
  		panic("%s too large", name);

  	last = strrchr(name, '/');
  	if (last)
  		last++;
  	else
  		last = name;

  	// 获取目录中的一个空位
  	f = diradd(dir, FTYPE_REG, last);
  	// 获取文件存放地址，分配空间
  	start = alloc(st.st_size);
  	// 将文件读如到磁盘中刚刚分配的地址
  	readn(fd, start, st.st_size);
  	// 完成文件信息
  	finishfile(f, blockof(start), st.st_size);
  	close(fd);
  }

  void finishfile(struct File *f, uint32_t start, uint32_t len)
  {
  	int i;
  	// 这个是刚才目录下传过来的地址，直接修改目录下的相应项
  	f->f_size = len;
  	len = ROUNDUP(len, BLKSIZE);
  	for (i = 0; i < len / BLKSIZE && i < NDIRECT; ++i)
  		f->f_direct[i] = start + i;
  	if (i == NDIRECT)
  	{
  		uint32_t *ind = alloc(BLKSIZE);
  		f->f_indirect = blockof(ind);
  		for (; i < len / BLKSIZE; ++i)
  			ind[i - NDIRECT] = start + i;
  	}
  }
  ```

- 目录结构体与何时将目录写入

  ```c
  void startdir(struct File *f, struct Dir *dout)
  {
  	dout->f = f;
  	dout->ents = malloc(MAX_DIR_ENTS * sizeof *dout->ents);
  	dout->n = 0;
  }

  void finishdir(struct Dir *d)
  {
  	// 目录文件的大小
  	int size = d->n * sizeof(struct File);
  	// 申请目录文件存放空间
  	struct File *start = alloc(size);
    	// 将目录的文件内容放进去
  	memmove(start, d->ents, size);
      // 补全目录在磁盘当中的信息
  	finishfile(d->f, blockof(start), ROUNDUP(size, BLKSIZE));
  	free(d->ents);
  	d->ents = NULL;
  }
  ```

- 添加`bin`路径，并在`shell`中类似`path`环境变量默认读取`bin`下的可执行文件

  ```c
  	opendisk(argv[1]);

  	startdir(&super->s_root, &root);
  	f = diradd(&root, FTYPE_DIR, "bin");
  	startdir(f,&bin);
  	for (i = 3; i < argc; i++)
  		writefile(&bin, argv[i]);
  	finishdir(&bin);
  	finishdir(&root);

  	finishdisk();
  ```




## 获取时间

又新增一个`syscall`，这里不再累述，利用`mc146818_read`获取`cmos`时间即可。

```c
int gettime(struct tm *tm)
{
    unsigned datas, datam, datah;
    int i;
    tm->tm_sec = BCD_TO_BIN(mc146818_read(0));
    tm->tm_min = BCD_TO_BIN(mc146818_read(2));
    tm->tm_hour = BCD_TO_BIN(mc146818_read(4)) + TIMEZONE;
    tm->tm_wday = BCD_TO_BIN(mc146818_read(6));
    tm->tm_mday = BCD_TO_BIN(mc146818_read(7));
    tm->tm_mon = BCD_TO_BIN(mc146818_read(8));
    tm->tm_year = BCD_TO_BIN(mc146818_read(9));
    return 0;
}
```



## 实机运行输出

```shell
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
====Graph mode on====
   scrnx = 1024
   scrny = 768
MMIO VRAM = 0xef803000
=====================
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2 4
FS is running
FS can do I/O
Device 1 presence: 1
block cache is good
superblock is good
bitmap is good

# msh in / [12: 4:28]
$ cd documents

# msh in /documents/ [12: 4:35]
$ echo hello liu > hello

# msh in /documents/ [12: 4:45]
$ cat hello
hello liu

# msh in /documents/ [12: 4:49]
$ cd /bin

# msh in /bin/ [12: 4:54]
$ ls -l -F
-          37 newmotd
-          92 motd
-         447 lorem
-         132 script
-        2916 testshell.key
-         113 testshell.sh
-       20308 cat
-       20076 echo
-       20508 ls
-       20332 lsfd
-       25060 sh
-       20076 hello
-       20276 pwd
-       20276 mkdir
-       20280 touch
-       29208 msh

# msh in /bin/ [12: 4:57]
$ 
```

