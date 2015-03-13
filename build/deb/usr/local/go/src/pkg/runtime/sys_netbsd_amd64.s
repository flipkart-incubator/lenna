// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// System calls and other sys.stuff for AMD64, NetBSD
// /usr/src/sys/kern/syscalls.master for syscall numbers.
//

#include "zasm_GOOS_GOARCH.h"
#include "../../cmd/ld/textflag.h"

// int32 lwp_create(void *context, uintptr flags, void *lwpid)
TEXT runtime·lwp_create(SB),NOSPLIT,$0
	MOVQ	context+0(FP), DI
	MOVQ	flags+8(FP), SI
	MOVQ	lwpid+16(FP), DX
	MOVL	$309, AX		// sys__lwp_create
	SYSCALL
	JCC	2(PC)
	NEGQ	AX
	RET

TEXT runtime·lwp_tramp(SB),NOSPLIT,$0
	
	// Set FS to point at m->tls.
	LEAQ	m_tls(R8), DI
	CALL	runtime·settls(SB)

	// Set up new stack.
	get_tls(CX)
	MOVQ	R8, m(CX)
	MOVQ	R9, g(CX)
	CALL	runtime·stackcheck(SB)

	// Call fn
	CALL	R12

	// It shouldn't return.  If it does, exit.
	MOVL	$310, AX		// sys__lwp_exit
	SYSCALL
	JMP	-3(PC)			// keep exiting

TEXT runtime·osyield(SB),NOSPLIT,$0
	MOVL	$350, AX		// sys_sched_yield
	SYSCALL
	RET

TEXT runtime·lwp_park(SB),NOSPLIT,$0
	MOVQ	8(SP), DI		// arg 1 - abstime
	MOVL	16(SP), SI		// arg 2 - unpark
	MOVQ	24(SP), DX		// arg 3 - hint
	MOVQ	32(SP), R10		// arg 4 - unparkhint
	MOVL	$434, AX		// sys__lwp_park
	SYSCALL
	RET

TEXT runtime·lwp_unpark(SB),NOSPLIT,$0
	MOVQ	8(SP), DI		// arg 1 - lwp
	MOVL	16(SP), SI		// arg 2 - hint
	MOVL	$321, AX		// sys__lwp_unpark
	SYSCALL
	RET

TEXT runtime·lwp_self(SB),NOSPLIT,$0
	MOVL	$311, AX		// sys__lwp_self
	SYSCALL
	RET

