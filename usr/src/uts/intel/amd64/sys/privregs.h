/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _AMD64_SYS_PRIVREGS_H
#define	_AMD64_SYS_PRIVREGS_H

#pragma ident	"@(#)privregs.h	1.4	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file describes the cpu's privileged register set, and
 * how the machine state is saved on the stack when a trap occurs.
 */

#if !defined(__amd64)
#error	"non-amd64 code depends on amd64 privileged header!"
#endif

#ifndef	_ASM

/*
 * This is NOT the structure to use for general purpose debugging;
 * see /proc for that.  This is NOT the structure to use to decode
 * the ucontext or grovel about in a core file; see <sys/regset.h>.
 */

struct regs {
	/*
	 * Extra frame for mdb to follow through high level interrupts and
	 * system traps.  Set them to 0 to terminate stacktrace.
	 */
	greg_t	r_savfp;	/* a copy of %rbp */
	greg_t	r_savpc;	/* a copy of %rip */

	greg_t	r_rdi;		/* 1st arg to function */
	greg_t	r_rsi;		/* 2nd arg to function */
	greg_t	r_rdx;		/* 3rd arg to function, 2nd return register */
	greg_t	r_rcx;		/* 4th arg to function */

	greg_t	r_r8;		/* 5th arg to function */
	greg_t	r_r9;		/* 6th arg to function */
	greg_t	r_rax;		/* 1st return register, # SSE registers */
	greg_t	r_rbx;		/* callee-saved, optional base pointer */

	greg_t	r_rbp;		/* callee-saved, optional frame pointer */
	greg_t	r_r10;		/* temporary register, static chain pointer */
	greg_t	r_r11;		/* temporary register */
	greg_t	r_r12;		/* callee-saved */

	greg_t	r_r13;		/* callee-saved */
	greg_t	r_r14;		/* callee-saved */
	greg_t	r_r15;		/* callee-saved */

	greg_t	r_fsbase;
	greg_t	r_gsbase;
	greg_t	r_ds;
	greg_t	r_es;
	greg_t	r_fs;		/* %fs is *never* used by the kernel */
	greg_t	r_gs;

	greg_t	r_trapno;

	/*
	 * (the rest of these are defined by the hardware)
	 */
	greg_t	r_err;
	greg_t	r_rip;
	greg_t	r_cs;
	greg_t	r_rfl;
	greg_t	r_rsp;
	greg_t	r_ss;
};

#define	r_r0	r_rax	/* r0 for portability */
#define	r_r1	r_rdx	/* r1 for portability */
#define	r_fp	r_rbp	/* kernel frame pointer */
#define	r_sp	r_rsp	/* user stack pointer */
#define	r_pc	r_rip	/* user's instruction pointer */
#define	r_ps	r_rfl	/* user's RFLAGS */

#ifdef _KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#endif	/* _KERNEL */

#else	/* !_ASM */

#define	TRAPERR_PUSH(err, trapno)	\
	pushq	$err;			\
	pushq	$trapno

/*
 * Create a struct regs on the stack suitable for an
 * interrupt trap.
 *
 * The swapgs instruction is conditionalized to make sure that
 * interrupts in kernel mode don't cause us to switch back to
 * the user's gsbase!
 *
 * Assumes that the trap handler has already pushed an
 * appropriate r_err and r_trapno
 */
