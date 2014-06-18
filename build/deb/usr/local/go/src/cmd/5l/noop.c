// Inferno utils/5l/noop.c
// http://code.google.com/p/inferno-os/source/browse/utils/5l/noop.c
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

// Code transformations.

#include	"l.h"
#include	"../ld/lib.h"
#include	"../../pkg/runtime/stack.h"

static	Sym*	sym_div;
static	Sym*	sym_divu;
static	Sym*	sym_mod;
static	Sym*	sym_modu;
static	Sym*	symmorestack;
static	Prog*	pmorestack;

static	Prog*	stacksplit(Prog*, int32);

static void
linkcase(Prog *casep)
{
	Prog *p;

	for(p = casep; p != P; p = p->link){
		if(p->as == ABCASE) {
			for(; p != P && p->as == ABCASE; p = p->link)
				p->pcrel = casep;
			break;
		}
	}
}

void
noops(void)
{
	Prog *p, *q, *q1, *q2;
	int o;
	Sym *tlsfallback, *gmsym;

	/*
	 * find leaf subroutines
	 * strip NOPs
	 * expand RET
	 * expand BECOME pseudo
	 * fixup TLS
	 */

	if(debug['v'])
		Bprint(&bso, "%5.2f noops\n", cputime());
	Bflush(&bso);

	symmorestack = lookup("runtime.morestack", 0);
	if(symmorestack->type != STEXT) {
		diag("runtime·morestack not defined");
		errorexit();
	}
	pmorestack = symmorestack->text;
	pmorestack->reg |= NOSPLIT;

	tlsfallback = lookup("runtime.read_tls_fallback", 0);
	gmsym = S;
	if(linkmode == LinkExternal)
		gmsym = lookup("runtime.tlsgm", 0);
	q = P;
	for(cursym = textp; cursym != nil; cursym = cursym->next) {
		for(p = cursym->text; p != P; p = p->link) {
			switch(p->as) {
			case ACASE:
				if(flag_shared)
					linkcase(p);
				break;

			case ATEXT:
				p->mark |= LEAF;
				break;
	
			case ARET:
				break;
	
			case ADIV:
			case ADIVU:
			case AMOD:
			case AMODU:
				q = p;
				if(prog_div == P)
					initdiv();
				cursym->text->mark &= ~LEAF;
				continue;
	
			case ANOP:
				q1 = p->link;
				q->link = q1;		/* q is non-nop */
				if(q1 != P)
					q1->mark |= p->mark;
				continue;
	
			case ABL:
			case ABX:
				cursym->text->mark &= ~LEAF;
	
			case ABCASE:
			case AB:
	
			case ABEQ:
			case ABNE:
			case ABCS:
			case ABHS:
			case ABCC:
			case ABLO:
			case ABMI:
			case ABPL:
			case ABVS:
			case ABVC:
			case ABHI:
			case ABLS:
			case ABGE:
			case ABLT:
			case ABGT:
			case ABLE:
				q1 = p->cond;
				if(q1 != P) {
					while(q1->as == ANOP) {
						q1 = q1->link;
						p->cond = q1;
					}
				}
				break;
			case AWORD:
				// Rewrite TLS register fetch: MRC 15, 0, <reg>, C13, C0, 3
				if((p->to.offset & 0xffff0fff) == 0xee1d0f70) {
					if(HEADTYPE == Hopenbsd) {
						p->as = ARET;
					} else if(goarm < 7) {
						if(tlsfallback->type != STEXT) {
							diag("runtime·read_tls_fallback not defined");
							errorexit();
						}
						// BL runtime.read_tls_fallback(SB)
						p->as = ABL;
						p->to.type = D_BRANCH;
						p->to.sym = tlsfallback;
						p->cond = tlsfallback->text;
						p->to.offset = 0;
						cursym->text->mark &= ~LEAF;
					}
					if(linkmode == LinkExternal) {
						// runtime.tlsgm is relocated with R_ARM_TLS_LE32
						// and $runtime.tlsgm will contain the TLS offset.
						//
						// MOV $runtime.tlsgm+tlsoffset(SB), REGTMP
						// ADD REGTMP, <reg>
						//
						// In shared mode, runtime.tlsgm is relocated with
						// R_ARM_TLS_IE32 and runtime.tlsgm(SB) will point
						// to the GOT entry containing the TLS offset.
						//
						// MOV runtime.tlsgm(SB), REGTMP
						// ADD REGTMP, <reg>
						// SUB -tlsoffset, <reg>
						//
						// The SUB compensates for tlsoffset
						// used in runtime.save_gm and runtime.load_gm.
						q = p;
						p = appendp(p);
						p->as = AMOVW;
						p->scond = 14;
						p->reg = NREG;
						if(flag_shared) {
							p->from.type = D_OREG;
							p->from.offset = 0;
						} else {
							p->from.type = D_CONST;
							p->from.offset = tlsoffset;
						}
						p->from.sym = gmsym;
						p->from.name = D_EXTERN;
						p->to.type = D_REG;
						p->to.reg = REGTMP;
						p->to.offset = 0;

						p = appendp(p);
						p->as = AADD;
						p->scond = 14;
						p->reg = NREG;
						p->from.type = D_REG;
						p->from.reg = REGTMP;
						p->to.type = D_REG;
						p->to.reg = (q->to.offset & 0xf000) >> 12;
						p->to.offset = 0;

						if(flag_shared) {
							p = appendp(p);
							p->as = ASUB;
							p->scond = 14;
							p->reg = NREG;
							p->from.type = D_CONST;
							p->from.offset = -tlsoffset;
							p->to.type = D_REG;
							p->to.reg = (q->to.offset & 0xf000) >> 12;
							p->to.offset = 0;
						}
					}
				}
			}
			q = p;
		}
	}

	for(cursym = textp; cursym != nil; cursym = cursym->next) {
		for(p = cursym->text; p != P; p = p->link) {
			o = p->as;
			switch(o) {
			case ATEXT:
				autosize = p->to.offset + 4;
				if(autosize <= 4)
				if(cursym->text->mark & LEAF) {
					p->to.offset = -4;
					autosize = 0;
				}
	
				if(!autosize && !(cursym->text->mark & LEAF)) {
					if(debug['v'])
						Bprint(&bso, "save suppressed in: %s\n",
							cursym->name);
					Bflush(&bso);
					cursym->text->mark |= LEAF;
				}
				if(cursym->text->mark & LEAF) {
					cursym->leaf = 1;
					if(!autosize)
						break;
				}
	
				if(!(p->reg & NOSPLIT))
					p = stacksplit(p, autosize); // emit split check
				
				// MOVW.W		R14,$-autosize(SP)
				p = appendp(p);
				p->as = AMOVW;
				p->scond |= C_WBIT;
				p->from.type = D_REG;
				p->from.reg = REGLINK;
				p->to.type = D_OREG;
				p->to.offset = -autosize;
				p->to.reg = REGSP;
				p->spadj = autosize;
				
				if(cursym->text->reg & WRAPPER) {
					// g->panicwrap += autosize;
					// MOVW panicwrap_offset(g), R3
					// ADD $autosize, R3
					// MOVW R3 panicwrap_offset(g)
					p = appendp(p);
					p->as = AMOVW;
					p->from.type = D_OREG;
					p->from.reg = REGG;
					p->from.offset = 2*PtrSize;
					p->to.type = D_REG;
					p->to.reg = 3;
				
					p = appendp(p);
					p->as = AADD;
					p->from.type = D_CONST;
					p->from.offset = autosize;
					p->to.type = D_REG;
					p->to.reg = 3;
					
					p = appendp(p);
					p->as = AMOVW;
					p->from.type = D_REG;
					p->from.reg = 3;
					p->to.type = D_OREG;
					p->to.reg = REGG;
					p->to.offset = 2*PtrSize;
				}
				break;
	
			case ARET:
				nocache(p);
				if(cursym->text->mark & LEAF) {
					if(!autosize) {
						p->as = AB;
						p->from = zprg.from;
						if(p->to.sym) { // retjmp
							p->to.type = D_BRANCH;
							p->cond = p->to.sym->text;
						} else {
							p->to.type = D_OREG;
							p->to.offset = 0;
							p->to.reg = REGLINK;
						}
						break;
					}
				}

				if(cursym->text->reg & WRAPPER) {
					int cond;
					
					// Preserve original RET's cond, to allow RET.EQ
					// in the implementation of reflect.call.
					cond = p->scond;
					p->scond = C_SCOND_NONE;

					// g->panicwrap -= autosize;
					// MOVW panicwrap_offset(g), R3
					// SUB $autosize, R3
					// MOVW R3 panicwrap_offset(g)
					p->as = AMOVW;
					p->from.type = D_OREG;
					p->from.reg = REGG;
					p->from.offset = 2*PtrSize;
					p->to.type = D_REG;
					p->to.reg = 3;
					p = appendp(p);
				
					p->as = ASUB;
					p->from.type = D_CONST;
					p->from.offset = autosize;
					p->to.type = D_REG;
					p->to.reg = 3;
					p = appendp(p);

					p->as = AMOVW;
					p->from.type = D_REG;
					p->from.reg = 3;
					p->to.type = D_OREG;
					p->to.reg = REGG;
					p->to.offset = 2*PtrSize;
					p = appendp(p);

					p->scond = cond;
				}

				p->as = AMOVW;
				p->scond |= C_PBIT;
				p->from.type = D_OREG;
				p->from.offset = autosize;
				p->from.reg = REGSP;
				p->to.type = D_REG;
				p->to.reg = REGPC;
				// If there are instructions following
				// this ARET, they come from a branch
				// with the same stackframe, so no spadj.
				
				if(p->to.sym) { // retjmp
					p->to.reg = REGLINK;
					q2 = appendp(p);
					q2->as = AB;
					q2->to.type = D_BRANCH;
					q2->to.sym = p->to.sym;
					q2->cond = p->to.sym->text;
					p->to.sym = nil;
					p = q2;
				}
				break;
	
			case AADD:
				if(p->from.type == D_CONST && p->from.reg == NREG && p->to.type == D_REG && p->to.reg == REGSP)
					p->spadj = -p->from.offset;
				break;

			case ASUB:
				if(p->from.type == D_CONST && p->from.reg == NREG && p->to.type == D_REG && p->to.reg == REGSP)
					p->spadj = p->from.offset;
				break;

			case ADIV:
			case ADIVU:
			case AMOD:
			case AMODU:
				if(debug['M'])
					break;
				if(p->from.type != D_REG)
					break;
				if(p->to.type != D_REG)
					break;
				q1 = p;
	
				/* MOV a,4(SP) */
				p = appendp(p);
				p->as = AMOVW;
				p->line = q1->line;
				p->from.type = D_REG;
				p->from.reg = q1->from.reg;
				p->to.type = D_OREG;
				p->to.reg = REGSP;
				p->to.offset = 4;
	
				/* MOV b,REGTMP */
				p = appendp(p);
				p->as = AMOVW;
				p->line = q1->line;
				p->from.type = D_REG;
				p->from.reg = q1->reg;
				if(q1->reg == NREG)
					p->from.reg = q1->to.reg;
				p->to.type = D_REG;
				p->to.reg = REGTMP;
				p->to.offset = 0;
	
				/* CALL appropriate */
				p = appendp(p);
				p->as = ABL;
				p->line = q1->line;
				p->to.type = D_BRANCH;
				p->cond = p;
				switch(o) {
				case ADIV:
					p->cond = prog_div;
					p->to.sym = sym_div;
					break;
				case ADIVU:
					p->cond = prog_divu;
					p->to.sym = sym_divu;
					break;
				case AMOD:
					p->cond = prog_mod;
					p->to.sym = sym_mod;
					break;
				case AMODU:
					p->cond = prog_modu;
					p->to.sym = sym_modu;
					break;
				}
	
				/* MOV REGTMP, b */
				p = appendp(p);
				p->as = AMOVW;
				p->line = q1->line;
				p->from.type = D_REG;
				p->from.reg = REGTMP;
				p->from.offset = 0;
				p->to.type = D_REG;
				p->to.reg = q1->to.reg;
	
				/* ADD $8,SP */
				p = appendp(p);
				p->as = AADD;
				p->line = q1->line;
				p->from.type = D_CONST;
				p->from.reg = NREG;
				p->from.offset = 8;
				p->reg = NREG;
				p->to.type = D_REG;
				p->to.reg = REGSP;
				p->spadj = -8;
	
				/* Keep saved LR at 0(SP) after SP change. */
				/* MOVW 0(SP), REGTMP; MOVW REGTMP, -8!(SP) */
				/* TODO: Remove SP adjustments; see issue 6699. */
				q1->as = AMOVW;
				q1->from.type = D_OREG;
				q1->from.reg = REGSP;
				q1->from.offset = 0;
				q1->reg = NREG;
				q1->to.type = D_REG;
				q1->to.reg = REGTMP;

				/* SUB $8,SP */
				q1 = appendp(q1);
				q1->as = AMOVW;
				q1->from.type = D_REG;
				q1->from.reg = REGTMP;
				q1->reg = NREG;
				q1->to.type = D_OREG;
				q1->to.reg = REGSP;
				q1->to.offset = -8;
				q1->scond |= C_WBIT;
				q1->spadj = 8;
	
				break;
			case AMOVW:
				if((p->scond & C_WBIT) && p->to.type == D_OREG && p->to.reg == REGSP)
					p->spadj = -p->to.offset;
				if((p->scond & C_PBIT) && p->from.type == D_OREG && p->from.reg == REGSP && p->to.reg != REGPC)
					p->spadj = -p->from.offset;
				if(p->from.type == D_CONST && p->from.reg == REGSP && p->to.type == D_REG && p->to.reg == REGSP)
					p->spadj = -p->from.offset;
				break;
			}
		}
	}
}

