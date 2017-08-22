// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW 0x800

extern void _pgfault_upcall(void);

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *)utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	if (!(err & FEC_WR))
		panic("Page fault: not write fault\n");
	if (!(uvpd[PDX(addr)] & PTE_P))
		panic("Page fault: PDE not exist\n");
	if ((uvpt[PGNUM(addr)] & (PTE_P | PTE_COW)) != (PTE_P | PTE_COW))
		panic("Page fault: Page not copy-on-write\n");
	// Allocate a new page, map it at a temporary location (PFTEMP)
	addr = ROUNDDOWN(addr, PGSIZE);
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc fault: %e", r);
	// copy the data from the old page to the new page
	memmove(PFTEMP, addr, PGSIZE);
	// then move the new page to the old page's address.
	if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_map fault: %e", r);
	// unmap the tmp addr
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap fault: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr = (void *)(pn * PGSIZE);

	// cprintf("dup start pn%#x %#x\n",pn,uvpt[pn]);
	// If the page is writable or copy-on-write
	if (uvpt[pn] & PTE_SHARE)
	{
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL)) < 0)
			panic("sys_page_map: %e\n", r);
	}
	else if (uvpt[pn] & (PTE_W | PTE_COW))
	{
		// cprintf("writeable\n");
		// the new mapping must be created copy-on-write
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P | PTE_COW) < 0))
			panic("sys_page_map fault: %e\n", r);
		// our mapping must be marked copy-on-write as well.
		if ((r = sys_page_map((envid_t)0, addr, 0, addr, PTE_U | PTE_P | PTE_COW) < 0))
			panic("sys_page_map fault: %e\n", r);
	}
	else
	{
		// cprintf("not writeable\n");
		if ((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P)) < 0)
			panic("sys_page_map fault: %e\n", r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	envid_t envid;
	uintptr_t addr;
	int r;
	// set parent's page fault handler
	set_pgfault_handler(pgfault);
	if ((envid = sys_exofork()) < 0)
		panic("sys_exofork fail: %e", envid);
	if (envid == 0)
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// Copy our address space(to USTACKTOP not UTOP as we have to
	// allocate a new User Exception Stack for child
	for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE)
	{
		if ((uvpd[PDX(addr)] & PTE_P) &&
			(uvpt[PGNUM(addr)] & (PTE_P | PTE_U)) == (PTE_P | PTE_U))
			duppage(envid, addr / PGSIZE);
	}
	// Allocate a new User Exception Stack for child
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P)) < 0)
		panic("sys_page_alloc fail: %e\n", r);
	// set child env's pgfault upcall
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall fail: %e\n", r);
	// set child env's status to runnable
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status fail: %e\n", r);
	// cprintf("Parent finished child %08x\n",envid);
	return envid;
}

// Challenge!
int sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
