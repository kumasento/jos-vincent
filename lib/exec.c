#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)	((void*) (addr) + (USTACKTOP - PGSIZE) - UTEMP)
#define UTEMP2			(UTEMP + PGSIZE)
#define UTEMP3			(UTEMP2 + PGSIZE)

// Helper functions for exec.
static int init_stack(const char **argv, uintptr_t *init_esp);
static int read_segment(size_t memsz, int fd, size_t filesz, off_t fileoffset, 
	uint32_t va, uint32_t* alloc_va);
static int map_segment(uintptr_t va, size_t memsz,
		       int fd, size_t filesz, off_t fileoffset, int perm);
static int copy_shared_pages(envid_t child);

// this file defined the exec function, the basic difference between the
// spawn and exec is that, we need to change the current env to run other 
// program, so it would be harder to do that

// But I think the hardest part is that, we don't have variable space allocation
// method to allocate a memory, just like malloc
// So here we could allocate a space in the bss segment, by assigning a boundary
// MAX_PROG_SIZE. 

#define MAX_PROG_SIZE PGSIZE * 5
#define MAX_PROGHDR_NUM 10

int 
exec(const char *prog, const char **argv) {
	cprintf("running exec...\n");
	//unsigned char prog_buf [MAX_PROG_SIZE];
	unsigned char elf_buf [512];
	//unsigned char stack_buf [MAX_STACK_SIZE]; // a place for placing new stack
	// some preparation
	// store the va of each program header
	uint32_t ph_buf[MAX_PROGHDR_NUM];
	uint32_t alloc_va;
	uint32_t va;

	int fd, i, r;
	struct Elf *elf;
	struct Proghdr *ph;
	int perm;

	// if you can't open the program file, just return
	if ((r = open(prog, O_RDONLY)) < 0) 
		return r;
	fd = r; // if succeed, assign the fd with value r

	// then read the ELF header
	elf = (struct Elf*) elf_buf; // elf will "parse" the content in elf_buf
	// read size as elf_buf from fd
	// and check the content of ELF header
	if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
	    || elf->e_magic != ELF_MAGIC) {
		close(fd);
		cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
		return -E_NOT_EXEC;
	}

	// here we will have a new trapframe and a new stack for 
	// the new program
	struct Trapframe new_tf;
	new_tf.tf_eip = elf->e_entry;

	// init the stack_buf space, by using the argv pointers
	if ((r = init_stack(argv, &new_tf.tf_esp)) < 0) 
		return r;
	cprintf("init stack at position 0x%08x\n", new_tf.tf_esp);

	// load data into position UTEMP2
	if (elf->e_phnum > MAX_PROGHDR_NUM) 
		panic("too many program header");

	alloc_va = (uint32_t)UTEMP2;
	va = (uint32_t) UTEMP2;
	ph = (struct Proghdr*) (elf_buf + elf->e_phoff);
	for (i = 0; i < elf->e_phnum; i++, ph++) {
		if (ph->p_type != ELF_PROG_LOAD)
			continue;
		perm = PTE_P | PTE_U;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;
		if ((r = read_segment(ph->p_memsz, fd, ph->p_filesz, ph->p_offset, 
				va, &alloc_va)) < 0)
			goto error;
		va = alloc_va;
		ph_buf[i] = alloc_va; // set the start va for this prog header
	}
	close(fd);
	fd = -1;

	return 0;
error:
	sys_env_destroy(0);
	close(fd);
	return r;
}

int
execl(const char *prog, const char *arg0, ...)
{
	// We calculate argc by advancing the args until we hit NULL.
	// The contract of the function guarantees that the last
	// argument will always be NULL, and that none of the other
	// arguments will be NULL.
	int argc=0;
	va_list vl;
	va_start(vl, arg0);
	while(va_arg(vl, void *) != NULL)
		argc++;
	va_end(vl);
	// Now that we have the size of the args, do a second pass
	// and store the values in a VLA, which has the format of argv
	const char *argv[argc+2];
	argv[0] = arg0;
	argv[argc+1] = NULL;

	va_start(vl, arg0);
	unsigned i;
	for(i=0;i<argc;i++)
		argv[i+1] = va_arg(vl, const char *);
	va_end(vl);
	cprintf("finished parsing parameter list ....\n");
	return exec(prog, argv);
}

// helper function from spawn.c



