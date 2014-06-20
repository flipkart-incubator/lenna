// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "zasm_GOOS_GOARCH.h"
#include "funcdata.h"
#include "../../cmd/ld/textflag.h"

TEXT _rt0_go(SB),NOSPLIT,$0
	// copy arguments forward on an even stack
	MOVL	argc+0(FP), AX
	MOVL	argv+4(FP), BX
	MOVL	SP, CX
	SUBL	$128, SP		// plenty of scratch
	ANDL	$~15, CX
	MOVL	CX, SP

	MOVL	AX, 16(SP)
	MOVL	BX, 24(SP)
	
	// create istack out of the given (operating system) stack.
	MOVL	$runtime·g0(SB), DI
	LEAL	(-64*1024+104)(SP), DI
	MOVL	BX, g_stackguard(DI)
	MOVL	BX, g_stackguard0(DI)
	MOVL	SP, g_stackbase(DI)

	// find out information about the processor we're on
	MOVQ	$0, AX
	CPUID
	CMPQ	AX, $0
	JE	nocpuinfo
	MOVQ	$1, AX
	CPUID
	MOVL	CX, runtime·cpuid_ecx(SB)
	MOVL	DX, runtime·cpuid_edx(SB)
nocpuinfo:	
	
needtls:
	LEAL	runtime·tls0(SB), DI
	CALL	runtime·settls(SB)

	// store through it, to make sure it works
	get_tls(BX)
	MOVQ	$0x123, g(BX)
	MOVQ	runtime·tls0(SB), AX
	CMPQ	AX, $0x123
	JEQ 2(PC)
	MOVL	AX, 0	// abort
ok:
	// set the per-goroutine and per-mach "registers"
	get_tls(BX)
	LEAL	runtime·g0(SB), CX
	MOVL	CX, g(BX)
	LEAL	runtime·m0(SB), AX
	MOVL	AX, m(BX)

	// save m->g0 = g0
	MOVL	CX, m_g0(AX)

	CLD				// convention is D is always left cleared
	CALL	runtime·check(SB)

	MOVL	16(SP), AX		// copy argc
	MOVL	AX, 0(SP)
	MOVL	24(SP), AX		// copy argv
	MOVL	AX, 4(SP)
	CALL	runtime·args(SB)
	CALL	runtime·osinit(SB)
	CALL	runtime·hashinit(SB)
	CALL	runtime·schedinit(SB)

	// create a new goroutine to start program
	MOVL	$runtime·main·f(SB), AX	// entry
	MOVL	$0, 0(SP)
	MOVL	AX, 4(SP)
	ARGSIZE(8)
	CALL	runtime·newproc(SB)
	ARGSIZE(-1)

	// start this M
	CALL	runtime·mstart(SB)

	MOVL	$0xf1, 0xf1  // crash
	RET

DATA	runtime·main·f+0(SB)/4,$runtime·main(SB)
GLOBL	runtime·main·f(SB),RODATA,$4

TEXT runtime·breakpoint(SB),NOSPLIT,$0-0
	INT $3
	RET

TEXT runtime·asminit(SB),NOSPLIT,$0-0
	// No per-thread init.
	RET

/*
 *  go-routine
 */

// void gosave(Gobuf*)
// save state in Gobuf; setjmp
TEXT runtime·gosave(SB), NOSPLIT, $0-4
	MOVL	b+0(FP), AX	// gobuf
	LEAL	b+0(FP), BX	// caller's SP
	MOVL	BX, gobuf_sp(AX)
	MOVL	0(SP), BX		// caller's PC
	MOVL	BX, gobuf_pc(AX)
	MOVL	$0, gobuf_ctxt(AX)
	MOVQ	$0, gobuf_ret(AX)
	get_tls(CX)
	MOVL	g(CX), BX
	MOVL	BX, gobuf_g(AX)
	RET

// void gogo(Gobuf*)
// restore state from Gobuf; longjmp
TEXT runtime·gogo(SB), NOSPLIT, $0-4
	MOVL	b+0(FP), BX		// gobuf
	MOVL	gobuf_g(BX), DX
	MOVL	0(DX), CX		// make sure g != nil
	get_tls(CX)
	MOVL	DX, g(CX)
	MOVL	gobuf_sp(BX), SP	// restore SP
	MOVL	gobuf_ctxt(BX), DX
	MOVQ	gobuf_ret(BX), AX
	MOVL	$0, gobuf_sp(BX)	// clear to help garbage collector
	MOVQ	$0, gobuf_ret(BX)
	MOVL	$0, gobuf_ctxt(BX)
	MOVL	gobuf_pc(BX), BX
	JMP	BX

