// Copyright 2013 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
// System calls and other sys.stuff for ARM, NetBSD
// /usr/src/sys/kern/syscalls.master for syscall numbers.
//

#include "zasm_GOOS_GOARCH.h"
#include "../../cmd/ld/textflag.h"

// Exit the entire program (like C exit)
TEXT runtime·exit(SB),NOSPLIT,$-4
	MOVW 0(FP), R0	// arg 1 exit status
	SWI $0xa00001
	MOVW.CS $0, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·exit1(SB),NOSPLIT,$-4
	SWI $0xa00136	// sys__lwp_exit
	MOVW $1, R8	// crash
	MOVW R8, (R8)
	RET
	
TEXT runtime·open(SB),NOSPLIT,$-8
	MOVW 0(FP), R0
	MOVW 4(FP), R1
	MOVW 8(FP), R2
	SWI $0xa00005
	RET

TEXT runtime·close(SB),NOSPLIT,$-8
	MOVW 0(FP), R0
	SWI $0xa00006
	RET

TEXT runtime·read(SB),NOSPLIT,$-8
	MOVW 0(FP), R0
	MOVW 4(FP), R1
	MOVW 8(FP), R2
	SWI $0xa00003
	RET

TEXT runtime·write(SB),NOSPLIT,$-4
	MOVW	0(FP), R0	// arg 1 - fd
	MOVW	4(FP), R1	// arg 2 - buf
	MOVW	8(FP), R2	// arg 3 - nbyte
	SWI $0xa00004	// sys_write
	RET

// int32 lwp_create(void *context, uintptr flags, void *lwpid)
TEXT runtime·lwp_create(SB),NOSPLIT,$0
	MOVW context+0(FP), R0
	MOVW flags+4(FP), R1
	MOVW lwpid+8(FP), R2
	SWI $0xa00135	// sys__lwp_create
	RET

TEXT runtime·osyield(SB),NOSPLIT,$0
	SWI $0xa0015e	// sys_sched_yield
	RET

TEXT runtime·lwp_park(SB),NOSPLIT,$0
	MOVW 0(FP), R0	// arg 1 - abstime
	MOVW 4(FP), R1	// arg 2 - unpark
	MOVW 8(FP), R2	// arg 3 - hint
	MOVW 12(FP), R3	// arg 4 - unparkhint
	SWI $0xa001b2	// sys__lwp_park
	RET

TEXT runtime·lwp_unpark(SB),NOSPLIT,$0
	MOVW	0(FP), R0	// arg 1 - lwp
	MOVW	4(FP), R1	// arg 2 - hint
	SWI $0xa00141 // sys__lwp_unpark
	RET

TEXT runtime·lwp_self(SB),NOSPLIT,$0
	SWI $0xa00137	// sys__lwp_self
	RET

TEXT runtime·lwp_tramp(SB),NOSPLIT,$0
	MOVW R0, m
	MOVW R1, g

	BL runtime·emptyfunc(SB) // fault if stack check is wrong
	BL (R2)
	MOVW $2, R8  // crash (not reached)
	MOVW R8, (R8)
	RET

TEXT runtime·usleep(SB),NOSPLIT,$16
	MOVW usec+0(FP), R0
	MOVW R0, R2
	MOVW $1000000, R1
	DIV R1, R0
	// 0(R13) is the saved LR, don't use it
	MOVW R0, 4(R13) // tv_sec.low
	MOVW $0, R0
	MOVW R0, 8(R13) // tv_sec.high
	MOD R1, R2
	MOVW $1000, R1
	MUL R1, R2
	MOVW R2, 12(R13) // tv_nsec

	MOVW $4(R13), R0 // arg 1 - rqtp
	MOVW $0, R1      // arg 2 - rmtp
	SWI $0xa001ae	// sys_nanosleep
	RET

TEXT runtime·raise(SB),NOSPLIT,$16
	SWI $0xa00137	// sys__lwp_self, the returned R0 is arg 1
	MOVW	sig+0(FP), R1	// arg 2 - signal
	SWI $0xa0013e	// sys__lwp_kill
	RET