#define	__SAVE_REGS				\
	movq	%r15, REGOFF_R15(%rsp);		\
	movq	%r14, REGOFF_R14(%rsp);		\
	movq	%r13, REGOFF_R13(%rsp);		\
	movq	%r12, REGOFF_R12(%rsp);		\
	movq	%r11, REGOFF_R11(%rsp);		\
	movq	%r10, REGOFF_R10(%rsp);		\
	movq	%rbp, REGOFF_RBP(%rsp);		\
	movq	%rbx, REGOFF_RBX(%rsp);		\
	movq	%rax, REGOFF_RAX(%rsp);		\
	movq	%r9, REGOFF_R9(%rsp);		\
	movq	%r8, REGOFF_R8(%rsp);		\
	movq	%rcx, REGOFF_RCX(%rsp);		\
	movq	%rdx, REGOFF_RDX(%rsp);		\
	movq	%rsi, REGOFF_RSI(%rsp);		\
	movq	%rdi, REGOFF_RDI(%rsp);		\
	movq	%rbp, REGOFF_SAVFP(%rsp);	\
	movq	REGOFF_RIP(%rsp), %rcx;		\
	movq	%rcx, REGOFF_SAVPC(%rsp);	\
	xorl	%ecx, %ecx;			\
	movw	%gs, %cx;			\
	movq	%rcx, REGOFF_GS(%rsp);		\
	movw	%fs, %cx;			\
	movq	%rcx, REGOFF_FS(%rsp);		\
	movw	%es, %cx;			\
	movq	%rcx, REGOFF_ES(%rsp);		\
	movw	%ds, %cx;			\
	movq	%rcx, REGOFF_DS(%rsp);		\
	movl	$MSR_AMD_FSBASE, %ecx;		\
	rdmsr;					\
	movl	%eax, REGOFF_FSBASE(%rsp);	\
	movl	%edx, REGOFF_FSBASE+4(%rsp);	\
	movl	$MSR_AMD_GSBASE, %ecx;		\
	rdmsr;					\
	movl	%eax, REGOFF_GSBASE(%rsp);	\
	movl	%edx, REGOFF_GSBASE+4(%rsp)

/*
 * Push register state onto the stack. If we've
 * interrupted userland, do a swapgs as well.
 */

#define	INTR_PUSH				\
	subq	$REGOFF_TRAPNO, %rsp;		\
	__SAVE_REGS;				\
	cmpw	$KCS_SEL, REGOFF_CS(%rsp);	\
	je	6f;				\
	movq	$0, REGOFF_SAVFP(%rsp);		\
	swapgs;					\
6:

#define	TRAP_PUSH				\
	subq	$REGOFF_TRAPNO, %rsp;		\
	__SAVE_REGS;				\
	cmpw	$KCS_SEL, REGOFF_CS(%rsp);	\
	je	6f;				\
	movq	$0, REGOFF_SAVFP(%rsp);		\
	swapgs;					\
6:

#define	DFTRAP_PUSH				\
	subq	$REGOFF_TRAPNO, %rsp;		\
	__SAVE_REGS

#define	__RESTORE_REGS			\
	movq	REGOFF_RDI(%rsp),	%rdi;	\
	movq	REGOFF_RSI(%rsp),	%rsi;	\
	movq	REGOFF_RDX(%rsp),	%rdx;	\
	movq	REGOFF_RCX(%rsp),	%rcx;	\
	movq	REGOFF_R8(%rsp),	%r8;	\
	movq	REGOFF_R9(%rsp),	%r9;	\
	movq	REGOFF_RAX(%rsp),	%rax;	\
	movq	REGOFF_RBX(%rsp),	%rbx;	\
	movq	REGOFF_RBP(%rsp),	%rbp;	\
	movq	REGOFF_R10(%rsp),	%r10;	\
	movq	REGOFF_R11(%rsp),	%r11;	\
	movq	REGOFF_R12(%rsp),	%r12;	\
	movq	REGOFF_R13(%rsp),	%r13;	\
	movq	REGOFF_R14(%rsp),	%r14;	\
	movq	REGOFF_R15(%rsp),	%r15

#define	INTR_POP			\
	leaq	sys_lcall32(%rip), %r11;\
	cmpq	%r11, REGOFF_RIP(%rsp);	\
	__RESTORE_REGS;			\
	je	5f;			\
	cmpw	$KCS_SEL, REGOFF_CS(%rsp);\
	je	8f;			\
5:	swapgs;				\
8:	addq	$REGOFF_RIP, %rsp

#define	USER_POP			\
	__RESTORE_REGS;			\
	swapgs;				\
	addq	$REGOFF_RIP, %rsp	/* Adjust %rsp to prepare for iretq */

