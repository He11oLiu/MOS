#ifndef JOS_INC_USYSCALL_H
#define JOS_INC_USYSCALL_H

#include <inc/types.h>
#include <inc/time.h>
#include <inc/env.h>

void sys_cputs(const char *string, size_t len);
int sys_cgetc(void);
envid_t sys_getenvid(void);
int sys_env_destroy(envid_t);
void sys_yield(void);
static envid_t sys_exofork(void);
int sys_env_set_status(envid_t env, int status);
int sys_env_set_trapframe(envid_t env, struct Trapframe *tf);
int sys_env_set_pgfault_upcall(envid_t env, void *upcall);
int sys_page_alloc(envid_t env, void *pg, int perm);
int sys_page_map(envid_t src_env, void *src_pg,
				 envid_t dst_env, void *dst_pg, int perm);
int sys_page_unmap(envid_t env, void *pg);
int sys_ipc_try_send(envid_t to_env, uint32_t value, void *pg, int perm);
int sys_ipc_recv(void *rcv_pg);
int sys_env_set_workpath(envid_t envid, const char *path);
int sys_gettime(struct tm *tm);
int sys_updatescreen();
int sys_setpalette();

#endif