static Prog*
stacksplit(Prog *p, int32 framesize)
{
	int32 arg;

	// MOVW			g_stackguard(g), R1
	p = appendp(p);
	p->as = AMOVW;
	p->from.type = D_OREG;
	p->from.reg = REGG;
	p->to.type = D_REG;
	p->to.reg = 1;
	
	if(framesize <= StackSmall) {
		// small stack: SP < stackguard
		//	CMP	stackguard, SP
		p = appendp(p);
		p->as = ACMP;
		p->from.type = D_REG;
		p->from.reg = 1;
		p->reg = REGSP;
	} else if(framesize <= StackBig) {
		// large stack: SP-framesize < stackguard-StackSmall
		//	MOVW $-framesize(SP), R2
		//	CMP stackguard, R2
		p = appendp(p);
		p->as = AMOVW;
		p->from.type = D_CONST;
		p->from.reg = REGSP;
		p->from.offset = -framesize;
		p->to.type = D_REG;
		p->to.reg = 2;
		
		p = appendp(p);
		p->as = ACMP;
		p->from.type = D_REG;
		p->from.reg = 1;
		p->reg = 2;
	} else {
		// Such a large stack we need to protect against wraparound
		// if SP is close to zero.
		//	SP-stackguard+StackGuard < framesize + (StackGuard-StackSmall)
		// The +StackGuard on both sides is required to keep the left side positive:
		// SP is allowed to be slightly below stackguard. See stack.h.
		//	CMP $StackPreempt, R1
		//	MOVW.NE $StackGuard(SP), R2
		//	SUB.NE R1, R2
		//	MOVW.NE $(framesize+(StackGuard-StackSmall)), R3
		//	CMP.NE R3, R2
		p = appendp(p);
		p->as = ACMP;
		p->from.type = D_CONST;
		p->from.offset = (uint32)StackPreempt;
		p->reg = 1;

		p = appendp(p);
		p->as = AMOVW;
		p->from.type = D_CONST;
		p->from.reg = REGSP;
		p->from.offset = StackGuard;
		p->to.type = D_REG;
		p->to.reg = 2;
		p->scond = C_SCOND_NE;
		
		p = appendp(p);
		p->as = ASUB;
		p->from.type = D_REG;
		p->from.reg = 1;
		p->to.type = D_REG;
		p->to.reg = 2;
		p->scond = C_SCOND_NE;
		
		p = appendp(p);
		p->as = AMOVW;
		p->from.type = D_CONST;
		p->from.offset = framesize + (StackGuard - StackSmall);
		p->to.type = D_REG;
		p->to.reg = 3;
		p->scond = C_SCOND_NE;
		
		p = appendp(p);
		p->as = ACMP;
		p->from.type = D_REG;
		p->from.reg = 3;
		p->reg = 2;
		p->scond = C_SCOND_NE;
	}
	
	// MOVW.LS		$framesize, R1
	p = appendp(p);
	p->as = AMOVW;
	p->scond = C_SCOND_LS;
	p->from.type = D_CONST;
	p->from.offset = framesize;
	p->to.type = D_REG;
	p->to.reg = 1;

	// MOVW.LS		$args, R2
	p = appendp(p);
	p->as = AMOVW;
	p->scond = C_SCOND_LS;
	p->from.type = D_CONST;
	arg = cursym->text->to.offset2;
	if(arg == 1) // special marker for known 0
		arg = 0;
	if(arg&3)
		diag("misaligned argument size in stack split");
	p->from.offset = arg;
	p->to.type = D_REG;
	p->to.reg = 2;

	// MOVW.LS	R14, R3
	p = appendp(p);
	p->as = AMOVW;
	p->scond = C_SCOND_LS;
	p->from.type = D_REG;
	p->from.reg = REGLINK;
	p->to.type = D_REG;
	p->to.reg = 3;

	// BL.LS		runtime.morestack(SB) // modifies LR, returns with LO still asserted
	p = appendp(p);
	p->as = ABL;
	p->scond = C_SCOND_LS;
	p->to.type = D_BRANCH;
	p->to.sym = symmorestack;
	p->cond = pmorestack;
	
	// BLS	start
	p = appendp(p);
	p->as = ABLS;
	p->to.type = D_BRANCH;
	p->cond = cursym->text->link;
	
	return p;
}