// void mcall(void (*fn)(G*))
// Switch to m->g0's stack, call fn(g).
// Fn must never return.  It should gogo(&g->sched)
// to keep running g.
TEXT runtime·mcall(SB), NOSPLIT, $0-4
	MOVL	fn+0(FP), DI
	
	get_tls(CX)
	MOVL	g(CX), AX	// save state in g->sched
	MOVL	0(SP), BX	// caller's PC
	MOVL	BX, (g_sched+gobuf_pc)(AX)
	LEAL	fn+0(FP), BX	// caller's SP
	MOVL	BX, (g_sched+gobuf_sp)(AX)
	MOVL	AX, (g_sched+gobuf_g)(AX)

	// switch to m->g0 & its stack, call fn
	MOVL	m(CX), BX
	MOVL	m_g0(BX), SI
	CMPL	SI, AX	// if g == m->g0 call badmcall
	JNE	3(PC)
	MOVL	$runtime·badmcall(SB), AX
	JMP	AX
	MOVL	SI, g(CX)	// g = m->g0
	MOVL	(g_sched+gobuf_sp)(SI), SP	// sp = m->g0->sched.sp
	PUSHQ	AX
	ARGSIZE(8)
	CALL	DI
	POPQ	AX
	MOVL	$runtime·badmcall2(SB), AX
	JMP	AX
	RET

/*
 * support for morestack
 */

// Called during function prolog when more stack is needed.
// Caller has already done get_tls(CX); MOVQ m(CX), BX.
//
// The traceback routines see morestack on a g0 as being
// the top of a stack (for example, morestack calling newstack
// calling the scheduler calling newm calling gc), so we must
// record an argument size. For that purpose, it has no arguments.
TEXT runtime·morestack(SB),NOSPLIT,$0-0
	// Cannot grow scheduler stack (m->g0).
	MOVL	m_g0(BX), SI
	CMPL	g(CX), SI
	JNE	2(PC)
	MOVL	0, AX

	// Called from f.
	// Set m->morebuf to f's caller.
	MOVL	8(SP), AX	// f's caller's PC
	MOVL	AX, (m_morebuf+gobuf_pc)(BX)
	LEAL	16(SP), AX	// f's caller's SP
	MOVL	AX, (m_morebuf+gobuf_sp)(BX)
	MOVL	AX, m_moreargp(BX)
	get_tls(CX)
	MOVL	g(CX), SI
	MOVL	SI, (m_morebuf+gobuf_g)(BX)

	// Set g->sched to context in f.
	MOVL	0(SP), AX // f's PC
	MOVL	AX, (g_sched+gobuf_pc)(SI)
	MOVL	SI, (g_sched+gobuf_g)(SI)
	LEAL	8(SP), AX // f's SP
	MOVL	AX, (g_sched+gobuf_sp)(SI)
	MOVL	DX, (g_sched+gobuf_ctxt)(SI)

	// Call newstack on m->g0's stack.
	MOVL	m_g0(BX), BX
	MOVL	BX, g(CX)
	MOVL	(g_sched+gobuf_sp)(BX), SP
	CALL	runtime·newstack(SB)
	MOVL	$0, 0x1003	// crash if newstack returns
	RET

