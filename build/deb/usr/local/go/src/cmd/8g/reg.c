// Derived from Inferno utils/6c/reg.c
// http://code.google.com/p/inferno-os/source/browse/utils/6c/reg.c
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

#include <u.h>
#include <libc.h>
#include "gg.h"
#include "opt.h"

#define	NREGVAR	16	/* 8 integer + 8 floating */
#define	REGBITS	((uint32)0xffff)

static	Reg*	firstr;
static	int	first	= 1;

int
rcmp(const void *a1, const void *a2)
{
	Rgn *p1, *p2;
	int c1, c2;

	p1 = (Rgn*)a1;
	p2 = (Rgn*)a2;
	c1 = p2->cost;
	c2 = p1->cost;
	if(c1 -= c2)
		return c1;
	return p2->varno - p1->varno;
}

static void
setoutvar(void)
{
	Type *t;
	Node *n;
	Addr a;
	Iter save;
	Bits bit;
	int z;

	t = structfirst(&save, getoutarg(curfn->type));
	while(t != T) {
		n = nodarg(t, 1);
		a = zprog.from;
		naddr(n, &a, 0);
		bit = mkvar(R, &a);
		for(z=0; z<BITS; z++)
			ovar.b[z] |= bit.b[z];
		t = structnext(&save);
	}
//if(bany(ovar))
//print("ovars = %Q\n", ovar);
}

static void
setaddrs(Bits bit)
{
	int i, n;
	Var *v;
	Node *node;

	while(bany(&bit)) {
		// convert each bit to a variable
		i = bnum(bit);
		node = var[i].node;
		n = var[i].name;
		bit.b[i/32] &= ~(1L<<(i%32));

		// disable all pieces of that variable
		for(i=0; i<nvar; i++) {
			v = var+i;
			if(v->node == node && v->name == n)
				v->addr = 2;
		}
	}
}

static char* regname[] = {
	".ax", ".cx", ".dx", ".bx", ".sp", ".bp", ".si", ".di",
	".x0", ".x1", ".x2", ".x3", ".x4", ".x5", ".x6", ".x7",
};

static Node* regnodes[NREGVAR];

