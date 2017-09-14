// Main public header file for our user-land support library,
// whose code lives in the lib directory.
// This library is roughly our OS's version of a standard C library,
// and is intended to be linked into all user-mode applications
// (NOT the kernel or boot loader).

#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H 1

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/trap.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/args.h>
#include <inc/spinlock.h>
#include <inc/time.h>
#include <inc/interface.h>
#include <inc/bitmap.h>
#include <inc/file.h>
#include <inc/usyscall.h>
#include <inc/sysinfo.h>
#include <inc/bprintf.h>

#define USED(x) (void)(x)

// main user program
void umain(int argc, char **argv);

// libmain.c or entry.S
extern const char *binaryname;
extern const volatile struct Env *thisenv;
extern const volatile struct Env envs[NENV];
extern const volatile struct PageInfo pages[];
uint8_t *framebuffer;

// exit.c
void exit(void);

// pgfault.c
void set_pgfault_handler(void (*handler)(struct UTrapframe *utf));

// readline.c
char *readline(const char *buf);

// This must be inlined.  Exercise for reader: why?
static inline envid_t __attribute__((always_inline))
sys_exofork(void)
{
	envid_t ret;
	asm volatile("int %2"
				 : "=a"(ret)
				 : "a"(SYS_exofork), "i"(T_SYSCALL));
	return ret;
}

// ipc.c
void ipc_send(envid_t to_env, uint32_t value, void *pg, int perm);
int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store);
envid_t ipc_find_env(enum EnvType type);

// fork.c
#define PTE_SHARE 0x400
envid_t fork(void);
envid_t sfork(void); // Challenge!

// fd.c


// pageref.c
int pageref(void *addr);

// spawn.c
envid_t spawn(const char *program, const char **argv);
envid_t spawnl(const char *program, const char *arg0, ...);

// console.c
void cputchar(int c);
int getchar(void);
int iscons(int fd);
int opencons(void);

// screen.c
int openscreen(struct interface *interface);
int isscreen(int fd);

// pipe.c
int pipe(int pipefds[2]);
int pipeisclosed(int pipefd);

// wait.c
void wait(envid_t env);

// dir.c
int chdir(const char *path);
char *getcwd(char *buffer, int maxlen);
int mkdir(const char *dirname);

// Graph
struct graph_info
{
	uint16_t scrnx, scrny;
	uint8_t *framebuffer;
} graph;

struct frame_info *frame;


#define KEY_UP 0xe2
#define KEY_DOWN 0xe3
#define KEY_LEFT 0xe4
#define KEY_RIGHT 0xe5
#define KEY_ENTER 0xa
#define KEY_ESC 0x1b

#endif // !JOS_INC_LIB_H
