#ifndef ROS_KERN_ARCH_TRAP_H
#define ROS_KERN_ARCH_TRAP_H

#define MSR_IA32_SYSENTER_CS 0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176

// Trap numbers
// These are processor defined:
#define T_DIVIDE     0		// divide error
#define T_DEBUG      1		// debug exception
#define T_NMI        2		// non-maskable interrupt
#define T_BRKPT      3		// breakpoint
#define T_OFLOW      4		// overflow
#define T_BOUND      5		// bounds check
#define T_ILLOP      6		// illegal opcode
#define T_DEVICE     7		// device not available 
#define T_DBLFLT     8		// double fault
/* #define T_COPROC  9 */	// reserved (not generated by recent processors)
#define T_TSS       10		// invalid task switch segment
#define T_SEGNP     11		// segment not present
#define T_STACK     12		// stack exception
#define T_GPFLT     13		// genernal protection fault
#define T_PGFLT     14		// page fault
/* #define T_RES    15 */	// reserved
#define T_FPERR     16		// floating point error
#define T_ALIGN     17		// aligment check
#define T_MCHK      18		// machine check
#define T_SIMDERR   19		// SIMD floating point error

// These are arbitrarily chosen, but with care not to overlap
// processor defined exceptions or interrupt vectors.

// T_SYSCALL is defined by the following include:
#include <ros/arch/syscall.h>

#define T_DEFAULT   0xdeadbeef		// catchall

/* Page faults return the nature of the fault in the bits of the error code: */
#define PF_ERROR_PRESENT 		0x01
#define PF_ERROR_WRITE 			0x02
#define PF_ERROR_USER 			0x04

/* IPIs */
/* Testing IPI (used in testing.c) */
#define I_TESTING		230
/* smp_call_function IPIs, keep in sync with NUM_HANDLER_WRAPPERS (and < 16)
 * it's important that this begins with 0xf0.  check i386/trap.c for details. */
#define I_SMP_CALL0 	0xf0 // 240
#define I_SMP_CALL1 	0xf1
#define I_SMP_CALL2 	0xf2
#define I_SMP_CALL3 	0xf3
#define I_SMP_CALL4 	0xf4
#define I_SMP_CALL_LAST I_SMP_CALL4
/* Direct/Hardwired IPIs.  Hardwired in trapentry.S */
#define I_KERNEL_MSG	255

#ifndef __ASSEMBLER__

#ifndef ROS_KERN_TRAP_H
#error "Do not include include arch/trap.h directly"
#endif

#include <ros/common.h>
#include <arch/mmu.h>
#include <ros/trapframe.h>

/* The kernel's interrupt descriptor table */
extern gatedesc_t idt[];
extern taskstate_t ts;

/* Determines if the given TF was in the kernel or not. */
static inline bool in_kernel(struct hw_trapframe *hw_tf)
{
	return (hw_tf->tf_cs & ~3) == GD_KT;
}

static inline void save_fp_state(struct ancillary_state *silly)
{
	asm volatile("fxsave %0" : : "m"(*silly));
}

/* TODO: this can trigger a GP fault if MXCSR reserved bits are set.  Callers
 * will need to handle intercepting the kernel fault. */
static inline void restore_fp_state(struct ancillary_state *silly)
{
	asm volatile("fxrstor %0" : : "m"(*silly));
}

static inline void init_fp_state(void)
{
	asm volatile("fninit");
}

static inline void __attribute__((always_inline))
set_stack_pointer(uintptr_t sp)
{
	asm volatile("mov %0,%%esp" : : "r"(sp) : "memory","esp");
}

/* Save's the current kernel context into tf, setting the PC to the end of this
 * function.  Note the kernel doesn't need to save a lot. */
static inline void save_kernel_ctx(struct kernel_ctx *ctx)
{
	/* Save the regs and the future esp. */
	asm volatile("movl %%esp,(%0);       " /* save esp in it's slot*/
	             "pushl %%eax;           " /* temp save eax */
	             "leal 1f,%%eax;         " /* get future eip */
	             "movl %%eax,(%1);       " /* store future eip */
	             "popl %%eax;            " /* restore eax */
	             "movl %2,%%esp;         " /* move to the beginning of the tf */
	             "addl $0x20,%%esp;      " /* move to after the push_regs */
	             "pushal;                " /* save regs */
	             "addl $0x44,%%esp;      " /* move to esp slot */
	             "popl %%esp;            " /* restore esp */
	             "1:                     " /* where this tf will restart */
	             : 
	             : "r"(&ctx->hw_tf.tf_esp), "r"(&ctx->hw_tf.tf_eip),
	               "g"(&ctx->hw_tf)
	             : "eax", "memory", "cc");
}

#endif /* !__ASSEMBLER__ */

#endif /* !ROS_INC_ARCH_TRAP_H */