// Called from panic.  Mimics morestack,
// reuses stack growth code to create a frame
// with the desired args running the desired function.
//
// func call(fn *byte, arg *byte, argsize uint32).
TEXT runtime·newstackcall(SB), NOSPLIT, $0-20
	get_tls(CX)
	MOVL	m(CX), BX

	// Save our caller's state as the PC and SP to
	// restore when returning from f.
	MOVL	0(SP), AX	// our caller's PC
	MOVL	AX, (m_morebuf+gobuf_pc)(BX)
	LEAL	8(SP), AX	// our caller's SP
	MOVL	AX, (m_morebuf+gobuf_sp)(BX)
	MOVL	g(CX), AX
	MOVL	AX, (m_morebuf+gobuf_g)(BX)
	
	// Save our own state as the PC and SP to restore
	// if this goroutine needs to be restarted.
	MOVL	$runtime·newstackcall(SB), DI
	MOVL	DI, (g_sched+gobuf_pc)(AX)
	MOVL	SP, (g_sched+gobuf_sp)(AX)

	// Set up morestack arguments to call f on a new stack.
	// We set f's frame size to 1, as a hint to newstack
	// that this is a call from runtime·newstackcall.
	// If it turns out that f needs a larger frame than
	// the default stack, f's usual stack growth prolog will
	// allocate a new segment (and recopy the arguments).
	MOVL	8(SP), AX	// fn
	MOVL	12(SP), DX	// arg frame
	MOVL	16(SP), CX	// arg size

	MOVQ	AX, m_cret(BX)	// f's PC
	MOVL	DX, m_moreargp(BX)	// argument frame pointer
	MOVL	CX, m_moreargsize(BX)	// f's argument size
	MOVL	$1, m_moreframesize(BX)	// f's frame size

	// Call newstack on m->g0's stack.
	MOVL	m_g0(BX), BX
	get_tls(CX)
	MOVL	BX, g(CX)
	MOVL	(g_sched+gobuf_sp)(BX), SP
	CALL	runtime·newstack(SB)
	MOVL	$0, 0x1103	// crash if newstack returns
	RET

// reflect·call: call a function with the given argument list
// func call(f *FuncVal, arg *byte, argsize uint32).
// we don't have variable-sized frames, so we use a small number
// of constant-sized-frame functions to encode a few bits of size in the pc.
// Caution: ugly multiline assembly macros in your future!

#define DISPATCH(NAME,MAXSIZE)		\
	CMPL	CX, $MAXSIZE;		\
	JA	3(PC);			\
	MOVL	$runtime·NAME(SB), AX;	\
	JMP	AX
// Note: can't just "JMP runtime·NAME(SB)" - bad inlining results.

TEXT reflect·call(SB), NOSPLIT, $0-20
	MOVLQZX argsize+8(FP), CX
	DISPATCH(call16, 16)
	DISPATCH(call32, 32)
	DISPATCH(call64, 64)
	DISPATCH(call128, 128)
	DISPATCH(call256, 256)
	DISPATCH(call512, 512)
	DISPATCH(call1024, 1024)
	DISPATCH(call2048, 2048)
	DISPATCH(call4096, 4096)
	DISPATCH(call8192, 8192)
	DISPATCH(call16384, 16384)
	DISPATCH(call32768, 32768)
	DISPATCH(call65536, 65536)
	DISPATCH(call131072, 131072)
	DISPATCH(call262144, 262144)
	DISPATCH(call524288, 524288)
	DISPATCH(call1048576, 1048576)
	DISPATCH(call2097152, 2097152)
	DISPATCH(call4194304, 4194304)
	DISPATCH(call8388608, 8388608)
	DISPATCH(call16777216, 16777216)
	DISPATCH(call33554432, 33554432)
	DISPATCH(call67108864, 67108864)
	DISPATCH(call134217728, 134217728)
	DISPATCH(call268435456, 268435456)
	DISPATCH(call536870912, 536870912)
	DISPATCH(call1073741824, 1073741824)
	MOVL	$runtime·badreflectcall(SB), AX
	JMP	AX

#define CALLFN(NAME,MAXSIZE)			\
TEXT runtime·NAME(SB), WRAPPER, $MAXSIZE-16;		\
	/* copy arguments to stack */		\
	MOVL	argptr+4(FP), SI;		\
	MOVL	argsize+8(FP), CX;		\
	MOVL	SP, DI;				\
	REP;MOVSB;				\
	/* call function */			\
	MOVL	f+0(FP), DX;			\
	MOVL	(DX), AX;				\
	CALL	AX; \
	/* copy return values back */		\
	MOVL	argptr+4(FP), DI;		\
	MOVL	argsize+8(FP), CX;		\
	MOVL	retoffset+12(FP), BX;		\
	MOVL	SP, SI;				\
	ADDL	BX, DI;				\
	ADDL	BX, SI;				\
	SUBL	BX, CX;				\
	REP;MOVSB;				\
	RET

