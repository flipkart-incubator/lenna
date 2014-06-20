// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "zasm_GOOS_GOARCH.h"
#include "../../cmd/ld/textflag.h"

// void runtime·asmstdcall(void *c);
TEXT runtime·asmstdcall(SB),NOSPLIT,$0
	MOVL	c+0(FP), BX

	// SetLastError(0).
	MOVL	$0, 0x34(FS)

	// Copy args to the stack.
	MOVL	SP, BP
	MOVL	libcall_n(BX), CX	// words
	MOVL	CX, AX
	SALL	$2, AX
	SUBL	AX, SP			// room for args
	MOVL	SP, DI
	MOVL	libcall_args(BX), SI
	CLD
	REP; MOVSL

	// Call stdcall or cdecl function.
	// DI SI BP BX are preserved, SP is not
	CALL	libcall_fn(BX)
	MOVL	BP, SP

	// Return result.
	MOVL	c+0(FP), BX
	MOVL	AX, libcall_r1(BX)
	MOVL	DX, libcall_r2(BX)

	// GetLastError().
	MOVL	0x34(FS), AX
	MOVL	AX, libcall_err(BX)

	RET

TEXT	runtime·badsignal2(SB),NOSPLIT,$24
	// stderr
	MOVL	$-12, 0(SP)
	MOVL	SP, BP
	CALL	*runtime·GetStdHandle(SB)
	MOVL	BP, SP

	MOVL	AX, 0(SP)	// handle
	MOVL	$runtime·badsignalmsg(SB), DX // pointer
	MOVL	DX, 4(SP)
	MOVL	runtime·badsignallen(SB), DX // count
	MOVL	DX, 8(SP)
	LEAL	20(SP), DX  // written count
	MOVL	$0, 0(DX)
	MOVL	DX, 12(SP)
	MOVL	$0, 16(SP) // overlapped
	CALL	*runtime·WriteFile(SB)
	MOVL	BP, SI
	RET

// faster get/set last error
TEXT runtime·getlasterror(SB),NOSPLIT,$0
	MOVL	0x34(FS), AX
	RET

TEXT runtime·setlasterror(SB),NOSPLIT,$0
	MOVL	err+0(FP), AX
	MOVL	AX, 0x34(FS)
	RET

// Called by Windows as a Vectored Exception Handler (VEH).
// First argument is pointer to struct containing
// exception record and context pointers.
// Return 0 for 'not handled', -1 for handled.
TEXT runtime·sigtramp(SB),NOSPLIT,$0-0
	MOVL	ptrs+0(FP), CX
	SUBL	$28, SP

	// save callee-saved registers
	MOVL	BX, 12(SP)
	MOVL	BP, 16(SP)
	MOVL	SI, 20(SP)
	MOVL	DI, 24(SP)

	MOVL	0(CX), BX // ExceptionRecord*
	MOVL	4(CX), CX // Context*

	// fetch g
	get_tls(DX)
	MOVL	m(DX), AX
	CMPL	AX, $0
	JNE	2(PC)
	CALL	runtime·badsignal2(SB)
	MOVL	g(DX), DX
	// call sighandler(ExceptionRecord*, Context*, G*)
	MOVL	BX, 0(SP)
	MOVL	CX, 4(SP)
	MOVL	DX, 8(SP)
	CALL	runtime·sighandler(SB)
	// AX is set to report result back to Windows

	// restore callee-saved registers
	MOVL	24(SP), DI
	MOVL	20(SP), SI
	MOVL	16(SP), BP
	MOVL	12(SP), BX

	ADDL	$28, SP
	// RET 4 (return and pop 4 bytes parameters)
	BYTE $0xC2; WORD $4
	RET // unreached; make assembler happy

TEXT runtime·ctrlhandler(SB),NOSPLIT,$0
	PUSHL	$runtime·ctrlhandler1(SB)
	CALL	runtime·externalthreadhandler(SB)
	MOVL	4(SP), CX
	ADDL	$12, SP
	JMP	CX

TEXT runtime·profileloop(SB),NOSPLIT,$0
	PUSHL	$runtime·profileloop1(SB)
	CALL	runtime·externalthreadhandler(SB)
	MOVL	4(SP), CX
	ADDL	$12, SP
	JMP	CX

TEXT runtime·externalthreadhandler(SB),NOSPLIT,$0
	PUSHL	BP
	MOVL	SP, BP
	PUSHL	BX
	PUSHL	SI
	PUSHL	DI
	PUSHL	0x14(FS)
	MOVL	SP, DX

	// setup dummy m, g
	SUBL	$m_end, SP		// space for M
	MOVL	SP, 0(SP)
	MOVL	$m_end, 4(SP)
	CALL	runtime·memclr(SB)	// smashes AX,BX,CX

	LEAL	m_tls(SP), CX
	MOVL	CX, 0x14(FS)
	MOVL	SP, m(CX)
	MOVL	SP, BX
	SUBL	$g_end, SP		// space for G
	MOVL	SP, g(CX)
	MOVL	SP, m_g0(BX)

	MOVL	SP, 0(SP)
	MOVL	$g_end, 4(SP)
	CALL	runtime·memclr(SB)	// smashes AX,BX,CX
	LEAL	-4096(SP), CX
	MOVL	CX, g_stackguard(SP)
	MOVL	DX, g_stackbase(SP)

	PUSHL	16(BP)			// arg for handler
	CALL	8(BP)
	POPL	CX

	get_tls(CX)
	MOVL	g(CX), CX
	MOVL	g_stackbase(CX), SP
	POPL	0x14(FS)
	POPL	DI
	POPL	SI
	POPL	BX
	POPL	BP
	RET

GLOBL runtime·cbctxts(SB), $4

