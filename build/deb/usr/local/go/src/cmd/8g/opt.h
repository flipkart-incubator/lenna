// Derived from Inferno utils/6c/gc.h
// http://code.google.com/p/inferno-os/source/browse/utils/6c/gc.h
//
//	Copyright © 1994-1999 Lucent Technologies Inc.  All rights reserved.
//	Portions Copyright © 1995-1997 C H Forsyth (forsyth@terzarima.net)
//	Portions Copyright © 1997-1999 Vita Nuova Limited
//	Portions Copyright © 2000-2007 Vita Nuova Holdings Limited (www.vitanuova.com)
//	Portions Copyright © 2004,2006 Bruce Ellis
//	Portions Copyright © 2005-2007 C H Forsyth (forsyth@terzarima.net)
//	Revisions Copyright © 2000-2007 Lucent Technologies Inc. and others
//	Portions Copyright © 2009 The Go Authors.  All rights reserved.
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

#include	"../gc/popt.h"

#define	Z	N
#define	Adr	Addr

#define	D_HI	D_NONE
#define	D_LO	D_NONE

#define	BLOAD(r)	band(bnot(r->refbehind), r->refahead)
#define	BSTORE(r)	band(bnot(r->calbehind), r->calahead)
#define	LOAD(r)		(~r->refbehind.b[z] & r->refahead.b[z])
#define	STORE(r)	(~r->calbehind.b[z] & r->calahead.b[z])

#define	CLOAD	5
#define	CREF	5
#define	CINF	1000
#define	LOOP	3

typedef	struct	Reg	Reg;
typedef	struct	Rgn	Rgn;

// A Reg is a wrapper around a single Prog (one instruction) that holds
// register optimization information while the optimizer runs.
// r->prog is the instruction.
// r->prog->opt points back to r.
struct	Reg
{
	Flow	f;

	Bits	set;  		// variables written by this instruction.
	Bits	use1; 		// variables read by prog->from.
	Bits	use2; 		// variables read by prog->to.

	Bits	refbehind;
	Bits	refahead;
	Bits	calbehind;
	Bits	calahead;
	Bits	regdiff;
	Bits	act;

	int32	regu;		// register used bitmap
	int32	rpo;		// reverse post ordering
	int32	active;

	uint16	loop;		// x5 for every loop
	uchar	refset;		// diagnostic generated

	Reg*	p1;     	// predecessors of this instruction: p1,
	Reg*	p2;     	// and then p2 linked though p2link.
	Reg*	p2link;
	Reg*	s1;     	// successors of this instruction (at most two: s1 and s2).
	Reg*	s2;
	Reg*	link;   	// next instruction in function code
	Prog*	prog;   	// actual instruction
};
#define	R	((Reg*)0)

#define	NRGN	600
struct	Rgn
{
	Reg*	enter;
	short	cost;
	short	varno;
	short	regno;
};

EXTERN	int32	exregoffset;		// not set
EXTERN	int32	exfregoffset;		// not set
EXTERN	Reg	zreg;
EXTERN	Reg*	freer;
EXTERN	Reg**	rpo2r;
EXTERN	Rgn	region[NRGN];
EXTERN	Rgn*	rgp;
EXTERN	int	nregion;
EXTERN	int	nvar;
EXTERN	int32	regbits;
EXTERN	int32	exregbits;
EXTERN	Bits	externs;
EXTERN	Bits	params;
EXTERN	Bits	consts;
EXTERN	Bits	addrs;
EXTERN	Bits	ovar;
EXTERN	int	change;
EXTERN	int32	maxnr;
EXTERN	int32*	idom;

EXTERN	struct
{
	int32	ncvtreg;
	int32	nspill;
	int32	nreload;
	int32	ndelmov;
	int32	nvar;
	int32	naddr;
} ostats;

/*
 * reg.c
 */
Reg*	rega(void);
int	rcmp(const void*, const void*);
void	regopt(Prog*);
void	addmove(Reg*, int, int, int);
Bits	mkvar(Reg*, Adr*);
void	prop(Reg*, Bits, Bits);
void	loopit(Reg*, int32);
void	synch(Reg*, Bits);
uint32	allreg(uint32, Rgn*);
void	paint1(Reg*, int);
uint32	paint2(Reg*, int);
void	paint3(Reg*, int, int32, int);
void	addreg(Adr*, int);
void	dumpone(Flow*, int);
void	dumpit(char*, Flow*, int);

/*
 * peep.c
 */
void	peep(Prog*);
void	excise(Flow*);
int	copyu(Prog*, Adr*, Adr*);

int32	RtoB(int);
int32	FtoB(int);
int	BtoR(int32);
int	BtoF(int32);

#pragma	varargck	type	"D"	Adr*

/*
 * prog.c
 */
typedef struct ProgInfo ProgInfo;
struct ProgInfo
{
	uint32 flags; // the bits below
	uint32 reguse; // required registers used by this instruction
	uint32 regset; // required registers set by this instruction
	uint32 regindex; // registers used by addressing mode
};

enum
{
	// Pseudo-op, like TEXT, GLOBL, TYPE, PCDATA, FUNCDATA.
	Pseudo = 1<<1,
	
	// There's nothing to say about the instruction,
	// but it's still okay to see.
	OK = 1<<2,

	// Size of right-side write, or right-side read if no write.
	SizeB = 1<<3,
	SizeW = 1<<4,
	SizeL = 1<<5,
	SizeQ = 1<<6,
	SizeF = 1<<7, // float aka float32
	SizeD = 1<<8, // double aka float64

	// Left side: address taken, read, write.
	LeftAddr = 1<<9,
	LeftRead = 1<<10,
	LeftWrite = 1<<11,
	
	// Right side: address taken, read, write.
	RightAddr = 1<<12,
	RightRead = 1<<13,
	RightWrite = 1<<14,

	// Set, use, or kill of carry bit.
	// Kill means we never look at the carry bit after this kind of instruction.
	SetCarry = 1<<15,
	UseCarry = 1<<16,
	KillCarry = 1<<17,

	// Instruction kinds
	Move = 1<<18, // straight move
	Conv = 1<<19, // size conversion
	Cjmp = 1<<20, // conditional jump
	Break = 1<<21, // breaks control flow (no fallthrough)
	Call = 1<<22, // function call
	Jump = 1<<23, // jump
	Skip = 1<<24, // data instruction

	// Special cases for register use.
	ShiftCX = 1<<25, // possible shift by CX
	ImulAXDX = 1<<26, // possible multiply into DX:AX
};

void proginfo(ProgInfo*, Prog*);