static void
sigdiv(char *n)
{
	Sym *s;

	s = lookup(n, 0);
	if(s->type == STEXT)
		if(s->sig == 0)
			s->sig = SIGNINTERN;
}

void
divsig(void)
{
	sigdiv("_div");
	sigdiv("_divu");
	sigdiv("_mod");
	sigdiv("_modu");
}

void
initdiv(void)
{
	Sym *s2, *s3, *s4, *s5;

	if(prog_div != P)
		return;
	sym_div = s2 = lookup("_div", 0);
	sym_divu = s3 = lookup("_divu", 0);
	sym_mod = s4 = lookup("_mod", 0);
	sym_modu = s5 = lookup("_modu", 0);
	prog_div = s2->text;
	prog_divu = s3->text;
	prog_mod = s4->text;
	prog_modu = s5->text;
	if(prog_div == P) {
		diag("undefined: %s", s2->name);
		prog_div = cursym->text;
	}
	if(prog_divu == P) {
		diag("undefined: %s", s3->name);
		prog_divu = cursym->text;
	}
	if(prog_mod == P) {
		diag("undefined: %s", s4->name);
		prog_mod = cursym->text;
	}
	if(prog_modu == P) {
		diag("undefined: %s", s5->name);
		prog_modu = cursym->text;
	}
}

void
nocache(Prog *p)
{
	p->optab = 0;
	p->from.class = 0;
	p->to.class = 0;
}