void
regopt(Prog *firstp)
{
	Reg *r, *r1;
	Prog *p;
	Graph *g;
	ProgInfo info;
	int i, z;
	uint32 vreg;
	Bits bit;

	if(first) {
		fmtinstall('Q', Qconv);
		exregoffset = D_DI;	// no externals
		first = 0;
	}
	
	fixjmp(firstp);
	mergetemp(firstp);

	/*
	 * control flow is more complicated in generated go code
	 * than in generated c code.  define pseudo-variables for
	 * registers, so we have complete register usage information.
	 */
	nvar = NREGVAR;
	memset(var, 0, NREGVAR*sizeof var[0]);
	for(i=0; i<NREGVAR; i++) {
		if(regnodes[i] == N)
			regnodes[i] = newname(lookup(regname[i]));
		var[i].node = regnodes[i];
	}

	regbits = RtoB(D_SP);
	for(z=0; z<BITS; z++) {
		externs.b[z] = 0;
		params.b[z] = 0;
		consts.b[z] = 0;
		addrs.b[z] = 0;
		ovar.b[z] = 0;
	}

	// build list of return variables
	setoutvar();

	/*
	 * pass 1
	 * build aux data structure
	 * allocate pcs
	 * find use and set of variables
	 */
	g = flowstart(firstp, sizeof(Reg));
	if(g == nil)
		return;
	firstr = (Reg*)g->start;

	for(r = firstr; r != R; r = (Reg*)r->f.link) {
		p = r->f.prog;
		proginfo(&info, p);

		// Avoid making variables for direct-called functions.
		if(p->as == ACALL && p->to.type == D_EXTERN)
			continue;

		r->use1.b[0] |= info.reguse | info.regindex;
		r->set.b[0] |= info.regset;

		bit = mkvar(r, &p->from);
		if(bany(&bit)) {
			if(info.flags & LeftAddr)
				setaddrs(bit);
			if(info.flags & LeftRead)
				for(z=0; z<BITS; z++)
					r->use1.b[z] |= bit.b[z];
			if(info.flags & LeftWrite)
				for(z=0; z<BITS; z++)
					r->set.b[z] |= bit.b[z];
		}

		bit = mkvar(r, &p->to);
		if(bany(&bit)) {	
			if(info.flags & RightAddr)
				setaddrs(bit);
			if(info.flags & RightRead)
				for(z=0; z<BITS; z++)
					r->use2.b[z] |= bit.b[z];
			if(info.flags & RightWrite)
				for(z=0; z<BITS; z++)
					r->set.b[z] |= bit.b[z];
		}
	}
	if(firstr == R)
		return;

	for(i=0; i<nvar; i++) {
		Var *v = var+i;
		if(v->addr) {
			bit = blsh(i);
			for(z=0; z<BITS; z++)
				addrs.b[z] |= bit.b[z];
		}

		if(debug['R'] && debug['v'])
			print("bit=%2d addr=%d et=%-6E w=%-2d s=%N + %lld\n",
				i, v->addr, v->etype, v->width, v->node, v->offset);
	}

	if(debug['R'] && debug['v'])
		dumpit("pass1", &firstr->f, 1);

	/*
	 * pass 2
	 * find looping structure
	 */
	flowrpo(g);

	if(debug['R'] && debug['v'])
		dumpit("pass2", &firstr->f, 1);

	/*
	 * pass 3
	 * iterate propagating usage
	 * 	back until flow graph is complete
	 */
loop1:
	change = 0;
	for(r = firstr; r != R; r = (Reg*)r->f.link)
		r->f.active = 0;
	for(r = firstr; r != R; r = (Reg*)r->f.link)
		if(r->f.prog->as == ARET)
			prop(r, zbits, zbits);
loop11:
	/* pick up unreachable code */
	i = 0;
	for(r = firstr; r != R; r = r1) {
		r1 = (Reg*)r->f.link;
		if(r1 && r1->f.active && !r->f.active) {
			prop(r, zbits, zbits);
			i = 1;
		}
	}
	if(i)
		goto loop11;
	if(change)
		goto loop1;

	if(debug['R'] && debug['v'])
		dumpit("pass3", &firstr->f, 1);

	/*
	 * pass 4
	 * iterate propagating register/variable synchrony
	 * 	forward until graph is complete
	 */
loop2:
	change = 0;
	for(r = firstr; r != R; r = (Reg*)r->f.link)
		r->f.active = 0;
	synch(firstr, zbits);
	if(change)
		goto loop2;

	if(debug['R'] && debug['v'])
		dumpit("pass4", &firstr->f, 1);

	/*
	 * pass 4.5
	 * move register pseudo-variables into regu.
	 */
	for(r = firstr; r != R; r = (Reg*)r->f.link) {
		r->regu = (r->refbehind.b[0] | r->set.b[0]) & REGBITS;

		r->set.b[0] &= ~REGBITS;
		r->use1.b[0] &= ~REGBITS;
		r->use2.b[0] &= ~REGBITS;
		r->refbehind.b[0] &= ~REGBITS;
		r->refahead.b[0] &= ~REGBITS;
		r->calbehind.b[0] &= ~REGBITS;
		r->calahead.b[0] &= ~REGBITS;
		r->regdiff.b[0] &= ~REGBITS;
		r->act.b[0] &= ~REGBITS;
	}

	/*
	 * pass 5
	 * isolate regions
	 * calculate costs (paint1)
	 */
	r = firstr;
	if(r) {
		for(z=0; z<BITS; z++)
			bit.b[z] = (r->refahead.b[z] | r->calahead.b[z]) &
			  ~(externs.b[z] | params.b[z] | addrs.b[z] | consts.b[z]);
		if(bany(&bit) && !r->f.refset) {
			// should never happen - all variables are preset
			if(debug['w'])
				print("%L: used and not set: %Q\n", r->f.prog->lineno, bit);
			r->f.refset = 1;
		}
	}
	for(r = firstr; r != R; r = (Reg*)r->f.link)
		r->act = zbits;
	rgp = region;
	nregion = 0;
	for(r = firstr; r != R; r = (Reg*)r->f.link) {
		for(z=0; z<BITS; z++)
			bit.b[z] = r->set.b[z] &
			  ~(r->refahead.b[z] | r->calahead.b[z] | addrs.b[z]);
		if(bany(&bit) && !r->f.refset) {
			if(debug['w'])
				print("%L: set and not used: %Q\n", r->f.prog->lineno, bit);
			r->f.refset = 1;
			excise(&r->f);
		}
		for(z=0; z<BITS; z++)
			bit.b[z] = LOAD(r) & ~(r->act.b[z] | addrs.b[z]);
		while(bany(&bit)) {
			i = bnum(bit);
			rgp->enter = r;
			rgp->varno = i;
			change = 0;
			paint1(r, i);
			bit.b[i/32] &= ~(1L<<(i%32));
			if(change <= 0)
				continue;
			rgp->cost = change;
			nregion++;
			if(nregion >= NRGN) {
				if(debug['R'] && debug['v'])
					print("too many regions\n");
				goto brk;
			}
			rgp++;
		}
	}
brk:
	qsort(region, nregion, sizeof(region[0]), rcmp);

	/*
	 * pass 6
	 * determine used registers (paint2)
	 * replace code (paint3)
	 */
	rgp = region;
	for(i=0; i<nregion; i++) {
		bit = blsh(rgp->varno);
		vreg = paint2(rgp->enter, rgp->varno);
		vreg = allreg(vreg, rgp);
		if(rgp->regno != 0)
			paint3(rgp->enter, rgp->varno, vreg, rgp->regno);
		rgp++;
	}

	if(debug['R'] && debug['v'])
		dumpit("pass6", &firstr->f, 1);

	/*
	 * free aux structures. peep allocates new ones.
	 */
	flowend(g);
	firstr = R;

	/*
	 * pass 7
	 * peep-hole on basic block
	 */
	if(!debug['R'] || debug['P'])
		peep(firstp);

	/*
	 * eliminate nops
	 */
	for(p=firstp; p!=P; p=p->link) {
		while(p->link != P && p->link->as == ANOP)
			p->link = p->link->link;
		if(p->to.type == D_BRANCH)
			while(p->to.u.branch != P && p->to.u.branch->as == ANOP)
				p->to.u.branch = p->to.u.branch->link;
	}

	if(!use_sse)
	for(p=firstp; p!=P; p=p->link) {
		if(p->from.type >= D_X0 && p->from.type <= D_X7)
			fatal("invalid use of %R with GO386=387: %P", p->from.type, p);
		if(p->to.type >= D_X0 && p->to.type <= D_X7)
			fatal("invalid use of %R with GO386=387: %P", p->to.type, p);
	}

	if(debug['R']) {
		if(ostats.ncvtreg ||
		   ostats.nspill ||
		   ostats.nreload ||
		   ostats.ndelmov ||
		   ostats.nvar ||
		   ostats.naddr ||
		   0)
			print("\nstats\n");

		if(ostats.ncvtreg)
			print("	%4d cvtreg\n", ostats.ncvtreg);
		if(ostats.nspill)
			print("	%4d spill\n", ostats.nspill);
		if(ostats.nreload)
			print("	%4d reload\n", ostats.nreload);
		if(ostats.ndelmov)
			print("	%4d delmov\n", ostats.ndelmov);
		if(ostats.nvar)
			print("	%4d var\n", ostats.nvar);
		if(ostats.naddr)
			print("	%4d addr\n", ostats.naddr);

		memset(&ostats, 0, sizeof(ostats));
	}
}

