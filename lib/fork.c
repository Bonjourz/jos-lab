// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
#define NCPU		8

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_WR) || !(vpd[PDX(addr)] & PTE_P) || 
		!(vpt[PGNUM(addr)] & PTE_P) || !(vpt[PGNUM(addr)] & PTE_COW))
		panic("pgfault: invalid pg info or errno!");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	if ((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W)))
		panic("Fork: pgfault %e\n", r);

	memmove((void *)PFTEMP, (void *)PTE_ADDR(addr), PGSIZE);
	if ((r = sys_page_map(0, (void *)PFTEMP, 0, (void *)PTE_ADDR(addr), PTE_U | PTE_P | PTE_W)) < 0)
		panic("Fork: pgfault %e\n", r);

	if ((r = sys_page_unmap(0, (void *)PFTEMP)) < 0)
		panic("Fork: pgfault %e\n", r);
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

	// LAB 4: Your code here. // TO DO: map other privilege bits
	if (((vpt[pn] & PTE_COW) || (vpt[pn] & PTE_W)) &&
		(vpt[pn] & PTE_P) && (vpt[pn] & PTE_U)) {
		if ((r = sys_page_map(0, (void *)(pn * PGSIZE), envid, (void *)(pn * PGSIZE), 
			PTE_COW | PTE_P | PTE_U)) < 0) {
			panic("Fork: duppage error: %e\n", r);
			return r;
		}

		if ((r = sys_page_map(0, (void *)(pn * PGSIZE), 0, (void *)(pn * PGSIZE), 
			PTE_COW | PTE_P | PTE_U)) < 0) {
			panic("Fork: duppage error: %e\n", r);
			return r;
		}
	}

	else if ((vpt[pn] & PTE_U) && (vpt[pn] & PTE_P)) {
		if ((r = sys_page_map(0, (void *)(pn * PGSIZE), envid, (void *)(pn * PGSIZE), 
			PTE_U | PTE_P)) < 0) {
			panic("Fork: duppage error: %e\n", r);
			return r;
		}

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
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
extern void _pgfault_upcall(void);

envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("Fork: sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	int r;
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("Fork: sys_env_set_pgfault_upcall: %e", envid);

	// We're the parent.
	// Eagerly copy our entire address space into the child.
	// This is NOT what you should do in your fork implementation.
	uint32_t addr;
	for (addr = 0; addr < UTOP - PGSIZE; addr += PGSIZE) {
		if ((vpd[PDX(addr)] & PTE_P) && (vpt[PGNUM(addr)] & PTE_P) &&
			(vpt[PGNUM(addr)] & PTE_U))
			duppage(envid, PGNUM(addr));
	}

	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		panic("Fork: sys_page_alloc: %e", r);

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("Fork: sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{	
	#ifdef SFORK
	set_pgfault_handler(pgfault);

	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("Fork: sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	int r;
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("Fork: sys_env_set_pgfault_upcall: %e", envid);

	uint32_t addr, heap_top = thisenv->heap_top;
	for (addr = USTACKTOP - PGSIZE; addr < USTACKTOP; addr += PGSIZE) {
		if ((vpd[PDX(addr)] & PTE_P) && (vpt[PGNUM(addr)] & PTE_P) &&
			(vpt[PGNUM(addr)] & PTE_U)) {
			int perm = vpt[PGNUM(addr)] & PTE_SYSCALL;
			if ((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
				return r;

			memmove((void *)PFTEMP, (void *)addr, PGSIZE);
			if ((r = sys_page_map(0, (void *)PFTEMP, envid, (void *)addr, perm | 
				PTE_U | PTE_W)) < 0)
				return r;

			if ((r = sys_page_unmap(0, (void *)PFTEMP)) < 0)
				return r;
		}
	}
	for (addr = 0; addr < USTACKTOP - PGSIZE; addr += PGSIZE) {
		if ((vpd[PDX(addr)] & PTE_P) && (vpt[PGNUM(addr)] & PTE_P) &&
			(vpt[PGNUM(addr)] & PTE_U)) {
			int perm = vpt[PGNUM(addr)] & PTE_SYSCALL;
			if ((r = sys_page_map(0, (void *)addr, envid, (void *)addr, perm)) < 0)
				return r;
		}
	}
	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		panic("Fork: sys_page_alloc: %e", r);

	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("Fork: sys_env_set_status: %e", r);

	return envid;
	#else
	panic("sfork not implemented");
	return -E_INVAL;
	#endif
}
