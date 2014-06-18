// Inferno's libkern/vlop-arm.s
// http://code.google.com/p/inferno-os/source/browse/libkern/vlop-arm.s
//
//         Copyright © 1994-1999 Lucent Technologies Inc.  All rights reserved.
//         Revisions Copyright © 2000-2007 Vita Nuova Holdings Limited (www.vitanuova.com).  All rights reserved.
//         Portions Copyright 2009 The Go Authors. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "zasm_GOOS_GOARCH.h"
#include "../../cmd/ld/textflag.h"

arg=0

/* replaced use of R10 by R11 because the former can be the data segment base register */

TEXT _mulv(SB), NOSPLIT, $0
	MOVW	0(FP), R0
	MOVW	4(FP), R2	/* l0 */
	MOVW	8(FP), R11	/* h0 */
	MOVW	12(FP), R4	/* l1 */
	MOVW	16(FP), R5	/* h1 */
	MULLU	R4, R2, (R7,R6)
	MUL	R11, R4, R8
	ADD	R8, R7
	MUL	R2, R5, R8
	ADD	R8, R7
	MOVW	R6, 0(R(arg))
	MOVW	R7, 4(R(arg))
	RET

// trampoline for _sfloat2. passes LR as arg0 and
// saves registers R0-R13 and CPSR on the stack. R0-R12 and CPSR flags can
// be changed by _sfloat2.
TEXT _sfloat(SB), NOSPLIT, $64-0 // 4 arg + 14*4 saved regs + cpsr
	MOVW	R14, 4(R13)
	MOVW	R0, 8(R13)
	MOVW	$12(R13), R0
	MOVM.IA.W	[R1-R12], (R0)
	MOVW	$68(R13), R1 // correct for frame size
	MOVW	R1, 60(R13)
	WORD	$0xe10f1000 // mrs r1, cpsr
	MOVW	R1, 64(R13)
	// Disable preemption of this goroutine during _sfloat2 by
	// m->locks++ and m->locks-- around the call.
	// Rescheduling this goroutine may cause the loss of the
	// contents of the software floating point registers in 
	// m->freghi, m->freglo, m->fflag, if the goroutine is moved
	// to a different m or another goroutine runs on this m.
	// Rescheduling at ordinary function calls is okay because
	// all registers are caller save, but _sfloat2 and the things
	// that it runs are simulating the execution of individual
	// program instructions, and those instructions do not expect
	// the floating point registers to be lost.
	// An alternative would be to move the software floating point
	// registers into G, but they do not need to be kept at the 
	// usual places a goroutine reschedules (at function calls),
	// so it would be a waste of 132 bytes per G.
	MOVW	m_locks(m), R1
	ADD	$1, R1
	MOVW	R1, m_locks(m)
	BL	runtime·_sfloat2(SB)
	MOVW	m_locks(m), R1
	SUB	$1, R1
	MOVW	R1, m_locks(m)
	MOVW	R0, 0(R13)
	MOVW	64(R13), R1
	WORD	$0xe128f001	// msr cpsr_f, r1
	MOVW	$12(R13), R0
	// Restore R1-R8 and R11-R12, but ignore the saved R9 (m) and R10 (g).
	// Both are maintained by the runtime and always have correct values,
	// so there is no need to restore old values here.
	// The g should not have changed, but m may have, if we were preempted
	// and restarted on a different thread, in which case restoring the old
	// value is incorrect and will cause serious confusion in the runtime.
	MOVM.IA.W	(R0), [R1-R8]
	MOVW	$52(R13), R0
	MOVM.IA.W	(R0), [R11-R12]
	MOVW	8(R13), R0
	RET

