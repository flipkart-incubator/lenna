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
	MOVL	wincall_n(BX), CX	// words
	MOVL	CX, AX
	SALL	$2, AX
	SUBL	AX, SP			// room for args
	MOVL	SP, DI
	MOVL	wincall_args(BX), SI
	CLD
	REP; MOVSL

	// Call stdcall or cdecl function.
	// DI SI BP BX are preserved, SP is not
	CALL	wincall_fn(BX)
	MOVL	BP, SP

	// Return result.
	MOVL	c+0(FP), BX
	MOVL	AX, wincall_r1(BX)
	MOVL	DX, wincall_r2(BX)

	// GetLastError().
	MOVL	0x34(FS), AX
	MOVL	AX, wincall_err(BX)

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

TEXT runtime·sigtramp(SB),NOSPLIT,$28
	// unwinding?
	MOVL	info+0(FP), CX
	TESTL	$6, 4(CX)		// exception flags
	MOVL	$1, AX
	JNZ	sigdone

	// copy arguments for call to sighandler
	MOVL	CX, 0(SP)
	MOVL	context+8(FP), CX
	MOVL	CX, 4(SP)

	get_tls(CX)

	// check that m exists
	MOVL	m(CX), AX
	CMPL	AX, $0
	JNE	2(PC)
	CALL	runtime·badsignal2(SB)

	MOVL	g(CX), CX
	MOVL	CX, 8(SP)

	MOVL	BX, 12(SP)
	MOVL	BP, 16(SP)
	MOVL	SI, 20(SP)
	MOVL	DI, 24(SP)

	CALL	runtime·sighandler(SB)
	// AX is set to report result back to Windows

	MOVL	24(SP), DI
	MOVL	20(SP), SI
	MOVL	16(SP), BP
	MOVL	12(SP), BX
sigdone:
	RET

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

	// set up SEH frame again
	PUSHL	$runtime·sigtramp(SB)
	PUSHL	0(FS)
	MOVL	SP, 0(FS)

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

	// pop SEH frame
	POPL	0(FS)
	POPL	BX

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

// void install_exception_handler()
TEXT runtime·install_exception_handler(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), CX		// m

	// Set up SEH frame
	MOVL	m_seh(CX), DX
	MOVL	$runtime·sigtramp(SB), AX
	MOVL	AX, seh_handler(DX)
	MOVL	0(FS), AX
	MOVL	AX, seh_prev(DX)

	// Install it
	MOVL	DX, 0(FS)

	RET

// void remove_exception_handler()
TEXT runtime·remove_exception_handler(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), CX		// m

	// Remove SEH frame
	MOVL	m_seh(CX), DX
	MOVL	seh_prev(DX), AX
	MOVL	AX, 0(FS)

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
	MOVL	m_g0(BP), SI
	CMPL	g(CX), SI
	JNE	3(PC)
	// executing on m->g0 already
	CALL	AX
	RET

	// Switch to m->g0 stack and back.
	MOVL	(g_sched+gobuf_sp)(SI), SI
	MOVL	SP, -4(SI)
	LEAL	-4(SI), SP
	CALL	AX
	MOVL	0(SP), SP
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
