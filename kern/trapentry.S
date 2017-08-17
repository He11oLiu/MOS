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
#define TRAPHANDLER(name, num)									\
	.text;														\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */				\
	name:			/* function starts here */					\
	pushl $(num);												\
	jmp _alltraps;												\
	.data;														\
	.long name;

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)								\
	.text;														\
	.globl name;												\
	.type name, @function;										\
	.align 2;													\
	name:														\
	pushl $0;													\
	pushl $(num);												\
	jmp _alltraps;												\
	.data;														\
	.long name;

.data
.globl traphandlers
traphandlers:
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(traphandler0, 0)
TRAPHANDLER_NOEC(traphandler1, 1)
TRAPHANDLER_NOEC(traphandler2, 2)
TRAPHANDLER_NOEC(traphandler3, 3)
TRAPHANDLER_NOEC(traphandler4, 4)
TRAPHANDLER_NOEC(traphandler5, 5)
TRAPHANDLER_NOEC(traphandler6, 6)
TRAPHANDLER_NOEC(traphandler7, 7)
TRAPHANDLER(traphandler8, 8)
TRAPHANDLER(traphandler9, 9)
TRAPHANDLER(traphandler10, 10)
TRAPHANDLER(traphandler11, 11)
TRAPHANDLER(traphandler12, 12)
TRAPHANDLER(traphandler13, 13)
TRAPHANDLER(traphandler14, 14)
TRAPHANDLER(traphandler15, 15)
TRAPHANDLER_NOEC(traphandler16, 16)

/*
 * _alltraps:
 * push values to make the stack look like a struct Trapframe
 * load GD_KD into %ds and %es
 * pushl %esp to pass a pointer to the Trapframe as an argument to trap()
 * call trap (can trap ever return?) nope
 */
_alltraps:
	pushw $0
  	pushw %ds
  	pushw $0
  	pushw %es
	/* %eax->%ecx->%edx->%ebx->%esp->%ebp->%esi->%edi */
  	pushal
  	movw $GD_KD,%eax
	movw %eax,%ds
	movw %eax,%es
  	pushl %esp
  	call trap