// func udiv(n, d uint32) (q, r uint32)
// Reference: 
// Sloss, Andrew et. al; ARM System Developer's Guide: Designing and Optimizing System Software
// Morgan Kaufmann; 1 edition (April 8, 2004), ISBN 978-1558608740
q = 0 // input d, output q
r = 1 // input n, output r
s = 2 // three temporary variables
M = 3
a = 11
// Be careful: R(a) == R11 will be used by the linker for synthesized instructions.
TEXT udiv<>(SB),NOSPLIT,$-4
	CLZ 	R(q), R(s) // find normalizing shift
	MOVW.S	R(q)<<R(s), R(a)
	MOVW	$fast_udiv_tab<>-64(SB), R(M)
	MOVBU.NE	R(a)>>25(R(M)), R(a) // index by most significant 7 bits of divisor

	SUB.S	$7, R(s)
	RSB 	$0, R(q), R(M) // M = -q
	MOVW.PL	R(a)<<R(s), R(q)

	// 1st Newton iteration
	MUL.PL	R(M), R(q), R(a) // a = -q*d
	BMI 	udiv_by_large_d
	MULAWT	R(a), R(q), R(q), R(q) // q approx q-(q*q*d>>32)
	TEQ 	R(M)->1, R(M) // check for d=0 or d=1

	// 2nd Newton iteration
	MUL.NE	R(M), R(q), R(a)
	MOVW.NE	$0, R(s)
	MULAL.NE R(q), R(a), (R(q),R(s))
	BEQ 	udiv_by_0_or_1

	// q now accurate enough for a remainder r, 0<=r<3*d
	MULLU	R(q), R(r), (R(q),R(s)) // q = (r * q) >> 32	
	ADD 	R(M), R(r), R(r) // r = n - d
	MULA	R(M), R(q), R(r), R(r) // r = n - (q+1)*d

	// since 0 <= n-q*d < 3*d; thus -d <= r < 2*d
	CMN 	R(M), R(r) // t = r-d
	SUB.CS	R(M), R(r), R(r) // if (t<-d || t>=0) r=r+d
	ADD.CC	$1, R(q)
	ADD.PL	R(M)<<1, R(r)
	ADD.PL	$2, R(q)
	RET

udiv_by_large_d:
	// at this point we know d>=2^(31-6)=2^25
	SUB 	$4, R(a), R(a)
	RSB 	$0, R(s), R(s)
	MOVW	R(a)>>R(s), R(q)
	MULLU	R(q), R(r), (R(q),R(s))
	MULA	R(M), R(q), R(r), R(r)

	// q now accurate enough for a remainder r, 0<=r<4*d
	CMN 	R(r)>>1, R(M) // if(r/2 >= d)
	ADD.CS	R(M)<<1, R(r)
	ADD.CS	$2, R(q)
	CMN 	R(r), R(M)
	ADD.CS	R(M), R(r)
	ADD.CS	$1, R(q)
	RET

udiv_by_0_or_1:
	// carry set if d==1, carry clear if d==0
	BCC udiv_by_0
	MOVW	R(r), R(q)
	MOVW	$0, R(r)
	RET

udiv_by_0:
	// The ARM toolchain expects it can emit references to DIV and MOD
	// instructions. The linker rewrites each pseudo-instruction into
	// a sequence that pushes two values onto the stack and then calls
	// _divu, _modu, _div, or _mod (below), all of which have a 16-byte
	// frame plus the saved LR. The traceback routine knows the expanded
	// stack frame size at the pseudo-instruction call site, but it
	// doesn't know that the frame has a non-standard layout. In particular,
	// it expects to find a saved LR in the bottom word of the frame.
	// Unwind the stack back to the pseudo-instruction call site, copy the
	// saved LR where the traceback routine will look for it, and make it
	// appear that panicdivide was called from that PC.
	MOVW	0(R13), LR
	ADD	$20, R13
	MOVW	8(R13), R1 // actual saved LR
	MOVW	R1, 0(R13) // expected here for traceback
	B 	runtime·panicdivide(SB)