/*
 * add mov b,rn
 * just after r
 */
void
addmove(Reg *r, int bn, int rn, int f)
{
	Prog *p, *p1;
	Adr *a;
	Var *v;

	p1 = mal(sizeof(*p1));
	clearp(p1);
	p1->loc = 9999;

	p = r->f.prog;
	p1->link = p->link;
	p->link = p1;
	p1->lineno = p->lineno;

	v = var + bn;

	a = &p1->to;
	a->offset = v->offset;
	a->etype = v->etype;
	a->type = v->name;
	a->node = v->node;
	a->sym = v->node->sym;

	// need to clean this up with wptr and
	// some of the defaults
	p1->as = AMOVL;
	switch(v->etype) {
	default:
		fatal("unknown type %E", v->etype);
	case TINT8:
	case TUINT8:
	case TBOOL:
		p1->as = AMOVB;
		break;
	case TINT16:
	case TUINT16:
		p1->as = AMOVW;
		break;
	case TFLOAT32:
		p1->as = AMOVSS;
		break;
	case TFLOAT64:
		p1->as = AMOVSD;
		break;
	case TINT:
	case TUINT:
	case TINT32:
	case TUINT32:
	case TPTR32:
		break;
	}

	p1->from.type = rn;
	if(!f) {
		p1->from = *a;
		*a = zprog.from;
		a->type = rn;
		if(v->etype == TUINT8)
			p1->as = AMOVB;
		if(v->etype == TUINT16)
			p1->as = AMOVW;
	}
	if(debug['R'] && debug['v'])
		print("%P ===add=== %P\n", p, p1);
	ostats.nspill++;
}