CALLFN(call16, 16)
CALLFN(call32, 32)
CALLFN(call64, 64)
CALLFN(call128, 128)
CALLFN(call256, 256)
CALLFN(call512, 512)
CALLFN(call1024, 1024)
CALLFN(call2048, 2048)
CALLFN(call4096, 4096)
CALLFN(call8192, 8192)
CALLFN(call16384, 16384)
CALLFN(call32768, 32768)
CALLFN(call65536, 65536)
CALLFN(call131072, 131072)
CALLFN(call262144, 262144)
CALLFN(call524288, 524288)
CALLFN(call1048576, 1048576)
CALLFN(call2097152, 2097152)
CALLFN(call4194304, 4194304)
CALLFN(call8388608, 8388608)
CALLFN(call16777216, 16777216)
CALLFN(call33554432, 33554432)
CALLFN(call67108864, 67108864)
CALLFN(call134217728, 134217728)
CALLFN(call268435456, 268435456)
CALLFN(call536870912, 536870912)
CALLFN(call1073741824, 1073741824)

// Return point when leaving stack.
//
// Lessstack can appear in stack traces for the same reason
// as morestack; in that context, it has 0 arguments.
TEXT runtime·lessstack(SB), NOSPLIT, $0-0
	// Save return value in m->cret
	get_tls(CX)
	MOVL	m(CX), BX
	MOVQ	AX, m_cret(BX)	// MOVQ, to save all 64 bits

	// Call oldstack on m->g0's stack.
	MOVL	m_g0(BX), BX
	MOVL	BX, g(CX)
	MOVL	(g_sched+gobuf_sp)(BX), SP
	CALL	runtime·oldstack(SB)
	MOVL	$0, 0x1004	// crash if oldstack returns
	RET

// morestack trampolines
TEXT runtime·morestack00(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), BX
	MOVQ	$0, AX
	MOVQ	AX, m_moreframesize(BX)
	MOVL	$runtime·morestack(SB), AX
	JMP	AX

TEXT runtime·morestack01(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), BX
	SHLQ	$32, AX
	MOVQ	AX, m_moreframesize(BX)
	MOVL	$runtime·morestack(SB), AX
	JMP	AX

TEXT runtime·morestack10(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), BX
	MOVLQZX	AX, AX
	MOVQ	AX, m_moreframesize(BX)
	MOVL	$runtime·morestack(SB), AX
	JMP	AX

TEXT runtime·morestack11(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), BX
	MOVQ	AX, m_moreframesize(BX)
	MOVL	$runtime·morestack(SB), AX
	JMP	AX

// subcases of morestack01
// with const of 8,16,...48
TEXT runtime·morestack8(SB),NOSPLIT,$0
	MOVQ	$1, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT runtime·morestack16(SB),NOSPLIT,$0
	MOVQ	$2, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT runtime·morestack24(SB),NOSPLIT,$0
	MOVQ	$3, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT runtime·morestack32(SB),NOSPLIT,$0
	MOVQ	$4, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT runtime·morestack40(SB),NOSPLIT,$0
	MOVQ	$5, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT runtime·morestack48(SB),NOSPLIT,$0
	MOVQ	$6, R8
	MOVL	$morestack<>(SB), AX
	JMP	AX

TEXT morestack<>(SB),NOSPLIT,$0
	get_tls(CX)
	MOVL	m(CX), BX
	SHLQ	$35, R8
	MOVQ	R8, m_moreframesize(BX)
	MOVL	$runtime·morestack(SB), AX
	JMP	AX

TEXT runtime·morestack00_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack00(SB)

TEXT runtime·morestack01_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack01(SB)

TEXT runtime·morestack10_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack10(SB)

TEXT runtime·morestack11_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack11(SB)

TEXT runtime·morestack8_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack8(SB)

TEXT runtime·morestack16_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack16(SB)

TEXT runtime·morestack24_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack24(SB)

TEXT runtime·morestack32_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack32(SB)

TEXT runtime·morestack40_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack40(SB)

TEXT runtime·morestack48_noctxt(SB),NOSPLIT,$0
	MOVL	$0, DX
	JMP	runtime·morestack48(SB)

// bool cas(int32 *val, int32 old, int32 new)
// Atomically:
//	if(*val == old){
//		*val = new;
//		return 1;
//	} else
//		return 0;
TEXT runtime·cas(SB), NOSPLIT, $0-12
	MOVL	val+0(FP), BX
	MOVL	old+4(FP), AX
	MOVL	new+8(FP), CX
	LOCK
	CMPXCHGL	CX, 0(BX)
	JZ 3(PC)
	MOVL	$0, AX
	RET
	MOVL	$1, AX
	RET

