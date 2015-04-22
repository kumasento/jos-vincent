/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


#define TRAPGENERATOR(name, num) \
.data; \
	.long name; \
.text; \
	TRAPHANDLER(name, num)

#define TRAPGENERATOR_NOEC(name, num) \
.data; \
	.long name; \
.text; \
	TRAPHANDLER_NOEC(name, num)	

#define TRAPGENERATOR_EMPTY() \
.data;\
	.long 0

.data
	.p2align 2
	.globl idt_entry
idt_entry:
.text
/*
 * Challenge: my code here
 */
	TRAPGENERATOR_NOEC(trap_divide, T_DIVIDE)
	TRAPGENERATOR_NOEC(trap_debug, T_DEBUG)
	TRAPGENERATOR_EMPTY()
	TRAPGENERATOR_NOEC(trap_brkpt, T_BRKPT)
	TRAPGENERATOR_NOEC(trap_oflow, T_OFLOW)
	TRAPGENERATOR_NOEC(trap_bound, T_BOUND)
	TRAPGENERATOR_NOEC(trap_illop, T_ILLOP)
	TRAPGENERATOR_NOEC(trap_device, T_DEVICE)
	TRAPGENERATOR(trap_dblflt, T_DBLFLT)
	TRAPGENERATOR_EMPTY()
	TRAPGENERATOR(trap_tss, T_TSS)
	TRAPGENERATOR(trap_segnp, T_SEGNP)
	TRAPGENERATOR(trap_stack, T_STACK)
	TRAPGENERATOR(trap_gpflt, T_GPFLT)
	TRAPGENERATOR(trap_pgflt, T_PGFLT)
	TRAPGENERATOR_EMPTY()
	TRAPGENERATOR_NOEC(trap_fperr, T_FPERR)
	TRAPGENERATOR_NOEC(trap_aligh, T_ALIGN)
	TRAPGENERATOR_NOEC(trap_mchk, T_MCHK)
	TRAPGENERATOR_NOEC(trap_simderr, T_SIMDERR) 
.data
	.space 112
.text
	TRAPGENERATOR_NOEC(th48, T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	pushl %ds
	pushl %es
	pushal
	pushl $GD_KD
	popl %ds
	pushl $GD_KD
	popl %es
	pushl %esp
	call trap
