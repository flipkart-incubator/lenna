// Copyright 2013 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "zasm_GOOS_GOARCH.h"
#include "../../cmd/ld/textflag.h"
#include "syscall_nacl.h"

#define NACL_SYSCALL(code) \
	MOVL $(0x10000 + ((code)<<5)), AX; CALL AX

#define NACL_SYSJMP(code) \
	MOVL $(0x10000 + ((code)<<5)), AX; JMP AX

TEXT runtime·exit(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_exit)

TEXT runtime·exit1(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_thread_exit)

TEXT runtime·open(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_open)

TEXT runtime·close(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_close)

TEXT runtime·read(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_read)

TEXT syscall·naclWrite(SB), NOSPLIT, $12-16
	MOVL arg1+0(FP), DI
	MOVL arg2+4(FP), SI
	MOVL arg3+8(FP), DX
	MOVL DI, 0(SP)
	MOVL SI, 4(SP)
	MOVL DX, 8(SP)
	CALL runtime·write(SB)
	MOVL AX, ret+16(FP)
	RET

TEXT runtime·write(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_write)

TEXT runtime·nacl_exception_stack(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_exception_stack)

TEXT runtime·nacl_exception_handler(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_exception_handler)

TEXT runtime·nacl_sem_create(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_sem_create)

TEXT runtime·nacl_sem_wait(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_sem_wait)

TEXT runtime·nacl_sem_post(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_sem_post)

TEXT runtime·nacl_mutex_create(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_mutex_create)

TEXT runtime·nacl_mutex_lock(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_mutex_lock)

TEXT runtime·nacl_mutex_trylock(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_mutex_trylock)

TEXT runtime·nacl_mutex_unlock(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_mutex_unlock)

TEXT runtime·nacl_cond_create(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_cond_create)

TEXT runtime·nacl_cond_wait(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_cond_wait)

TEXT runtime·nacl_cond_signal(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_cond_signal)

TEXT runtime·nacl_cond_broadcast(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_cond_broadcast)

TEXT runtime·nacl_cond_timed_wait_abs(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_cond_timed_wait_abs)

TEXT runtime·nacl_thread_create(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_thread_create)

TEXT runtime·mstart_nacl(SB),NOSPLIT,$0
	JMP runtime·mstart(SB)

TEXT runtime·nacl_nanosleep(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_nanosleep)

TEXT runtime·osyield(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_sched_yield)

TEXT runtime·mmap(SB),NOSPLIT,$32
	MOVL	arg1+0(FP), AX
	MOVL	AX, 0(SP)
	MOVL	arg2+4(FP), AX
	MOVL	AX, 4(SP)
	MOVL	arg3+8(FP), AX
	MOVL	AX, 8(SP)
	MOVL	arg4+12(FP), AX
	MOVL	AX, 12(SP)
	MOVL	arg5+16(FP), AX
	MOVL	AX, 16(SP)
	MOVL	arg6+20(FP), AX
	MOVL	AX, 24(SP)
	MOVL	$0, 28(SP)
	LEAL	24(SP), AX
	MOVL	AX, 20(SP)
	NACL_SYSCALL(SYS_mmap)
	RET

TEXT time·now(SB),NOSPLIT,$20
	MOVL $0, 0(SP) // real time clock
	LEAL 8(SP), AX
	MOVL AX, 4(SP) // timespec
	NACL_SYSCALL(SYS_clock_gettime)
	MOVL 8(SP), AX // low 32 sec
	MOVL 12(SP), CX // high 32 sec
	MOVL 16(SP), BX // nsec

	// sec is in AX, nsec in BX
	MOVL	AX, sec+0(FP)
	MOVL	CX, sec+4(FP)
	MOVL	BX, nsec+8(FP)
	RET

TEXT syscall·now(SB),NOSPLIT,$0
	JMP time·now(SB)

TEXT runtime·nacl_clock_gettime(SB),NOSPLIT,$0
	NACL_SYSJMP(SYS_clock_gettime)
	