TEXT runtime·setitimer(SB),NOSPLIT,$-4
	MOVW 0(FP), R0	// arg 1 - which
	MOVW 4(FP), R1	// arg 2 - itv
	MOVW 8(FP), R2	// arg 3 - oitv
	SWI $0xa001a9	// sys_setitimer
	RET

// func now() (sec int64, nsec int32)
TEXT time·now(SB), NOSPLIT, $32
	MOVW $0, R0	// CLOCK_REALTIME
	MOVW $8(R13), R1
	SWI $0xa001ab	// clock_gettime

	MOVW 8(R13), R0	// sec.low
	MOVW 12(R13), R1 // sec.high
	MOVW 16(R13), R2 // nsec

	MOVW R0, 0(FP)
	MOVW R1, 4(FP)
	MOVW R2, 8(FP)
	RET

// int64 nanotime(void) so really
// void nanotime(int64 *nsec)
TEXT runtime·nanotime(SB), NOSPLIT, $32
	MOVW $0, R0 // CLOCK_REALTIME
	MOVW $8(R13), R1
	SWI $0xa001ab	// clock_gettime

	MOVW 8(R13), R0 // sec.low
	MOVW 12(R13), R4 // sec.high
	MOVW 16(R13), R2 // nsec

	MOVW $1000000000, R3
	MULLU R0, R3, (R1, R0)
	MUL R3, R4
	ADD.S R2, R0
	ADC R4, R1

	MOVW 0(FP), R3
	MOVW R0, 0(R3)
	MOVW R1, 4(R3)
	RET

TEXT runtime·getcontext(SB),NOSPLIT,$-4
	MOVW 0(FP), R0	// arg 1 - context
	SWI $0xa00133	// sys_getcontext
	MOVW.CS $0, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·sigprocmask(SB),NOSPLIT,$0
	MOVW 0(FP), R0	// arg 1 - how
	MOVW 4(FP), R1	// arg 2 - set
	MOVW 8(FP), R2	// arg 3 - oset
	SWI $0xa00125	// sys_sigprocmask
	MOVW.CS $0, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·sigreturn_tramp(SB),NOSPLIT,$-4
	// on entry, SP points to siginfo, we add sizeof(ucontext)
	// to SP to get a pointer to ucontext.
	ADD $0x80, R13, R0 // 0x80 == sizeof(UcontextT)
	SWI $0xa00134	// sys_setcontext
	// something failed, we have to exit
	MOVW $0x4242, R0 // magic return number
	SWI $0xa00001	// sys_exit
	B -2(PC)	// continue exit

TEXT runtime·sigaction(SB),NOSPLIT,$4
	MOVW 0(FP), R0	// arg 1 - signum
	MOVW 4(FP), R1	// arg 2 - nsa
	MOVW 8(FP), R2	// arg 3 - osa
	MOVW $runtime·sigreturn_tramp(SB), R3	// arg 4 - tramp
	MOVW $2, R4	// arg 5 - vers
	MOVW R4, 4(R13)
	ADD $4, R13	// pass arg 5 on stack
	SWI $0xa00154	// sys___sigaction_sigtramp
	SUB $4, R13
	MOVW.CS $3, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·sigtramp(SB),NOSPLIT,$24
	// this might be called in external code context,
	// where g and m are not set.
	// first save R0, because runtime·load_gm will clobber it
	MOVW	R0, 4(R13) // signum
	MOVB	runtime·iscgo(SB), R0
	CMP 	$0, R0
	BL.NE	runtime·load_gm(SB)

	CMP $0, m
	BNE 4(PC)
	// signal number is already prepared in 4(R13)
	MOVW $runtime·badsignal(SB), R11
	BL (R11)
	RET

	// save g
	MOVW g, R4
	MOVW g, 20(R13)

	// g = m->signal
	MOVW m_gsignal(m), g

	// R0 is already saved
	MOVW R1, 8(R13) // info
	MOVW R2, 12(R13) // context
	MOVW R4, 16(R13) // gp

	BL runtime·sighandler(SB)

	// restore g
	MOVW 20(R13), g
	RET