// bool	runtime·cas64(uint64 *val, uint64 old, uint64 new)
// Atomically:
//	if(*val == *old){
//		*val = new;
//		return 1;
//	} else {
//		return 0;
//	}
TEXT runtime·cas64(SB), NOSPLIT, $0-24
	MOVL	val+0(FP), BX
	MOVQ	old+8(FP), AX
	MOVQ	new+16(FP), CX
	LOCK
	CMPXCHGQ	CX, 0(BX)
	JNZ	cas64_fail
	MOVL	$1, AX
	RET
cas64_fail:
	MOVL	$0, AX
	RET

// bool casp(void **val, void *old, void *new)
// Atomically:
//	if(*val == old){
//		*val = new;
//		return 1;
//	} else
//		return 0;
TEXT runtime·casp(SB), NOSPLIT, $0-12
	MOVL	val+0(FP), BX
	MOVL	old+4(FP), AX
	MOVL	new+8(FP), CX
	LOCK
	CMPXCHGL	CX, 0(BX)
	JZ 3(PC)
	MOVL	$0, AX
	RET
	MOVL	$1, AX
	RET

// uint32 xadd(uint32 volatile *val, int32 delta)
// Atomically:
//	*val += delta;
//	return *val;
TEXT runtime·xadd(SB), NOSPLIT, $0-8
	MOVL	val+0(FP), BX
	MOVL	delta+4(FP), AX
	MOVL	AX, CX
	LOCK
	XADDL	AX, 0(BX)
	ADDL	CX, AX
	RET

TEXT runtime·xadd64(SB), NOSPLIT, $0-16
	MOVL	val+0(FP), BX
	MOVQ	delta+8(FP), AX
	MOVQ	AX, CX
	LOCK
	XADDQ	AX, 0(BX)
	ADDQ	CX, AX
	RET

TEXT runtime·xchg(SB), NOSPLIT, $0-8
	MOVL	val+0(FP), BX
	MOVL	new+4(FP), AX
	XCHGL	AX, 0(BX)
	RET

TEXT runtime·xchg64(SB), NOSPLIT, $0-16
	MOVL	val+0(FP), BX
	MOVQ	new+8(FP), AX
	XCHGQ	AX, 0(BX)
	RET

TEXT runtime·procyield(SB),NOSPLIT,$0-0
	MOVL	val+0(FP), AX
again:
	PAUSE
	SUBL	$1, AX
	JNZ	again
	RET

TEXT runtime·atomicstorep(SB), NOSPLIT, $0-8
	MOVL	ptr+0(FP), BX
	MOVL	val+4(FP), AX
	XCHGL	AX, 0(BX)
	RET

TEXT runtime·atomicstore(SB), NOSPLIT, $0-8
	MOVL	ptr+0(FP), BX
	MOVL	val+4(FP), AX
	XCHGL	AX, 0(BX)
	RET

TEXT runtime·atomicstore64(SB), NOSPLIT, $0-16
	MOVL	ptr+0(FP), BX
	MOVQ	val+8(FP), AX
	XCHGQ	AX, 0(BX)
	RET

// void jmpdefer(fn, sp);
// called from deferreturn.
// 1. pop the caller
// 2. sub 5 bytes from the callers return
// 3. jmp to the argument
TEXT runtime·jmpdefer(SB), NOSPLIT, $0-16
	MOVL	fn+0(FP), DX
	MOVL	callersp+4(FP), BX
	LEAL	-8(BX), SP	// caller sp after CALL
	SUBL	$5, (SP)	// return to CALL again
	MOVL	0(DX), BX
	JMP	BX	// but first run the deferred function

// asmcgocall(void(*fn)(void*), void *arg)
// Not implemented.
TEXT runtime·asmcgocall(SB),NOSPLIT,$0-8
	MOVL	0, AX
	RET

// cgocallback(void (*fn)(void*), void *frame, uintptr framesize)
// Not implemented.
TEXT runtime·cgocallback(SB),NOSPLIT,$0-12
	MOVL	0, AX
	RET

// void setmg(M*, G*); set m and g. for use by needm.
// Not implemented.
TEXT runtime·setmg(SB), NOSPLIT, $0-8
	MOVL	0, AX
	RET

// check that SP is in range [g->stackbase, g->stackguard)
TEXT runtime·stackcheck(SB), NOSPLIT, $0-0
	get_tls(CX)
	MOVL	g(CX), AX
	CMPL	g_stackbase(AX), SP
	JHI	2(PC)
	MOVL	0, AX
	CMPL	SP, g_stackguard(AX)
	JHI	2(PC)
	MOVL	0, AX
	RET

