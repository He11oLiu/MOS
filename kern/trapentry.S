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

#define SYSCALLHANDLER(name, num)								\
	.text;														\
	.globl name;													\
	.type name, @function;										\
	.align 2;													\
	name:														\
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
TRAPHANDLER_NOEC(traphandler17, 17)
TRAPHANDLER_NOEC(traphandler18, 18)
TRAPHANDLER_NOEC(traphandler19, 19)
TRAPHANDLER_NOEC(traphandler20, 20)
TRAPHANDLER_NOEC(traphandler21, 21)
TRAPHANDLER_NOEC(traphandler22, 22)
TRAPHANDLER_NOEC(traphandler23, 23)
TRAPHANDLER_NOEC(traphandler24, 24)
TRAPHANDLER_NOEC(traphandler25, 25)
TRAPHANDLER_NOEC(traphandler26, 26)
TRAPHANDLER_NOEC(traphandler27, 27)
TRAPHANDLER_NOEC(traphandler28, 28)
TRAPHANDLER_NOEC(traphandler29, 29)
TRAPHANDLER_NOEC(traphandler30, 30)
TRAPHANDLER_NOEC(traphandler31, 31)
TRAPHANDLER_NOEC(traphandler32, 32)
TRAPHANDLER_NOEC(traphandler33, 33)
TRAPHANDLER_NOEC(traphandler34, 34)
TRAPHANDLER_NOEC(traphandler35, 35)
TRAPHANDLER_NOEC(traphandler36, 36)
TRAPHANDLER_NOEC(traphandler37, 37)
TRAPHANDLER_NOEC(traphandler38, 38)
TRAPHANDLER_NOEC(traphandler39, 39)
TRAPHANDLER_NOEC(traphandler40, 40)
TRAPHANDLER_NOEC(traphandler41, 41)
TRAPHANDLER_NOEC(traphandler42, 42)
TRAPHANDLER_NOEC(traphandler43, 43)
TRAPHANDLER_NOEC(traphandler44, 44)
TRAPHANDLER_NOEC(traphandler45, 45)
TRAPHANDLER_NOEC(traphandler46, 46)
TRAPHANDLER_NOEC(traphandler47, 47)
TRAPHANDLER_NOEC(traphandler48, 48)


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
  	movw $GD_KD,%ax
	movw %ax,%ds
	movw %ax,%es
  	pushl %esp
  	call trap