uint32
doregbits(int r)
{
	uint32 b;

	b = 0;
	if(r >= D_INDIR)
		r -= D_INDIR;
	if(r >= D_AX && r <= D_DI)
		b |= RtoB(r);
	else
	if(r >= D_AL && r <= D_BL)
		b |= RtoB(r-D_AL+D_AX);
	else
	if(r >= D_AH && r <= D_BH)
		b |= RtoB(r-D_AH+D_AX);
	else
	if(r >= D_X0 && r <= D_X0+7)
		b |= FtoB(r);
	return b;
}

static int
overlap(int32 o1, int w1, int32 o2, int w2)
{
	int32 t1, t2;

	t1 = o1+w1;
	t2 = o2+w2;

	if(!(t1 > o2 && t2 > o1))
		return 0;

	return 1;
}

Bits
mkvar(Reg *r, Adr *a)
{
	Var *v;
	int i, t, n, et, z, w, flag, regu;
	int32 o;
	Bits bit;
	Node *node;

	/*
	 * mark registers used
	 */
	t = a->type;
	if(t == D_NONE)
		goto none;

	if(r != R)
		r->use1.b[0] |= doregbits(a->index);

	switch(t) {
	default:
		regu = doregbits(t);
		if(regu == 0)
			goto none;
		bit = zbits;
		bit.b[0] = regu;
		return bit;

	case D_ADDR:
		a->type = a->index;
		bit = mkvar(r, a);
		setaddrs(bit);
		a->type = t;
		ostats.naddr++;
		goto none;

	case D_EXTERN:
	case D_STATIC:
	case D_PARAM:
	case D_AUTO:
		n = t;
		break;
	}

	node = a->node;
	if(node == N || node->op != ONAME || node->orig == N)
		goto none;
	node = node->orig;
	if(node->orig != node)
		fatal("%D: bad node", a);
	if(node->sym == S || node->sym->name[0] == '.')
		goto none;
	et = a->etype;
	o = a->offset;
	w = a->width;
	if(w < 0)
		fatal("bad width %d for %D", w, a);

	flag = 0;
	for(i=0; i<nvar; i++) {
		v = var+i;
		if(v->node == node && v->name == n) {
			if(v->offset == o)
			if(v->etype == et)
			if(v->width == w)
				return blsh(i);

			// if they overlap, disable both
			if(overlap(v->offset, v->width, o, w)) {
				if(debug['R'])
					print("disable %s\n", node->sym->name);
				v->addr = 1;
				flag = 1;
			}
		}
	}

	switch(et) {
	case 0:
	case TFUNC:
		goto none;
	}

	if(nvar >= NVAR) {
		if(debug['w'] > 1 && node != N)
			fatal("variable not optimized: %D", a);
		goto none;
	}

	i = nvar;
	nvar++;
	v = var+i;
	v->offset = o;
	v->name = n;
	v->etype = et;
	v->width = w;
	v->addr = flag;		// funny punning
	v->node = node;

	if(debug['R'])
		print("bit=%2d et=%2E w=%d+%d %#N %D flag=%d\n", i, et, o, w, node, a, v->addr);
	ostats.nvar++;

	bit = blsh(i);
	if(n == D_EXTERN || n == D_STATIC)
		for(z=0; z<BITS; z++)
			externs.b[z] |= bit.b[z];
	if(n == D_PARAM)
		for(z=0; z<BITS; z++)
			params.b[z] |= bit.b[z];

	return bit;

none:
	return zbits;
}