TEXT runtime·memclr(SB),NOSPLIT,$0-8
	MOVL	addr+0(FP), DI
	MOVL	count+4(FP), CX
	MOVQ	CX, BX
	ANDQ	$7, BX
	SHRQ	$3, CX
	MOVQ	$0, AX
	CLD
	REP
	STOSQ
	MOVQ	BX, CX
	REP
	STOSB
	RET

TEXT runtime·getcallerpc(SB),NOSPLIT,$0-8
	MOVL	x+0(FP),AX		// addr of first arg
	MOVL	-8(AX),AX		// get calling pc
	RET

TEXT runtime·setcallerpc(SB),NOSPLIT,$0-16
	MOVL	x+0(FP),AX		// addr of first arg
	MOVL	pc+4(FP), BX		// pc to set
	MOVQ	BX, -8(AX)		// set calling pc
	RET

TEXT runtime·getcallersp(SB),NOSPLIT,$0-8
	MOVL	sp+0(FP), AX
	RET

// int64 runtime·cputicks(void)
TEXT runtime·cputicks(SB),NOSPLIT,$0-0
	RDTSC
	SHLQ	$32, DX
	ADDQ	DX, AX
	RET

TEXT runtime·stackguard(SB),NOSPLIT,$0-16
	MOVL	SP, DX
	MOVL	DX, sp+0(FP)
	get_tls(CX)
	MOVL	g(CX), BX
	MOVL	g_stackguard(BX), DX
	MOVL	DX, limit+4(FP)
	RET

GLOBL runtime·tls0(SB), $64

// hash function using AES hardware instructions
// For now, our one amd64p32 system (NaCl) does not
// support using AES instructions, so have not bothered to
// write the implementations. Can copy and adjust the ones
// in asm_amd64.s when the time comes.

TEXT runtime·aeshash(SB),NOSPLIT,$0-24
	RET

TEXT runtime·aeshashstr(SB),NOSPLIT,$0-24
	RET

TEXT runtime·aeshash32(SB),NOSPLIT,$0-24
	RET

TEXT runtime·aeshash64(SB),NOSPLIT,$0-24
	RET

TEXT runtime·memeq(SB),NOSPLIT,$0-12
	MOVL	a+0(FP), SI
	MOVL	b+4(FP), DI
	MOVL	count+8(FP), BX
	JMP	runtime·memeqbody(SB)

// a in SI
// b in DI
// count in BX
TEXT runtime·memeqbody(SB),NOSPLIT,$0-0
	XORQ	AX, AX

	CMPQ	BX, $8
	JB	small
	
	// 64 bytes at a time using xmm registers
hugeloop:
	CMPQ	BX, $64
	JB	bigloop
	MOVOU	(SI), X0
	MOVOU	(DI), X1
	MOVOU	16(SI), X2
	MOVOU	16(DI), X3
	MOVOU	32(SI), X4
	MOVOU	32(DI), X5
	MOVOU	48(SI), X6
	MOVOU	48(DI), X7
	PCMPEQB	X1, X0
	PCMPEQB	X3, X2
	PCMPEQB	X5, X4
	PCMPEQB	X7, X6
	PAND	X2, X0
	PAND	X6, X4
	PAND	X4, X0
	PMOVMSKB X0, DX
	ADDQ	$64, SI
	ADDQ	$64, DI
	SUBQ	$64, BX
	CMPL	DX, $0xffff
	JEQ	hugeloop
	RET

	// 8 bytes at a time using 64-bit register
bigloop:
	CMPQ	BX, $8
	JBE	leftover
	MOVQ	(SI), CX
	MOVQ	(DI), DX
	ADDQ	$8, SI
	ADDQ	$8, DI
	SUBQ	$8, BX
	CMPQ	CX, DX
	JEQ	bigloop
	RET

	// remaining 0-8 bytes
leftover:
	ADDQ	BX, SI
	ADDQ	BX, DI
	MOVQ	-8(SI), CX
	MOVQ	-8(DI), DX
	CMPQ	CX, DX
	SETEQ	AX
	RET

small:
	CMPQ	BX, $0
	JEQ	equal

	LEAQ	0(BX*8), CX
	NEGQ	CX

	CMPB	SI, $0xf8
	JA	si_high

	// load at SI won't cross a page boundary.
	MOVQ	(SI), SI
	JMP	si_finish