TEXT runtime·nanotime(SB),NOSPLIT,$20
	MOVL $0, 0(SP) // real time clock
	LEAL 8(SP), AX
	MOVL AX, 4(SP) // timespec
	NACL_SYSCALL(SYS_clock_gettime)
	MOVL 8(SP), AX // low 32 sec
	MOVL 16(SP), BX // nsec

	// sec is in AX, nsec in BX
	// convert to DX:AX nsec
	MOVL	$1000000000, CX
	MULL	CX
	ADDL	BX, AX
	ADCL	$0, DX

	MOVL	ret+0(FP), DI
	MOVL	AX, 0(DI)
	MOVL	DX, 4(DI)
	RET

TEXT runtime·setldt(SB),NOSPLIT,$8
	MOVL	addr+4(FP), BX // aka base
	ADDL	$0x8, BX
	MOVL	BX, 0(SP)
	NACL_SYSCALL(SYS_tls_init)
	RET

TEXT runtime·sigtramp(SB),NOSPLIT,$0
	get_tls(CX)

	// check that m exists
	MOVL	m(CX), BX
	CMPL	BX, $0
	JNE	6(PC)
	MOVL	$11, BX
	MOVL	BX, 0(SP)
	MOVL	$runtime·badsignal(SB), AX
	CALL	AX
	JMP 	sigtramp_ret

	// save g
	MOVL	g(CX), DI
	MOVL	DI, 20(SP)
	
	// g = m->gsignal
	MOVL	m_gsignal(BX), BX
	MOVL	BX, g(CX)
	
	// copy arguments for sighandler
	MOVL	$11, 0(SP) // signal
	MOVL	$0, 4(SP) // siginfo
	LEAL	ctxt+4(FP), AX
	MOVL	AX, 8(SP) // context
	MOVL	DI, 12(SP) // g

	CALL	runtime·sighandler(SB)

	// restore g
	get_tls(CX)
	MOVL	20(SP), BX
	MOVL	BX, g(CX)

sigtramp_ret:
	// Enable exceptions again.
	NACL_SYSCALL(SYS_exception_clear_flag)

	// NaCl has abidcated its traditional operating system responsibility
	// and declined to implement 'sigreturn'. Instead the only way to return
	// to the execution of our program is to restore the registers ourselves.
	// Unfortunately, that is impossible to do with strict fidelity, because
	// there is no way to do the final update of PC that ends the sequence
	// without either (1) jumping to a register, in which case the register ends
	// holding the PC value instead of its intended value or (2) storing the PC
	// on the stack and using RET, which imposes the requirement that SP is
	// valid and that is okay to smash the word below it. The second would
	// normally be the lesser of the two evils, except that on NaCl, the linker
	// must rewrite RET into "POP reg; AND $~31, reg; JMP reg", so either way
	// we are going to lose a register as a result of the incoming signal.
	// Similarly, there is no way to restore EFLAGS; the usual way is to use
	// POPFL, but NaCl rejects that instruction. We could inspect the bits and
	// execute a sequence of instructions designed to recreate those flag
	// settings, but that's a lot of work.
	//
	// Thankfully, Go's signal handlers never try to return directly to the
	// executing code, so all the registers and EFLAGS are dead and can be
	// smashed. The only registers that matter are the ones that are setting
	// up for the simulated call that the signal handler has created.
	// Today those registers are just PC and SP, but in case additional registers
	// are relevant in the future (for example DX is the Go func context register)
	// we restore as many registers as possible.
	// 
	// We smash BP, because that's what the linker smashes during RET.
	//
	LEAL	ctxt+4(FP), BP
	ADDL	$64, BP
	MOVL	0(BP), AX
	MOVL	4(BP), CX
	MOVL	8(BP), DX
	MOVL	12(BP), BX
	MOVL	16(BP), SP
	// 20(BP) is saved BP, never to be seen again
	MOVL	24(BP), SI
	MOVL	28(BP), DI
	// 36(BP) is saved EFLAGS, never to be seen again
	MOVL	32(BP), BP // saved PC
	JMP	BP