void
prop(Reg *r, Bits ref, Bits cal)
{
	Reg *r1, *r2;
	int z;

	for(r1 = r; r1 != R; r1 = (Reg*)r1->f.p1) {
		for(z=0; z<BITS; z++) {
			ref.b[z] |= r1->refahead.b[z];
			if(ref.b[z] != r1->refahead.b[z]) {
				r1->refahead.b[z] = ref.b[z];
				change++;
			}
			cal.b[z] |= r1->calahead.b[z];
			if(cal.b[z] != r1->calahead.b[z]) {
				r1->calahead.b[z] = cal.b[z];
				change++;
			}
		}
		switch(r1->f.prog->as) {
		case ACALL:
			if(noreturn(r1->f.prog))
				break;
			for(z=0; z<BITS; z++) {
				cal.b[z] |= ref.b[z] | externs.b[z];
				ref.b[z] = 0;
			}
			break;

		case ATEXT:
			for(z=0; z<BITS; z++) {
				cal.b[z] = 0;
				ref.b[z] = 0;
			}
			break;

		case ARET:
			for(z=0; z<BITS; z++) {
				cal.b[z] = externs.b[z] | ovar.b[z];
				ref.b[z] = 0;
			}
			break;

		default:
			// Work around for issue 1304:
			// flush modified globals before each instruction.
			for(z=0; z<BITS; z++) {
				cal.b[z] |= externs.b[z];
				// issue 4066: flush modified return variables in case of panic
				if(hasdefer)
					cal.b[z] |= ovar.b[z];
			}
			break;
		}
		for(z=0; z<BITS; z++) {
			ref.b[z] = (ref.b[z] & ~r1->set.b[z]) |
				r1->use1.b[z] | r1->use2.b[z];
			cal.b[z] &= ~(r1->set.b[z] | r1->use1.b[z] | r1->use2.b[z]);
			r1->refbehind.b[z] = ref.b[z];
			r1->calbehind.b[z] = cal.b[z];
		}
		if(r1->f.active)
			break;
		r1->f.active = 1;
	}
	for(; r != r1; r = (Reg*)r->f.p1)
		for(r2 = (Reg*)r->f.p2; r2 != R; r2 = (Reg*)r2->f.p2link)
			prop(r2, r->refbehind, r->calbehind);
}

void
synch(Reg *r, Bits dif)
{
	Reg *r1;
	int z;

	for(r1 = r; r1 != R; r1 = (Reg*)r1->f.s1) {
		for(z=0; z<BITS; z++) {
			dif.b[z] = (dif.b[z] &
				~(~r1->refbehind.b[z] & r1->refahead.b[z])) |
					r1->set.b[z] | r1->regdiff.b[z];
			if(dif.b[z] != r1->regdiff.b[z]) {
				r1->regdiff.b[z] = dif.b[z];
				change++;
			}
		}
		if(r1->f.active)
			break;
		r1->f.active = 1;
		for(z=0; z<BITS; z++)
			dif.b[z] &= ~(~r1->calbehind.b[z] & r1->calahead.b[z]);
		if((Reg*)r1->f.s2 != R)
			synch((Reg*)r1->f.s2, dif);
	}
}

uint32
allreg(uint32 b, Rgn *r)
{
	Var *v;
	int i;

	v = var + r->varno;
	r->regno = 0;
	switch(v->etype) {

	default:
		fatal("unknown etype %d/%E", bitno(b), v->etype);
		break;

	case TINT8:
	case TUINT8:
	case TINT16:
	case TUINT16:
	case TINT32:
	case TUINT32:
	case TINT64:
	case TINT:
	case TUINT:
	case TUINTPTR:
	case TBOOL:
	case TPTR32:
		i = BtoR(~b);
		if(i && r->cost > 0) {
			r->regno = i;
			return RtoB(i);
		}
		break;

	case TFLOAT32:
	case TFLOAT64:
		if(!use_sse)
			break;
		i = BtoF(~b);
		if(i && r->cost > 0) {
			r->regno = i;
			return FtoB(i);
		}
		break;
	}
	return 0;
}