TEXT fast_udiv_tab<>(SB),NOSPLIT,$-4
	// var tab [64]byte
	// tab[0] = 255; for i := 1; i <= 63; i++ { tab[i] = (1<<14)/(64+i) }
	// laid out here as little-endian uint32s
	WORD $0xf4f8fcff
	WORD $0xe6eaedf0
	WORD $0xdadde0e3
	WORD $0xcfd2d4d7
	WORD $0xc5c7cacc
	WORD $0xbcbec0c3
	WORD $0xb4b6b8ba
	WORD $0xacaeb0b2
	WORD $0xa5a7a8aa
	WORD $0x9fa0a2a3
	WORD $0x999a9c9d
	WORD $0x93949697
	WORD $0x8e8f9092
	WORD $0x898a8c8d
	WORD $0x85868788
	WORD $0x81828384

// The linker will pass numerator in R(TMP), and it also
// expects the result in R(TMP)
TMP = 11

TEXT _divu(SB), NOSPLIT, $16
	MOVW	R(q), 4(R13)
	MOVW	R(r), 8(R13)
	MOVW	R(s), 12(R13)
	MOVW	R(M), 16(R13)

	MOVW	R(TMP), R(r)		/* numerator */
	MOVW	0(FP), R(q) 		/* denominator */
	BL  	udiv<>(SB)
	MOVW	R(q), R(TMP)
	MOVW	4(R13), R(q)
	MOVW	8(R13), R(r)
	MOVW	12(R13), R(s)
	MOVW	16(R13), R(M)
	RET

TEXT _modu(SB), NOSPLIT, $16
	MOVW	R(q), 4(R13)
	MOVW	R(r), 8(R13)
	MOVW	R(s), 12(R13)
	MOVW	R(M), 16(R13)

	MOVW	R(TMP), R(r)		/* numerator */
	MOVW	0(FP), R(q) 		/* denominator */
	BL  	udiv<>(SB)
	MOVW	R(r), R(TMP)
	MOVW	4(R13), R(q)
	MOVW	8(R13), R(r)
	MOVW	12(R13), R(s)
	MOVW	16(R13), R(M)
	RET

TEXT _div(SB),NOSPLIT,$16
	MOVW	R(q), 4(R13)
	MOVW	R(r), 8(R13)
	MOVW	R(s), 12(R13)
	MOVW	R(M), 16(R13)
	MOVW	R(TMP), R(r)		/* numerator */
	MOVW	0(FP), R(q) 		/* denominator */
	CMP 	$0, R(r)
	BGE 	d1
	RSB 	$0, R(r), R(r)
	CMP 	$0, R(q)
	BGE 	d2
	RSB 	$0, R(q), R(q)
d0:
	BL  	udiv<>(SB)  		/* none/both neg */
	MOVW	R(q), R(TMP)
	B		out
d1:
	CMP 	$0, R(q)
	BGE 	d0
	RSB 	$0, R(q), R(q)
d2:
	BL  	udiv<>(SB)  		/* one neg */
	RSB		$0, R(q), R(TMP)
	B   	out

TEXT _mod(SB),NOSPLIT,$16
	MOVW	R(q), 4(R13)
	MOVW	R(r), 8(R13)
	MOVW	R(s), 12(R13)
	MOVW	R(M), 16(R13)
	MOVW	R(TMP), R(r)		/* numerator */
	MOVW	0(FP), R(q) 		/* denominator */
	CMP 	$0, R(q)
	RSB.LT	$0, R(q), R(q)
	CMP 	$0, R(r)
	BGE 	m1
	RSB 	$0, R(r), R(r)
	BL  	udiv<>(SB)  		/* neg numerator */
	RSB 	$0, R(r), R(TMP)
	B   	out
m1:
	BL  	udiv<>(SB)  		/* pos numerator */
	MOVW	R(r), R(TMP)
out:
	MOVW	4(R13), R(q)
	MOVW	8(R13), R(r)
	MOVW	12(R13), R(s)
	MOVW	16(R13), R(M)
	RET