TEXT runtime·callbackasm1+0(SB),NOSPLIT,$0
  	MOVL	0(SP), AX	// will use to find our callback context

	// remove return address from stack, we are not returning there
	ADDL	$4, SP

	// address to callback parameters into CX
	LEAL	4(SP), CX

	// save registers as required for windows callback
	PUSHL	DI
	PUSHL	SI
	PUSHL	BP
	PUSHL	BX

	// determine index into runtime·cbctxts table
	SUBL	$runtime·callbackasm(SB), AX
	MOVL	$0, DX
	MOVL	$5, BX	// divide by 5 because each call instruction in runtime·callbacks is 5 bytes long
	DIVL	BX,

	// find correspondent runtime·cbctxts table entry
	MOVL	runtime·cbctxts(SB), BX
	MOVL	-4(BX)(AX*4), BX

	// extract callback context
	MOVL	cbctxt_gobody(BX), AX
	MOVL	cbctxt_argsize(BX), DX

	// preserve whatever's at the memory location that
	// the callback will use to store the return value
	PUSHL	0(CX)(DX*1)

	// extend argsize by size of return value
	ADDL	$4, DX

	// remember how to restore stack on return
	MOVL	cbctxt_restorestack(BX), BX
	PUSHL	BX

	// call target Go function
	PUSHL	DX			// argsize (including return value)
	PUSHL	CX			// callback parameters
	PUSHL	AX			// address of target Go function
	CLD
	CALL	runtime·cgocallback_gofunc(SB)
	POPL	AX
	POPL	CX
	POPL	DX

	// how to restore stack on return
	POPL	BX

	// return value into AX (as per Windows spec)
	// and restore previously preserved value
	MOVL	-4(CX)(DX*1), AX
	POPL	-4(CX)(DX*1)

	MOVL	BX, CX			// cannot use BX anymore

	// restore registers as required for windows callback
	POPL	BX
	POPL	BP
	POPL	SI
	POPL	DI

	// remove callback parameters before return (as per Windows spec)
	POPL	DX
	ADDL	CX, SP
	PUSHL	DX

	CLD

	RET

// void tstart(M *newm);
TEXT runtime·tstart(SB),NOSPLIT,$0
	MOVL	newm+4(SP), CX		// m
	MOVL	m_g0(CX), DX		// g

	// Layout new m scheduler stack on os stack.
	MOVL	SP, AX
	MOVL	AX, g_stackbase(DX)
	SUBL	$(64*1024), AX		// stack size
	MOVL	AX, g_stackguard(DX)

	// Set up tls.
	LEAL	m_tls(CX), SI
	MOVL	SI, 0x14(FS)
	MOVL	CX, m(SI)
	MOVL	DX, g(SI)

	// Someday the convention will be D is always cleared.
	CLD

	CALL	runtime·stackcheck(SB)	// clobbers AX,CX
	CALL	runtime·mstart(SB)

	RET

// uint32 tstart_stdcall(M *newm);
TEXT runtime·tstart_stdcall(SB),NOSPLIT,$0
	MOVL	newm+4(SP), BX

	PUSHL	BX
	CALL	runtime·tstart(SB)
	POPL	BX

	// Adjust stack for stdcall to return properly.
	MOVL	(SP), AX		// save return address
	ADDL	$4, SP			// remove single parameter
	MOVL	AX, (SP)		// restore return address

	XORL	AX, AX			// return 0 == success

	RET

// setldt(int entry, int address, int limit)
TEXT runtime·setldt(SB),NOSPLIT,$0
	MOVL	address+4(FP), CX
	MOVL	CX, 0x14(FS)
	RET

// Sleep duration is in 100ns units.
TEXT runtime·usleep1(SB),NOSPLIT,$0
	MOVL	duration+0(FP), BX
	MOVL	$runtime·usleep2(SB), AX // to hide from 8l

	// Execute call on m->g0 stack, in case we are not actually
	// calling a system call wrapper, like when running under WINE.
	get_tls(CX)
	CMPL	CX, $0
	JNE	3(PC)
	// Not a Go-managed thread. Do not switch stack.
	CALL	AX
	RET

	MOVL	m(CX), BP

	// leave pc/sp for cpu profiler
	MOVL	(SP), SI
	MOVL	SI, m_libcallpc(BP)
	MOVL	g(CX), SI
	MOVL	SI, m_libcallg(BP)
	// sp must be the last, because once async cpu profiler finds
	// all three values to be non-zero, it will use them
	LEAL	4(SP), SI
	MOVL	SI, m_libcallsp(BP)

	MOVL	m_g0(BP), SI
	CMPL	g(CX), SI
	JNE	usleep1_switch
	// executing on m->g0 already
	CALL	AX
	JMP	usleep1_ret

usleep1_switch:
	// Switch to m->g0 stack and back.
	MOVL	(g_sched+gobuf_sp)(SI), SI
	MOVL	SP, -4(SI)
	LEAL	-4(SI), SP
	CALL	AX
	MOVL	0(SP), SP

usleep1_ret:
	get_tls(CX)
	MOVL	m(CX), BP
	MOVL	$0, m_libcallsp(BP)
	RET

// Runs on OS stack. duration (in 100ns units) is in BX.
TEXT runtime·usleep2(SB),NOSPLIT,$20
	// Want negative 100ns units.
	NEGL	BX
	MOVL	$-1, hi-4(SP)
	MOVL	BX, lo-8(SP)
	LEAL	lo-8(SP), BX
	MOVL	BX, ptime-12(SP)
	MOVL	$0, alertable-16(SP)
	MOVL	$-1, handle-20(SP)
	MOVL	SP, BP
	MOVL	runtime·NtWaitForSingleObject(SB), AX
	CALL	AX
	MOVL	BP, SP
	RET