void
paint1(Reg *r, int bn)
{
	Reg *r1;
	Prog *p;
	int z;
	uint32 bb;

	z = bn/32;
	bb = 1L<<(bn%32);
	if(r->act.b[z] & bb)
		return;
	for(;;) {
		if(!(r->refbehind.b[z] & bb))
			break;
		r1 = (Reg*)r->f.p1;
		if(r1 == R)
			break;
		if(!(r1->refahead.b[z] & bb))
			break;
		if(r1->act.b[z] & bb)
			break;
		r = r1;
	}

	if(LOAD(r) & ~(r->set.b[z]&~(r->use1.b[z]|r->use2.b[z])) & bb) {
		change -= CLOAD * r->f.loop;
	}
	for(;;) {
		r->act.b[z] |= bb;
		p = r->f.prog;

		if(r->use1.b[z] & bb) {
			change += CREF * r->f.loop;
			if(p->as == AFMOVL || p->as == AFMOVW)
				if(BtoR(bb) != D_F0)
					change = -CINF;
		}

		if((r->use2.b[z]|r->set.b[z]) & bb) {
			change += CREF * r->f.loop;
			if(p->as == AFMOVL || p->as == AFMOVW)
				if(BtoR(bb) != D_F0)
					change = -CINF;
		}

		if(STORE(r) & r->regdiff.b[z] & bb) {
			change -= CLOAD * r->f.loop;
			if(p->as == AFMOVL || p->as == AFMOVW)
				if(BtoR(bb) != D_F0)
					change = -CINF;
		}

		if(r->refbehind.b[z] & bb)
			for(r1 = (Reg*)r->f.p2; r1 != R; r1 = (Reg*)r1->f.p2link)
				if(r1->refahead.b[z] & bb)
					paint1(r1, bn);

		if(!(r->refahead.b[z] & bb))
			break;
		r1 = (Reg*)r->f.s2;
		if(r1 != R)
			if(r1->refbehind.b[z] & bb)
				paint1(r1, bn);
		r = (Reg*)r->f.s1;
		if(r == R)
			break;
		if(r->act.b[z] & bb)
			break;
		if(!(r->refbehind.b[z] & bb))
			break;
	}
}

uint32
regset(Reg *r, uint32 bb)
{
	uint32 b, set;
	Adr v;
	int c;

	set = 0;
	v = zprog.from;
	while(b = bb & ~(bb-1)) {
		v.type = b & 0xFF ? BtoR(b): BtoF(b);
		c = copyu(r->f.prog, &v, A);
		if(c == 3)
			set |= b;
		bb &= ~b;
	}
	return set;
}

uint32
reguse(Reg *r, uint32 bb)
{
	uint32 b, set;
	Adr v;
	int c;

	set = 0;
	v = zprog.from;
	while(b = bb & ~(bb-1)) {
		v.type = b & 0xFF ? BtoR(b): BtoF(b);
		c = copyu(r->f.prog, &v, A);
		if(c == 1 || c == 2 || c == 4)
			set |= b;
		bb &= ~b;
	}
	return set;
}

uint32
paint2(Reg *r, int bn)
{
	Reg *r1;
	int z;
	uint32 bb, vreg, x;

	z = bn/32;
	bb = 1L << (bn%32);
	vreg = regbits;
	if(!(r->act.b[z] & bb))
		return vreg;
	for(;;) {
		if(!(r->refbehind.b[z] & bb))
			break;
		r1 = (Reg*)r->f.p1;
		if(r1 == R)
			break;
		if(!(r1->refahead.b[z] & bb))
			break;
		if(!(r1->act.b[z] & bb))
			break;
		r = r1;
	}
	for(;;) {
		r->act.b[z] &= ~bb;

		vreg |= r->regu;

		if(r->refbehind.b[z] & bb)
			for(r1 = (Reg*)r->f.p2; r1 != R; r1 = (Reg*)r1->f.p2link)
				if(r1->refahead.b[z] & bb)
					vreg |= paint2(r1, bn);

		if(!(r->refahead.b[z] & bb))
			break;
		r1 = (Reg*)r->f.s2;
		if(r1 != R)
			if(r1->refbehind.b[z] & bb)
				vreg |= paint2(r1, bn);
		r = (Reg*)r->f.s1;
		if(r == R)
			break;
		if(!(r->act.b[z] & bb))
			break;
		if(!(r->refbehind.b[z] & bb))
			break;
	}

	bb = vreg;
	for(; r; r=(Reg*)r->f.s1) {
		x = r->regu & ~bb;
		if(x) {
			vreg |= reguse(r, x);
			bb |= regset(r, x);
		}
	}
	return vreg;
}