si_high:
	// address ends in 11111xxx.  Load up to bytes we want, move to correct position.
	MOVQ	BX, DX
	ADDQ	SI, DX
	MOVQ	-8(DX), SI
	SHRQ	CX, SI
si_finish:

	// same for DI.
	CMPB	DI, $0xf8
	JA	di_high
	MOVQ	(DI), DI
	JMP	di_finish
di_high:
	MOVQ	BX, DX
	ADDQ	DI, DX
	MOVQ	-8(DX), DI
	SHRQ	CX, DI
di_finish:

	SUBQ	SI, DI
	SHLQ	CX, DI
equal:
	SETEQ	AX
	RET

TEXT runtime·cmpstring(SB),NOSPLIT,$0-20
	MOVL	s1+0(FP), SI
	MOVL	s1+4(FP), BX
	MOVL	s2+8(FP), DI
	MOVL	s2+12(FP), DX
	CALL	runtime·cmpbody(SB)
	MOVL	AX, res+16(FP)
	RET

TEXT bytes·Compare(SB),NOSPLIT,$0-28
	MOVL	s1+0(FP), SI
	MOVL	s1+4(FP), BX
	MOVL	s2+12(FP), DI
	MOVL	s2+16(FP), DX
	CALL	runtime·cmpbody(SB)
	MOVQ	AX, res+24(FP)
	RET

// input:
//   SI = a
//   DI = b
//   BX = alen
//   DX = blen
// output:
//   AX = 1/0/-1
TEXT runtime·cmpbody(SB),NOSPLIT,$0-0
	CMPQ	SI, DI
	JEQ	cmp_allsame
	CMPQ	BX, DX
	MOVQ	DX, R8
	CMOVQLT	BX, R8 // R8 = min(alen, blen) = # of bytes to compare
	CMPQ	R8, $8
	JB	cmp_small

cmp_loop:
	CMPQ	R8, $16
	JBE	cmp_0through16
	MOVOU	(SI), X0
	MOVOU	(DI), X1
	PCMPEQB X0, X1
	PMOVMSKB X1, AX
	XORQ	$0xffff, AX	// convert EQ to NE
	JNE	cmp_diff16	// branch if at least one byte is not equal
	ADDQ	$16, SI
	ADDQ	$16, DI
	SUBQ	$16, R8
	JMP	cmp_loop
	
	// AX = bit mask of differences
cmp_diff16:
	BSFQ	AX, BX	// index of first byte that differs
	XORQ	AX, AX
	ADDQ	BX, SI
	MOVB	(SI), CX
	ADDQ	BX, DI
	CMPB	CX, (DI)
	SETHI	AX
	LEAQ	-1(AX*2), AX	// convert 1/0 to +1/-1
	RET

	// 0 through 16 bytes left, alen>=8, blen>=8
cmp_0through16:
	CMPQ	R8, $8
	JBE	cmp_0through8
	MOVQ	(SI), AX
	MOVQ	(DI), CX
	CMPQ	AX, CX
	JNE	cmp_diff8
cmp_0through8:
	ADDQ	R8, SI
	ADDQ	R8, DI
	MOVQ	-8(SI), AX
	MOVQ	-8(DI), CX
	CMPQ	AX, CX
	JEQ	cmp_allsame

	// AX and CX contain parts of a and b that differ.
cmp_diff8:
	BSWAPQ	AX	// reverse order of bytes
	BSWAPQ	CX
	XORQ	AX, CX
	BSRQ	CX, CX	// index of highest bit difference
	SHRQ	CX, AX	// move a's bit to bottom
	ANDQ	$1, AX	// mask bit
	LEAQ	-1(AX*2), AX // 1/0 => +1/-1
	RET

	// 0-7 bytes in common
cmp_small:
	LEAQ	(R8*8), CX	// bytes left -> bits left
	NEGQ	CX		//  - bits lift (== 64 - bits left mod 64)
	JEQ	cmp_allsame

	// load bytes of a into high bytes of AX
	CMPB	SI, $0xf8
	JA	cmp_si_high
	MOVQ	(SI), SI
	JMP	cmp_si_finish
cmp_si_high:
	ADDQ	R8, SI
	MOVQ	-8(SI), SI
	SHRQ	CX, SI
