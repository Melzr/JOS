/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



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

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(trap_div_zero , T_DIVIDE)
TRAPHANDLER_NOEC(trap_debug, T_DEBUG)
TRAPHANDLER_NOEC(trap_nmi, T_NMI)
TRAPHANDLER_NOEC(trap_breakpoint, T_BRKPT)
TRAPHANDLER_NOEC(trap_overflow, T_OFLOW)
TRAPHANDLER_NOEC(trap_bound, T_BOUND)
TRAPHANDLER_NOEC(trap_illegal_opcode, T_ILLOP)
TRAPHANDLER_NOEC(trap_device , T_DEVICE)
TRAPHANDLER(trap_double_fault, T_DBLFLT)
TRAPHANDLER(trap_tss, T_TSS)
TRAPHANDLER(trap_segment_not_present, T_SEGNP)
TRAPHANDLER(trap_stack_exception, T_STACK)
TRAPHANDLER(trap_gen_protection_fault, T_GPFLT)
TRAPHANDLER(trap_page_fault, T_PGFLT)
TRAPHANDLER_NOEC(trap_float_error, T_FPERR)
TRAPHANDLER(trap_alignment, T_ALIGN)
TRAPHANDLER_NOEC(trap_machine_check, T_MCHK)
TRAPHANDLER_NOEC(trap_simd , T_SIMDERR)
TRAPHANDLER_NOEC(trap_syscall, T_SYSCALL)
TRAPHANDLER_NOEC(trap_default, T_DEFAULT)


/*
 * Lab 3: Your code here for _alltraps
 */
 _alltraps:
 	pushl %ds;
 	pushl %es;
 	pushal;
 	mov $GD_KD, %ax;
 	mov %ax, %ds;
 	mov %ax, %es;
 	pushl %esp;
 	call trap
