// Inferno libmach/6obj.c
// http://code.google.com/p/inferno-os/source/browse/utils/libmach/6obj.c
//
// 	Copyright © 1994-1999 Lucent Technologies Inc.
// 	Power PC support Copyright © 1995-2004 C H Forsyth (forsyth@terzarima.net).
// 	Portions Copyright © 1997-1999 Vita Nuova Limited.
// 	Portions Copyright © 2000-2007 Vita Nuova Holdings Limited (www.vitanuova.com).
// 	Revisions Copyright © 2000-2004 Lucent Technologies Inc. and others.
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

/*
 * 6obj.c - identify and parse an amd64 object file
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "../cmd/6l/6.out.h"
#include "obj.h"

typedef struct Addr	Addr;
struct Addr
{
	char	sym;
	char	flags;
	char gotype;
};
static	Addr	addr(Biobuf*);
static	char	type2char(int);
static	void	skip(Biobuf*, int);

int
_is6(char *t)
{
	uchar *s = (uchar*)t;

	return  s[0] == (ANAME&0xff)			/* also = ANAME */
		&& s[1] == ((ANAME>>8)&0xff)
		&& s[2] == D_FILE			/* type */
		&& s[3] == 1				/* sym */
		&& s[4] == '<';				/* name of file */
}

int
_read6(Biobuf *bp, Prog* p)
{
	int as, n, c;
	Addr a;

	as = BGETC(bp);		/* as(low) */
	if(as < 0)
		return 0;
	c = BGETC(bp);		/* as(high) */
	if(c < 0)
		return 0;
	as |= ((c & 0xff) << 8);
	p->kind = aNone;
	p->sig = 0;
	if(as == ANAME || as == ASIGNAME){
		if(as == ASIGNAME){
			Bread(bp, &p->sig, 4);
			p->sig = leswal(p->sig);
		}
		p->kind = aName;
		p->type = type2char(BGETC(bp));		/* type */
		p->sym = BGETC(bp);			/* sym */
		n = 0;
		for(;;) {
			as = BGETC(bp);
			if(as < 0)
				return 0;
			n++;
			if(as == 0)
				break;
		}
		p->id = malloc(n);
		if(p->id == 0)
			return 0;
		Bseek(bp, -n, 1);
		if(Bread(bp, p->id, n) != n)
			return 0;
		return 1;
	}
	if(as == ATEXT)
		p->kind = aText;
	if(as == AGLOBL)
		p->kind = aData;
	skip(bp, 4);		/* lineno(4) */
	a = addr(bp);
	addr(bp);
	if(!(a.flags & T_SYM))
		p->kind = aNone;
	p->sym = a.sym;
	return 1;
}

static Addr
addr(Biobuf *bp)
{
	Addr a;
	int t;
	int32 l;
	vlong off;

	off = 0;
	a.sym = -1;
	a.flags = BGETC(bp);			/* flags */
	a.gotype = 0;
	if(a.flags & T_INDEX)
		skip(bp, 2);
	if(a.flags & T_OFFSET){
		l = BGETLE4(bp);
		off = l;
		if(a.flags & T_64){
			l = BGETLE4(bp);
			off = ((vlong)l << 32) | (off & 0xFFFFFFFF);
		}
		if(off < 0)
			off = -(uvlong)off;
	}
	if(a.flags & T_SYM)
		a.sym = BGETC(bp);
	if(a.flags & T_FCONST)
		skip(bp, 8);
	else
	if(a.flags & T_SCONST)
		skip(bp, NSNAME);
	if(a.flags & T_TYPE) {
		t = BGETC(bp);
		if(a.sym > 0 && (t==D_PARAM || t==D_AUTO))
			_offset(a.sym, off);
	}
	if(a.flags & T_GOTYPE)
		a.gotype = BGETC(bp);
	return a;
}

static char
type2char(int t)
{
	switch(t){
	case D_EXTERN:		return 'U';
	case D_STATIC:		return 'b';
	case D_AUTO:		return 'a';
	case D_PARAM:		return 'p';
	default:		return UNKNOWN;
	}
}

static void
skip(Biobuf *bp, int n)
{
	while (n-- > 0)
		Bgetc(bp);
}