// Exit the entire program (like C exit)
TEXT runtime·exit(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 - exit status
	MOVL	$1, AX			// sys_exit
	SYSCALL
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·exit1(SB),NOSPLIT,$-8
	MOVL	$310, AX		// sys__lwp_exit
	SYSCALL
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·open(SB),NOSPLIT,$-8
	MOVQ	8(SP), DI		// arg 1 pathname
	MOVL	16(SP), SI		// arg 2 flags
	MOVL	20(SP), DX		// arg 3 mode
	MOVL	$5, AX
	SYSCALL
	RET

TEXT runtime·close(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 fd
	MOVL	$6, AX
	SYSCALL
	RET

TEXT runtime·read(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 fd
	MOVQ	16(SP), SI		// arg 2 buf
	MOVL	24(SP), DX		// arg 3 count
	MOVL	$3, AX
	SYSCALL
	RET

TEXT runtime·write(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 - fd
	MOVQ	16(SP), SI		// arg 2 - buf
	MOVL	24(SP), DX		// arg 3 - nbyte
	MOVL	$4, AX			// sys_write
	SYSCALL
	RET

TEXT runtime·usleep(SB),NOSPLIT,$16
	MOVL	$0, DX
	MOVL	usec+0(FP), AX
	MOVL	$1000000, CX
	DIVL	CX
	MOVQ	AX, 0(SP)		// tv_sec
	MOVL	$1000, AX
	MULL	DX
	MOVQ	AX, 8(SP)		// tv_nsec

	MOVQ	SP, DI			// arg 1 - rqtp
	MOVQ	$0, SI			// arg 2 - rmtp
	MOVL	$430, AX		// sys_nanosleep
	SYSCALL
	RET

TEXT runtime·raise(SB),NOSPLIT,$16
	MOVL	$311, AX		// sys__lwp_self
	SYSCALL
	MOVQ	AX, DI			// arg 1 - target
	MOVL	sig+0(FP), SI		// arg 2 - signo
	MOVL	$318, AX		// sys__lwp_kill
	SYSCALL
	RET

TEXT runtime·setitimer(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 - which
	MOVQ	16(SP), SI		// arg 2 - itv
	MOVQ	24(SP), DX		// arg 3 - oitv
	MOVL	$425, AX		// sys_setitimer
	SYSCALL
	RET

// func now() (sec int64, nsec int32)
TEXT time·now(SB), NOSPLIT, $32
	MOVQ	$0, DI			// arg 1 - clock_id
	LEAQ	8(SP), SI		// arg 2 - tp
	MOVL	$427, AX		// sys_clock_gettime
	SYSCALL
	MOVQ	8(SP), AX		// sec
	MOVL	16(SP), DX		// nsec

	// sec is in AX, nsec in DX
	MOVQ	AX, sec+0(FP)
	MOVL	DX, nsec+8(FP)
	RET

TEXT runtime·nanotime(SB),NOSPLIT,$32
	MOVQ	$0, DI			// arg 1 - clock_id
	LEAQ	8(SP), SI		// arg 2 - tp
	MOVL	$427, AX		// sys_clock_gettime
	SYSCALL
	MOVQ	8(SP), AX		// sec
	MOVL	16(SP), DX		// nsec

	// sec is in AX, nsec in DX
	// return nsec in AX
	IMULQ	$1000000000, AX
	ADDQ	DX, AX
	RET

TEXT runtime·getcontext(SB),NOSPLIT,$-8
	MOVQ	8(SP), DI		// arg 1 - context
	MOVL	$307, AX		// sys_getcontext
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·sigprocmask(SB),NOSPLIT,$0
	MOVL	8(SP), DI		// arg 1 - how
	MOVQ	16(SP), SI		// arg 2 - set
	MOVQ	24(SP), DX		// arg 3 - oset
	MOVL	$293, AX		// sys_sigprocmask
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·sigreturn_tramp(SB),NOSPLIT,$-8
	MOVQ	R15, DI			// Load address of ucontext
	MOVQ	$308, AX		// sys_setcontext
	SYSCALL
	MOVQ	$-1, DI			// Something failed...
	MOVL	$1, AX			// sys_exit
	SYSCALL

TEXT runtime·sigaction(SB),NOSPLIT,$-8
	MOVL	8(SP), DI		// arg 1 - signum
	MOVQ	16(SP), SI		// arg 2 - nsa
	MOVQ	24(SP), DX		// arg 3 - osa
					// arg 4 - tramp
	LEAQ	runtime·sigreturn_tramp(SB), R10
	MOVQ	$2, R8			// arg 5 - vers
	MOVL	$340, AX		// sys___sigaction_sigtramp
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·sigtramp(SB),NOSPLIT,$64
	get_tls(BX)

	// check that m exists
	MOVQ	m(BX), BP
	CMPQ	BP, $0
	JNE	5(PC)
	MOVQ	DI, 0(SP)
	MOVQ	$runtime·badsignal(SB), AX
	CALL	AX
	RET

	// save g
	MOVQ	g(BX), R10
	MOVQ	R10, 40(SP)

	// g = m->signal
	MOVQ	m_gsignal(BP), BP
	MOVQ	BP, g(BX)

	MOVQ	DI, 0(SP)
	MOVQ	SI, 8(SP)
	MOVQ	DX, 16(SP)
	MOVQ	R10, 24(SP)

	CALL	runtime·sighandler(SB)

	// restore g
	get_tls(BX)
	MOVQ	40(SP), R10
	MOVQ	R10, g(BX)
	RET

TEXT runtime·mmap(SB),NOSPLIT,$0
	MOVQ	8(SP), DI		// arg 1 - addr
	MOVQ	16(SP), SI		// arg 2 - len
	MOVL	24(SP), DX		// arg 3 - prot
	MOVL	28(SP), R10		// arg 4 - flags
	MOVL	32(SP), R8		// arg 5 - fd
	MOVQ	36(SP), R9
	SUBQ	$16, SP
	MOVQ	R9, 8(SP)		// arg 7 - offset (passed on stack)
	MOVQ	$0, R9			// arg 6 - pad
	MOVL	$197, AX		// sys_mmap
	SYSCALL
	ADDQ	$16, SP
	RET

TEXT runtime·munmap(SB),NOSPLIT,$0
	MOVQ	8(SP), DI		// arg 1 - addr
	MOVQ	16(SP), SI		// arg 2 - len
	MOVL	$73, AX			// sys_munmap
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET


TEXT runtime·madvise(SB),NOSPLIT,$0
	MOVQ	addr+0(FP), DI		// arg 1 - addr
	MOVQ	len+8(FP), SI		// arg 2 - len
	MOVQ	behav+16(FP), DX	// arg 3 - behav
	MOVQ	$75, AX			// sys_madvise
	SYSCALL
	// ignore failure - maybe pages are locked
	RET

TEXT runtime·sigaltstack(SB),NOSPLIT,$-8
	MOVQ	new+8(SP), DI		// arg 1 - nss
	MOVQ	old+16(SP), SI		// arg 2 - oss
	MOVQ	$281, AX		// sys___sigaltstack14
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET

// set tls base to DI
TEXT runtime·settls(SB),NOSPLIT,$8
	// adjust for ELF: wants to use -16(FS) and -8(FS) for g and m
	ADDQ	$16, DI			// arg 1 - ptr
	MOVQ	$317, AX		// sys__lwp_setprivate
	SYSCALL
	JCC	2(PC)
	MOVL	$0xf1, 0xf1		// crash
	RET

TEXT runtime·sysctl(SB),NOSPLIT,$0
	MOVQ	8(SP), DI		// arg 1 - name
	MOVL	16(SP), SI		// arg 2 - namelen
	MOVQ	24(SP), DX		// arg 3 - oldp
	MOVQ	32(SP), R10		// arg 4 - oldlenp
	MOVQ	40(SP), R8		// arg 5 - newp
	MOVQ	48(SP), R9		// arg 6 - newlen
	MOVQ	$202, AX		// sys___sysctl
	SYSCALL
	JCC 3(PC)
	NEGQ	AX
	RET
	MOVL	$0, AX
	RET

// int32 runtime·kqueue(void)
TEXT runtime·kqueue(SB),NOSPLIT,$0
	MOVQ	$0, DI
	MOVL	$344, AX
	SYSCALL
	JCC	2(PC)
	NEGQ	AX
	RET

// int32 runtime·kevent(int kq, Kevent *changelist, int nchanges, Kevent *eventlist, int nevents, Timespec *timeout)
TEXT runtime·kevent(SB),NOSPLIT,$0
	MOVL	8(SP), DI
	MOVQ	16(SP), SI
	MOVL	24(SP), DX
	MOVQ	32(SP), R10
	MOVL	40(SP), R8
	MOVQ	48(SP), R9
	MOVL	$435, AX
	SYSCALL
	JCC	2(PC)
	NEGQ	AX
	RET

// void runtime·closeonexec(int32 fd)
TEXT runtime·closeonexec(SB),NOSPLIT,$0
	MOVL	8(SP), DI	// fd
	MOVQ	$2, SI		// F_SETFD
	MOVQ	$1, DX		// FD_CLOEXEC
	MOVL	$92, AX		// fcntl
	SYSCALL
	RET
