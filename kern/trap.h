/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

extern void ENTRY_DIVIDE();     /* divide error */
extern void ENTRY_DEBUG();      /* debug exception */
extern void ENTRY_NMI();        /* non-maskable interrupt */
extern void ENTRY_BRKPT();      /* breakpoint */
extern void ENTRY_OFLOW();      /* overflow */
extern void ENTRY_BOUND();      /* bounds check */
extern void ENTRY_ILLOP();      /* illegal opcode */
extern void ENTRY_DEVICE();     /* device not available */
extern void ENTRY_DBLFLT();     /* double fault */
extern void ENTRY_TSS();        /* invalid task switch segment */
extern void ENTRY_SEGNP();      /* segment not present */
extern void ENTRY_STACK();      /* stack exception */
extern void ENTRY_GPFLT();      /* general protection fault */
extern void ENTRY_PGFLT();      /* page fault */
extern void ENTRY_FPERR();      /* floating point error */
extern void ENTRY_ALIGN();      /* aligment check */
extern void ENTRY_MCHK();       /* machine check */
extern void ENTRY_SIMDERR();    /* SIMD floating point error */

#endif /* JOS_KERN_TRAP_H */