TEXT runtime·mmap(SB),NOSPLIT,$12
	MOVW 0(FP), R0	// arg 1 - addr
	MOVW 4(FP), R1	// arg 2 - len
	MOVW 8(FP), R2	// arg 3 - prot
	MOVW 12(FP), R3	// arg 4 - flags
	// arg 5 (fid) and arg6 (offset_lo, offset_hi) are passed on stack
	// note the C runtime only passes the 32-bit offset_lo to us
	MOVW 16(FP), R4		// arg 5
	MOVW R4, 4(R13)
	MOVW 20(FP), R5		// arg 6 lower 32-bit
	MOVW R5, 8(R13)
	MOVW $0, R6 // higher 32-bit for arg 6
	MOVW R6, 12(R13)
	ADD $4, R13 // pass arg 5 and arg 6 on stack
	SWI $0xa000c5	// sys_mmap
	SUB $4, R13
	RET

TEXT runtime·munmap(SB),NOSPLIT,$0
	MOVW 0(FP), R0	// arg 1 - addr
	MOVW 4(FP), R1	// arg 2 - len
	SWI $0xa00049	// sys_munmap
	MOVW.CS $0, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·madvise(SB),NOSPLIT,$0
	MOVW 0(FP), R0	// arg 1 - addr
	MOVW 4(FP), R1	// arg 2 - len
	MOVW 8(FP), R2	// arg 3 - behav
	SWI $0xa0004b	// sys_madvise
	// ignore failure - maybe pages are locked
	RET

TEXT runtime·sigaltstack(SB),NOSPLIT,$-4
	MOVW 0(FP), R0	// arg 1 - nss
	MOVW 4(FP), R1	// arg 2 - oss
	SWI $0xa00119	// sys___sigaltstack14
	MOVW.CS $0, R8	// crash on syscall failure
	MOVW.CS R8, (R8)
	RET

TEXT runtime·sysctl(SB),NOSPLIT,$8
	MOVW 0(FP), R0	// arg 1 - name
	MOVW 4(FP), R1	// arg 2 - namelen
	MOVW 8(FP), R2	// arg 3 - oldp
	MOVW 12(FP), R3	// arg 4 - oldlenp
	MOVW 16(FP), R4	// arg 5 - newp
	MOVW R4, 4(R13)
	MOVW 20(FP), R4	// arg 6 - newlen
	MOVW R4, 8(R13)
	ADD $4, R13	// pass arg 5 and 6 on stack
	SWI $0xa000ca	// sys___sysctl
	SUB $4, R13
	RET

// int32 runtime·kqueue(void)
TEXT runtime·kqueue(SB),NOSPLIT,$0
	SWI $0xa00158	// sys_kqueue
	RSB.CS $0, R0
	RET

// int32 runtime·kevent(int kq, Kevent *changelist, int nchanges, Kevent *eventlist, int nevents, Timespec *timeout)
TEXT runtime·kevent(SB),NOSPLIT,$8
	MOVW 0(FP), R0	// kq
	MOVW 4(FP), R1	// changelist
	MOVW 8(FP), R2	// nchanges
	MOVW 12(FP), R3	// eventlist
	MOVW 16(FP), R4	// nevents
	MOVW R4, 4(R13)
	MOVW 20(FP), R4	// timeout
	MOVW R4, 8(R13)
	ADD $4, R13	// pass arg 5 and 6 on stack
	SWI $0xa001b3	// sys___kevent50
	RSB.CS $0, R0
	SUB $4, R13
	RET

// void runtime·closeonexec(int32 fd)
TEXT runtime·closeonexec(SB),NOSPLIT,$0
	MOVW 0(FP), R0	// fd
	MOVW $2, R1	// F_SETFD
	MOVW $1, R2	// FD_CLOEXEC
	SWI $0xa0005c	// sys_fcntl
	RET

TEXT runtime·casp(SB),NOSPLIT,$0
	B	runtime·cas(SB)

// TODO(minux): this is only valid for ARMv6+
// bool armcas(int32 *val, int32 old, int32 new)
// Atomically:
//	if(*val == old){
//		*val = new;
//		return 1;
//	}else
//		return 0;
TEXT runtime·cas(SB),NOSPLIT,$0
	B runtime·armcas(SB)

TEXT runtime·read_tls_fallback(SB),NOSPLIT,$-4
	MOVM.WP [R1, R2, R3, R12], (R13)
	SWI $0x00a0013c // _lwp_getprivate
	MOVM.IAW    (R13), [R1, R2, R3, R12]
	RET
