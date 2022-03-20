// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW 0x800

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
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if (!(err & FEC_PR) || !(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
		panic("pgfault error");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);

	if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);

	memmove(PFTEMP, addr, PGSIZE);

	if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_map: %e", r);

	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
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
	void *addr = (void *) (pn * PGSIZE);
	pte_t pte = uvpt[pn];

	if (pte & PTE_W) {
		r = sys_page_map(0, addr, envid, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0)
			panic("sys_page_map error");
		r = sys_page_map(0, addr, 0, addr, PTE_COW | PTE_P | PTE_U);
		if (r < 0)
			panic("sys_page_map error");
	} else {
		r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U);
		if (r < 0)
			panic("sys_page_map error");
	}

	return r;
}

static void
dup_or_share(envid_t dstenv, void *va, int perm)
{
	int r;
	if (perm & PTE_W) {
		if ((r = sys_page_alloc(dstenv, va, perm)) < 0)
			panic("sys_page_alloc: %e", r);
		if ((r = sys_page_map(dstenv, va, 0, UTEMP, perm)) < 0)
			panic("sys_page_map: %e", r);
		memmove(UTEMP, va, PGSIZE);
		if ((r = sys_page_unmap(0, UTEMP)) < 0)
			panic("sys_page_unmap: %e", r);
	} else {
		if ((r = sys_page_map(0, va, dstenv, va, perm)) < 0)
			panic("sys_page_map: %e", r);
	}
}

envid_t
fork_v0(void)
{
	envid_t envid;
	uint8_t *addr;
	int r;

	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	for (addr = 0; (int) addr < UTOP; addr += PGSIZE) {
		pde_t pde = uvpd[PDX(addr)];
		if (pde & PTE_P) {
			pte_t pte = uvpt[PGNUM(addr)];

			if (pte & PTE_P) {
				dup_or_share(envid,
				             (void *) addr,
				             pte & PTE_SYSCALL);
			}
		}
	}

	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
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
	// LAB 4: Your code here.
	// 1. Instalar, en el padre, la función pgfault como manejador de page faults.
	set_pgfault_handler(pgfault);

	// 2. Llamar a sys_exofork() y manejar el resultado.
	envid_t id = sys_exofork();
	if (id < 0) {
		panic("sys_exofork error");
	} else if (id == 0) {  // Hijo
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// 3. Reservar memoria para la pila de excepciones del hijo, e instalar su manejador de excepciones.
	if (sys_page_alloc(id,
	                   (void *) (UXSTACKTOP - PGSIZE),
	                   PTE_P | PTE_W | PTE_U) < 0) {
		panic("sys_page_alloc error");
	}
	if (sys_env_set_pgfault_upcall(id, thisenv->env_pgfault_upcall) < 0)
		panic("sys_env_set_pgfault_upcall error");

	// 4. Iterar sobre el espacio de memoria del padre (desde 0 hasta UTOP)
	// y, para cada página presente, invocar a la función duppage() para
	// mapearla en el hijo.
	for (int i = 0; i < PDX(UTOP); i++) {
		pde_t pde = uvpd[i];
		if (pde & PTE_P) {
			for (int j = 0; j < NPTENTRIES; j++) {
				uintptr_t pg_addr = (uintptr_t) PGADDR(i, j, 0);
				if (pg_addr != (UXSTACKTOP - PGSIZE)) {
					pte_t pte = uvpt[PGNUM(pg_addr)];
					if ((pte & PTE_P))
						duppage(id, PGNUM(pg_addr));
				}
			}
		}
	}

	// 5. Finalizar la ejecución de fork() marcando al proceso hijo como
	// ENV_RUNNABLE sys_env_set_status(id, ENV_RUNNABLE);
	if (sys_env_set_status(id, ENV_RUNNABLE) < 0)
		panic("sys_env_set_status error");

	return id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
