// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/pmap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "showmappings", "Display in a useful and easy-to-read format all of the physical page mappings", mon_showmappings},
	{ "setmapping", "Reset physical address mapping", mon_setmapping},
	{ "dump", "Dump physical or virtual address", mon_dump},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
    int i;
    uint32_t ebp, eip, args[5];
    struct Eipdebuginfo info;

    cprintf("Stack backtrace:\n");
    ebp = read_ebp();
    while (ebp != 0x0)
    {
        eip = *((uint32_t *)ebp+0x1);
        for (i = 0; i < 5; i++)
            args[i] = *((uint32_t*) ebp+2+i);
        cprintf("  ebp %08x  eip %08x", ebp, eip);
        cprintf("  args %08x %08x %08x %08x %08x\n",
                args[0], args[1], args[2], args[3], args[4]);

        if (debuginfo_eip(eip, &info) != -1)
        {
            cprintf("        %s:",  info.eip_file);
            cprintf("%d: ",         info.eip_line);
            cprintf("%.*s",         info.eip_fn_namelen, info.eip_fn_name);
            cprintf("+%u\n",        eip-(uint32_t)info.eip_fn_addr);
        }

        ebp = *((uint32_t *)ebp);
    }
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc <= 2) {
		cprintf("Usage: %s [pa_start] [pa_end]\n", argv[0]);
		return 1;
	}
	physaddr_t pa_start, pa_end;
	pa_start = strtol(argv[1], 0, 0);
	pa_end = strtol(argv[2], 0, 0);

	pte_t * pte_addr;
	while (pa_start < pa_end) {
		pte_addr = pgdir_walk(kern_pgdir, (void *) pa_start, 0);
		if (pte_addr == NULL) {
			cprintf("%x: No such page table\n", pa_start);
		}
		else if (!(*pte_addr & PTE_P)) {
			cprintf("%x: Not mapped(PTE_P set to 0)\n", pa_start);
		}
		else {
			cprintf("%x: mapped to %x\n", pa_start, PTE_ADDR(*pte_addr));
			cprintf("%x: PTE_U %d\n", pa_start, *pte_addr & PTE_U);
			cprintf("%x: PTE_W %d\n", pa_start, *pte_addr & PTE_W);
		}
		pa_start = pa_start + PGSIZE;
	}
	return 0;
}

int
mon_setmapping(int argc, char **argv, struct Trapframe *tf)
{
	if (argc <= 4) {
		cprintf("Usage: %s [va] [size] [pa] [perm]\n", argv[0]);
		return 1;
	}
	uintptr_t va = strtol(argv[1], 0, 0);
	uintptr_t pa = strtol(argv[3], 0, 0);
	uintptr_t size = strtol(argv[2], 0, 0) * PGSIZE;
	uint32_t perm = 0;
	int i;
	for (i = 0; i < strlen(argv[4]); i++) {
		perm |= (argv[4][i] == 'U') ? PTE_U : PTE_W;
	}
	size_t tsize = 0;
	for (; tsize < size; tsize += PGSIZE) {
		uintptr_t vaddr = va + tsize;
		uintptr_t paddr = pa + tsize;
		pte_t * pte = pgdir_walk(kern_pgdir, (const void *)vaddr, (int)1);
		if (pte == NULL)
			continue;
		*pte = paddr | perm | PTE_P;
	}
	return 0;
}
int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	if (argc <= 3) {
		cprintf("Usage: %s [addr_start] [addr_end] [type]\n", argv[0]);
		return 1;
	}
	uintptr_t addr_start = strtol(argv[1], 0, 0);
	uintptr_t addr_end = strtol(argv[2], 0, 0);

	if (argv[3][0] == 'P') {
		addr_start += KERNBASE;
		addr_end += KERNBASE;
	}

	pte_t * pte_addr;
	while (addr_start < addr_end) {
		pte_addr = pgdir_walk(kern_pgdir, (void *) addr_start, 0);
		if (pte_addr == NULL) {
			cprintf("%x: No such page table\n", addr_start);
			break;
		}
		else if (!(*pte_addr & PTE_P)) {
			cprintf("%x: Not mapped(PTE_P set to 0)\n", addr_start);
			break;
		}
		uint32_t offset = PGOFF(addr_start);
		uint32_t *va = KADDR(PTE_ADDR(*pte_addr)) + offset;
		int i;
		for (i = 0; offset + i < NPTENTRIES; i++) {
			cprintf("%x->%p: %x\n", addr_start, va, *va);
			addr_start = addr_start + 1;
		}
	}

	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);
    user_vga_color = VGA_COLOR_RED;
    cprintf("Hello Every one I'm the little RED Hat!\n");
    user_vga_color = VGA_COLOR_GREEN;
    cprintf("Green Green I'm Green!\n");
    user_vga_color = VGA_COLOR_BLUE;
    cprintf("Blue Whale\n");
    user_vga_color = VGA_COLOR_BLACK;

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
