# User Application

本文将主要关注用户程序，并补全内核功能。本文主要包括以下用户应用程序：

```
ls		list directory contents
pwd		return working directory name
mkdir	make directories
shell
```



## ls list directory contents

### 读文件

由于写到这里第一次在用户空间读取文件，简要记录一下读取文件的过程。

首先是文件结构，在lab5中设计文件系统的时候设计的，保存在`struct File`中，用户可以根据此结构体偏移来找具体的信息。

再是`fsformat`中提供的与文件系统相关的接口。这里用到了`readn`。其只是对于`read`的一层包装。

### 功能实现

回到`ls`本身的逻辑上。`ls` 主要是读取`path`文件，并将其下所有的文件名全部打印出来。

##pwd return working directory name

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







