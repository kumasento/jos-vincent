// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	// panic("pgfault");
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	// cprintf("[%08x] fork pgfault: addr %08p\n", thisenv->env_id, addr);
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint: 
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pde_t pde = uvpd[PDX(addr)];
	pte_t pte = uvpt[PGNUM(addr)];
	if (!(err & FEC_WR) || !(pde & PTE_P) || !(pte & PTE_P) || !(pte & PTE_COW)){
		if (!(err & FEC_WR)) panic("not a write action.");
		if (!(pde & PTE_P)) panic("page directory missed.");
		if (!(pte & PTE_P)) panic("page table missed.");
		if (!(pte & PTE_COW)) panic("not a COW page.");
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	
	if (sys_page_alloc(0, (void *)PFTEMP, PTE_W|PTE_U|PTE_P) < 0)
		panic("can't alloc page");
	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy((void *)PFTEMP, addr, PGSIZE);
	if (sys_page_map(0, (void *)PFTEMP, 0, addr, PTE_W|PTE_U|PTE_P) < 0)
		panic("can't map page");
	if (sys_page_unmap(0, (void *)PFTEMP) < 0)
		panic("can't unmap page");
	return;
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
	// LAB 4: Your code here.
	void *addr = (void*) (pn*PGSIZE);
	pte_t pte = uvpt[pn];
	if ((pte & PTE_SHARE)) {
		if (sys_page_map(0, addr, envid, addr, PTE_SYSCALL & pte) < 0)
			panic("duppage: PTE_SHARE mapping failed");
	}
	else if ((pte & PTE_W) || (pte & PTE_COW)) {
		if (sys_page_map(0, addr, envid, addr, PTE_COW|PTE_U|PTE_P) < 0)
			panic("can't map page from envid to 0");
		if (sys_page_map(0, addr, 0, addr, PTE_COW|PTE_U|PTE_P) < 0)
			panic("can't map page from 0 to 0 at the same addr");
	} 
	else {
		if (sys_page_map(0, addr, envid, addr, PTE_U|PTE_P) < 0)
			panic("can't map page from envid to 0");
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
	set_pgfault_handler(pgfault);

	envid_t envid;
	uint32_t addr;

	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	

	for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE)
		if ((uvpd[PDX(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_U)) 
			duppage(envid, PGNUM(addr));

	if (sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U|PTE_W|PTE_P) < 0)
		panic("can't alloc page for UXSTACK");

	extern void _pgfault_upcall();
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

	if (sys_env_set_status(envid, ENV_RUNNABLE) < 0)
		panic("can't set status for child env");
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}