// Set up the initial stack page for the new child process with envid 'child'
// using the arguments array pointed to by 'argv',
// which is a null-terminated array of pointers to null-terminated strings.
//
// On success, returns 0 and sets *init_esp
// to the initial stack pointer with which the child should start.
// Returns < 0 on failure.
static int
init_stack(const char **argv, uintptr_t *init_esp)
{
	size_t string_size;
	int argc, i, r;
	char *string_store;
	uintptr_t *argv_store;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (string_size).
	string_size = 0;
	for (argc = 0; argv[argc] != 0; argc++)
		string_size += strlen(argv[argc]) + 1;

	//cprintf("child %d argc %d\n", child, argc);

	// Determine where to place the strings and the argv array.
	// Set up pointers into the temporary page 'UTEMP'; we'll map a page
	// there later, then remap that page into the child environment
	// at (USTACKTOP - PGSIZE).
	// strings is the topmost thing on the stack.
	string_store = (char*) UTEMP + PGSIZE - string_size;
	// argv is below that.  There's one argument pointer per argument, plus
	// a null pointer.
	argv_store = (uintptr_t*) (ROUNDDOWN(string_store, 4) - 4 * (argc + 1));

	// Make sure that argv, strings, and the 2 words that hold 'argc'
	// and 'argv' themselves will all fit in a single stack page.
	if ((void*) (argv_store - 2) < (void*) UTEMP)
		return -E_NO_MEM;

	// Allocate the single stack page at UTEMP.
	if ((r = sys_page_alloc(0, (void*) UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		return r;


	//	* Initialize 'argv_store[i]' to point to argument string i,
	//	  for all 0 <= i < argc.
	//	  Also, copy the argument strings from 'argv' into the
	//	  newly-allocated stack page.
	//
	//	* Set 'argv_store[argc]' to 0 to null-terminate the args array.
	//
	//	* Push two more words onto the child's stack below 'args',
	//	  containing the argc and argv parameters to be passed
	//	  to the child's umain() function.
	//	  argv should be below argc on the stack.
	//	  (Again, argv should use an address valid in the child's
	//	  environment.)
	//
	//	* Set *init_esp to the initial stack pointer for the child,
	//	  (Again, use an address valid in the child's environment.)
	for (i = 0; i < argc; i++) {
		argv_store[i] = UTEMP2USTACK(string_store);
		strcpy(string_store, argv[i]);
		string_store += strlen(argv[i]) + 1;
	}
	argv_store[argc] = 0;
	assert(string_store == (char*)UTEMP + PGSIZE);

	argv_store[-1] = UTEMP2USTACK(argv_store);
	argv_store[-2] = argc;

	*init_esp = UTEMP2USTACK(&argv_store[-2]);

	// After completing the stack, map it into the child's address space
	// and unmap it from ours!
	//if ((r = sys_page_map(0, UTEMP, child, (void*) (USTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
	//	goto error;
	//if ((r = sys_page_unmap(0, UTEMP)) < 0)
	//	goto error;

	return 0;

error:
	sys_page_unmap(0, UTEMP);
	return r;
}

static int
map_segment(uintptr_t va, size_t memsz,
	int fd, size_t filesz, off_t fileoffset, int perm)
{
	int i, r;
	void *blk;

	//cprintf("map_segment %x+%x\n", va, memsz);

	if ((i = PGOFF(va))) {
		va -= i;
		memsz += i;
		filesz += i;
		fileoffset -= i;
	}

	for (i = 0; i < memsz; i += PGSIZE) {
		if (i >= filesz) {
			// allocate a blank page
			// if ((r = sys_page_alloc(child, (void*) (va + i), perm)) < 0)
			// 	return r;
		} else {
			// from file
			cprintf("allocating page at 0x%08x ...\n", UTEMP2 + i);
			if ((r = sys_page_alloc(0, UTEMP2 + i, PTE_P|PTE_U|PTE_W)) < 0)
				return r;
			if ((r = seek(fd, fileoffset + i)) < 0)
				return r;
			if ((r = readn(fd, UTEMP2 + i, MIN(PGSIZE, filesz-i))) < 0)
				return r;
			//if ((r = sys_page_map(0, UTEMP, child, (void*) (va + i), perm)) < 0)
			//	panic("spawn: sys_page_map data: %e", r);
			//sys_page_unmap(0, UTEMP);
		}
	}
	return 0;
}

// different from map_segment, this part of code will load the file
// from fd, and write data start from 
static int
read_segment(size_t memsz, int fd, size_t filesz, off_t fileoffset, 
	uint32_t va, uint32_t* alloc_va)
{
	int i, r;
	void *blk;

	cprintf("map_segment %x+%x\n", va, memsz);

	if ((i = PGOFF(va))) {
		va -= i;
		memsz += i;
		filesz += i;
		fileoffset -= i;
	}

	for (i = 0; i < memsz; i += PGSIZE) {
		if (i >= filesz) {
			// allocate a blank page
			// if ((r = sys_page_alloc(child, (void*) (va + i), perm)) < 0)
			// 	return r;
		} else {
			// from file
			cprintf("allocating page at 0x%08x ...\n", (uint32_t)va + i);
			if ((r = sys_page_alloc(0, (void*)(va + i), PTE_P|PTE_U|PTE_W)) < 0)
				return r;
			if ((r = seek(fd, fileoffset + i)) < 0)
				return r;
			if ((r = readn(fd, (void*)(va + i), MIN(PGSIZE, filesz-i))) < 0)
				return r;
			*alloc_va = va + i + PGSIZE;
			//if ((r = sys_page_map(0, UTEMP, child, (void*) (va + i), perm)) < 0)
			//	panic("spawn: sys_page_map data: %e", r);
			//sys_page_unmap(0, UTEMP);
		}
	}
	return 0;
}

// Copy the mappings for shared pages into the child address space.
static int
copy_shared_pages(envid_t child)
{
	// LAB 5: Your code here.
	uint32_t addr;
	for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE) {
		//cprintf("addr: 0x%08x\n", addr);
		if ((uvpd[PDX(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_P) && 
			(uvpt[PGNUM(addr)] & PTE_U) &&
			(uvpt[PGNUM(addr)] & PTE_SHARE)) {
			if (sys_page_map(0, (void*)addr, child, (void*)addr, uvpt[PGNUM(addr)] & PTE_SYSCALL) < 0)
				panic("copy_shared_pages: can't copy page 0x%08x", addr);
		}
	}
	return 0;
}