#define	USER32_POP			\
	movl	REGOFF_RDI(%rsp), %edi;	\
	movl	REGOFF_RSI(%rsp), %esi;	\
	movl	REGOFF_RDX(%rsp), %edx;	\
	movl	REGOFF_RCX(%rsp), %ecx;	\
	movl	REGOFF_RAX(%rsp), %eax;	\
	movl	REGOFF_RBX(%rsp), %ebx;	\
	movl	REGOFF_RBP(%rsp), %ebp;	\
	swapgs;				\
	addq	$REGOFF_RIP, %rsp	/* Adjust %rsp to prepare for iretq */


/*
 * Smaller versions of INTR_PUSH and INTR_POP for fast traps.
 * The following registers have been pushed onto the stack by
 * hardware at this point:
 *
 *	greg_t	r_rip;
 *	greg_t	r_cs;
 *	greg_t	r_rfl;
 *	greg_t	r_rsp;
 *	greg_t	r_ss;
 *
 * This handler is executed both by 32-bit and 64-bit applications.
 * 64-bit applications allow us to treat the set (%rdi, %rsi, %rdx,
 * %rcx, %r8, %r9, %r10, %r11, %rax) as volatile across function calls.
 * However, 32-bit applications only expect (%eax, %edx, %ecx) to be volatile
 * across a function call -- in particular, %esi and %edi MUST be saved!
 *
 * We could do this differently by making a FAST_INTR_PUSH32 for 32-bit
 * programs, and FAST_INTR_PUSH for 64-bit programs, but it doesn't seem
 * particularly worth it.
 */
#define	FAST_INTR_PUSH			\
	subq	$REGOFF_RIP, %rsp;	\
	movq	%rsi, REGOFF_RSI(%rsp);	\
	movq	%rdi, REGOFF_RDI(%rsp);	\
	swapgs

#define	FAST_INTR_POP			\
	swapgs;				\
	movq	REGOFF_RSI(%rsp), %rsi;	\
	movq	REGOFF_RDI(%rsp), %rdi;	\
	addq	$REGOFF_RIP, %rsp

#define	DISABLE_INTR_FLAGS		\
	pushq	$F_OFF;			\
	popfq

#define	ENABLE_INTR_FLAGS		\
	pushq	$F_ON;			\
	popfq

#endif	/* !_ASM */

#include <sys/controlregs.h>

#if defined(_KERNEL) && !defined(_ASM)
#if !defined(__lint) && defined(__GNUC__)

extern __inline__ ulong_t getcr8(void)
{
	uint64_t value;

	__asm__ __volatile__(
		"movq %%cr8, %0"
		: "=r" (value));
	return (value);
}

extern __inline__ void setcr8(ulong_t value)
{
	__asm__ __volatile__(
		"movq %0, %%cr8"
		: /* no output */
		: "r" (value));
}

#else

extern ulong_t getcr8(void);
extern void setcr8(ulong_t);

#endif	/* !defined(__lint) && defined(__GNUC__) */
#endif	/* _KERNEL && !_ASM */

/* Control register layout for panic dump */

#define	CREGSZ		0x68
#define	CREG_GDT	0
#define	CREG_IDT	0x10
#define	CREG_LDT	0x20
#define	CREG_TASKR	0x28
#define	CREG_CR0	0x30
#define	CREG_CR2	0x38
#define	CREG_CR3	0x40
#define	CREG_CR4	0x48
#define	CREG_CR8	0x50
#define	CREG_KGSBASE	0x58
#define	CREG_EFER	0x60

#if !defined(_ASM)

typedef	uint64_t	creg64_t;
typedef	upad128_t	creg128_t;

struct cregs {
	creg128_t	cr_gdt;
	creg128_t	cr_idt;
	creg64_t	cr_ldt;
	creg64_t	cr_task;
	creg64_t	cr_cr0;
	creg64_t	cr_cr2;
	creg64_t	cr_cr3;
	creg64_t	cr_cr4;
	creg64_t	cr_cr8;
	creg64_t	cr_kgsbase;
	creg64_t	cr_efer;
};

#if defined(_KERNEL)
extern void getcregs(struct cregs *);
#endif	/* _KERNEL */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _AMD64_SYS_PRIVREGS_H */