cmp_si_finish:
	SHLQ	CX, SI

	// load bytes of b in to high bytes of BX
	CMPB	DI, $0xf8
	JA	cmp_di_high
	MOVQ	(DI), DI
	JMP	cmp_di_finish
cmp_di_high:
	ADDQ	R8, DI
	MOVQ	-8(DI), DI
	SHRQ	CX, DI
cmp_di_finish:
	SHLQ	CX, DI

	BSWAPQ	SI	// reverse order of bytes
	BSWAPQ	DI
	XORQ	SI, DI	// find bit differences
	JEQ	cmp_allsame
	BSRQ	DI, CX	// index of highest bit difference
	SHRQ	CX, SI	// move a's bit to bottom
	ANDQ	$1, SI	// mask bit
	LEAQ	-1(SI*2), AX // 1/0 => +1/-1
	RET

cmp_allsame:
	XORQ	AX, AX
	XORQ	CX, CX
	CMPQ	BX, DX
	SETGT	AX	// 1 if alen > blen
	SETEQ	CX	// 1 if alen == blen
	LEAQ	-1(CX)(AX*2), AX	// 1,0,-1 result
	RET

TEXT bytes·IndexByte(SB),NOSPLIT,$0
	MOVL s+0(FP), SI
	MOVL s_len+4(FP), BX
	MOVB c+12(FP), AL
	CALL runtime·indexbytebody(SB)
	MOVL AX, ret+16(FP)
	RET

TEXT strings·IndexByte(SB),NOSPLIT,$0
	MOVL s+0(FP), SI
	MOVL s_len+4(FP), BX
	MOVB c+8(FP), AL
	CALL runtime·indexbytebody(SB)
	MOVL AX, ret+16(FP)
	RET

// input:
//   SI: data
//   BX: data len
//   AL: byte sought
// output:
//   AX
TEXT runtime·indexbytebody(SB),NOSPLIT,$0
	MOVL SI, DI

	CMPL BX, $16
	JLT indexbyte_small

	// round up to first 16-byte boundary
	TESTL $15, SI
	JZ aligned
	MOVL SI, CX
	ANDL $~15, CX
	ADDL $16, CX

	// search the beginning
	SUBL SI, CX
	REPN; SCASB
	JZ success

// DI is 16-byte aligned; get ready to search using SSE instructions
aligned:
	// round down to last 16-byte boundary
	MOVL BX, R11
	ADDL SI, R11
	ANDL $~15, R11

	// shuffle X0 around so that each byte contains c
	MOVD AX, X0
	PUNPCKLBW X0, X0
	PUNPCKLBW X0, X0
	PSHUFL $0, X0, X0
	JMP condition

sse:
	// move the next 16-byte chunk of the buffer into X1
	MOVO (DI), X1
	// compare bytes in X0 to X1
	PCMPEQB X0, X1
	// take the top bit of each byte in X1 and put the result in DX
	PMOVMSKB X1, DX
	TESTL DX, DX
	JNZ ssesuccess
	ADDL $16, DI

condition:
	CMPL DI, R11
	JLT sse

	// search the end
	MOVL SI, CX
	ADDL BX, CX
	SUBL R11, CX
	// if CX == 0, the zero flag will be set and we'll end up
	// returning a false success
	JZ failure
	REPN; SCASB
	JZ success

failure:
	MOVL $-1, AX
	RET

// handle for lengths < 16
indexbyte_small:
	MOVL BX, CX
	REPN; SCASB
	JZ success
	MOVL $-1, AX
	RET

// we've found the chunk containing the byte
// now just figure out which specific byte it is
ssesuccess:
	// get the index of the least significant set bit
	BSFW DX, DX
	SUBL SI, DI
	ADDL DI, DX
	MOVL DX, AX
	RET

success:
	SUBL SI, DI
	SUBL $1, DI
	MOVL DI, AX
	RET

TEXT bytes·Equal(SB),NOSPLIT,$0-25
	MOVL	a_len+4(FP), BX
	MOVL	b_len+16(FP), CX
	XORL	AX, AX
	CMPL	BX, CX
	JNE	eqret
	MOVL	a+0(FP), SI
	MOVL	b+12(FP), DI
	CALL	runtime·memeqbody(SB)
eqret:
	MOVB	AX, ret+24(FP)
	RET

TEXT runtime·timenow(SB), NOSPLIT, $0-0
	JMP	time·now(SB)