void
paint3(Reg *r, int bn, int32 rb, int rn)
{
	Reg *r1;
	Prog *p;
	int z;
	uint32 bb;

	z = bn/32;
	bb = 1L << (bn%32);
	if(r->act.b[z] & bb)
		return;
	for(;;) {
		if(!(r->refbehind.b[z] & bb))
			break;
		r1 = (Reg*)r->f.p1;
		if(r1 == R)
			break;
		if(!(r1->refahead.b[z] & bb))
			break;
		if(r1->act.b[z] & bb)
			break;
		r = r1;
	}

	if(LOAD(r) & ~(r->set.b[z] & ~(r->use1.b[z]|r->use2.b[z])) & bb)
		addmove(r, bn, rn, 0);
	for(;;) {
		r->act.b[z] |= bb;
		p = r->f.prog;

		if(r->use1.b[z] & bb) {
			if(debug['R'] && debug['v'])
				print("%P", p);
			addreg(&p->from, rn);
			if(debug['R'] && debug['v'])
				print(" ===change== %P\n", p);
		}
		if((r->use2.b[z]|r->set.b[z]) & bb) {
			if(debug['R'] && debug['v'])
				print("%P", p);
			addreg(&p->to, rn);
			if(debug['R'] && debug['v'])
				print(" ===change== %P\n", p);
		}

		if(STORE(r) & r->regdiff.b[z] & bb)
			addmove(r, bn, rn, 1);
		r->regu |= rb;

		if(r->refbehind.b[z] & bb)
			for(r1 = (Reg*)r->f.p2; r1 != R; r1 = (Reg*)r1->f.p2link)
				if(r1->refahead.b[z] & bb)
					paint3(r1, bn, rb, rn);

		if(!(r->refahead.b[z] & bb))
			break;
		r1 = (Reg*)r->f.s2;
		if(r1 != R)
			if(r1->refbehind.b[z] & bb)
				paint3(r1, bn, rb, rn);
		r = (Reg*)r->f.s1;
		if(r == R)
			break;
		if(r->act.b[z] & bb)
			break;
		if(!(r->refbehind.b[z] & bb))
			break;
	}
}

void
addreg(Adr *a, int rn)
{

	a->sym = 0;
	a->offset = 0;
	a->type = rn;

	ostats.ncvtreg++;
}

int32
RtoB(int r)
{

	if(r < D_AX || r > D_DI)
		return 0;
	return 1L << (r-D_AX);
}

int
BtoR(int32 b)
{

	b &= 0xffL;
	if(b == 0)
		return 0;
	return bitno(b) + D_AX;
}

int32
FtoB(int f)
{
	if(f < D_X0 || f > D_X7)
		return 0;
	return 1L << (f - D_X0 + 8);
}

int
BtoF(int32 b)
{
	b &= 0xFF00L;
	if(b == 0)
		return 0;
	return bitno(b) - 8 + D_X0;
}

void
dumpone(Flow *f, int isreg)
{
	int z;
	Bits bit;
	Reg *r;

	print("%d:%P", f->loop, f->prog);
	if(isreg) {
		r = (Reg*)f;
		for(z=0; z<BITS; z++)
			bit.b[z] =
				r->set.b[z] |
				r->use1.b[z] |
				r->use2.b[z] |
				r->refbehind.b[z] |
				r->refahead.b[z] |
				r->calbehind.b[z] |
				r->calahead.b[z] |
				r->regdiff.b[z] |
				r->act.b[z] |
					0;
		if(bany(&bit)) {
			print("\t");
			if(bany(&r->set))
				print(" s:%Q", r->set);
			if(bany(&r->use1))
				print(" u1:%Q", r->use1);
			if(bany(&r->use2))
				print(" u2:%Q", r->use2);
			if(bany(&r->refbehind))
				print(" rb:%Q ", r->refbehind);
			if(bany(&r->refahead))
				print(" ra:%Q ", r->refahead);
			if(bany(&r->calbehind))
				print(" cb:%Q ", r->calbehind);
			if(bany(&r->calahead))
				print(" ca:%Q ", r->calahead);
			if(bany(&r->regdiff))
				print(" d:%Q ", r->regdiff);
			if(bany(&r->act))
				print(" a:%Q ", r->act);
		}
	}
	print("\n");
}

void
dumpit(char *str, Flow *r0, int isreg)
{
	Flow *r, *r1;

	print("\n%s\n", str);
	for(r = r0; r != nil; r = r->link) {
		dumpone(r, isreg);
		r1 = r->p2;
		if(r1 != nil) {
			print("	pred:");
			for(; r1 != nil; r1 = r->p2link)
				print(" %.4ud", r1->prog->loc);
			print("\n");
		}
//		r1 = r->s1;
//		if(r1 != nil) {
//			print("	succ:");
//			for(; r1 != R; r1 = r1->s1)
//				print(" %.4ud", r1->prog->loc);
//			print("\n");
//		}
	}
}
