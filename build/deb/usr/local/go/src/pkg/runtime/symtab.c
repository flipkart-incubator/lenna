// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Runtime symbol table parsing.
// See http://golang.org/s/go12symtab for an overview.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "arch_GOARCH.h"
#include "malloc.h"
#include "funcdata.h"

typedef struct Ftab Ftab;
struct Ftab
{
	uintptr	entry;
	uintptr	funcoff;
};

extern byte pclntab[];

static Ftab *ftab;
static uintptr nftab;
static uint32 *filetab;
static uint32 nfiletab;

static String end = { (uint8*)"end", 3 };

void
runtime·symtabinit(void)
{
	int32 i, j;
	Func *f1, *f2;
	
	// See golang.org/s/go12symtab for header: 0xfffffffb,
	// two zero bytes, a byte giving the PC quantum,
	// and a byte giving the pointer width in bytes.
	if(*(uint32*)pclntab != 0xfffffffb || pclntab[4] != 0 || pclntab[5] != 0 || pclntab[6] != PCQuantum || pclntab[7] != sizeof(void*)) {
		runtime·printf("runtime: function symbol table header: 0x%x 0x%x\n", *(uint32*)pclntab, *(uint32*)(pclntab+4));
		runtime·throw("invalid function symbol table\n");
	}

	nftab = *(uintptr*)(pclntab+8);
	ftab = (Ftab*)(pclntab+8+sizeof(void*));
	for(i=0; i<nftab; i++) {
		// NOTE: ftab[nftab].entry is legal; it is the address beyond the final function.
		if(ftab[i].entry > ftab[i+1].entry) {
			f1 = (Func*)(pclntab + ftab[i].funcoff);
			f2 = (Func*)(pclntab + ftab[i+1].funcoff);
			runtime·printf("function symbol table not sorted by program counter: %p %s > %p %s", ftab[i].entry, runtime·funcname(f1), ftab[i+1].entry, i+1 == nftab ? "end" : runtime·funcname(f2));
			for(j=0; j<=i; j++)
				runtime·printf("\t%p %s\n", ftab[j].entry, runtime·funcname((Func*)(pclntab + ftab[j].funcoff)));
			runtime·throw("invalid runtime symbol table");
		}
	}
	
	filetab = (uint32*)(pclntab + *(uint32*)&ftab[nftab].funcoff);
	nfiletab = filetab[0];
}

static uint32
readvarint(byte **pp)
{
	byte *p;
	uint32 v;
	int32 shift;
	
	v = 0;
	p = *pp;
	for(shift = 0;; shift += 7) {
		v |= (*p & 0x7F) << shift;
		if(!(*p++ & 0x80))
			break;
	}
	*pp = p;
	return v;
}

void*
runtime·funcdata(Func *f, int32 i)
{
	byte *p;

	if(i < 0 || i >= f->nfuncdata)
		return nil;
	p = (byte*)&f->nfuncdata + 4 + f->npcdata*4;
	if(sizeof(void*) == 8 && ((uintptr)p & 4))
		p += 4;
	return ((void**)p)[i];
}

static bool
step(byte **pp, uintptr *pc, int32 *value, bool first)
{
	uint32 uvdelta, pcdelta;
	int32 vdelta;

	uvdelta = readvarint(pp);
	if(uvdelta == 0 && !first)
		return 0;
	if(uvdelta&1)
		uvdelta = ~(uvdelta>>1);
	else
		uvdelta >>= 1;
	vdelta = (int32)uvdelta;
	pcdelta = readvarint(pp) * PCQuantum;
	*value += vdelta;
	*pc += pcdelta;
	return 1;
}

// Return associated data value for targetpc in func f.
// (Source file is f->src.)
static int32
pcvalue(Func *f, int32 off, uintptr targetpc, bool strict)
{
	byte *p;
	uintptr pc;
	int32 value;

	enum {
		debug = 0
	};

	// The table is a delta-encoded sequence of (value, pc) pairs.
	// Each pair states the given value is in effect up to pc.
	// The value deltas are signed, zig-zag encoded.
	// The pc deltas are unsigned.
	// The starting value is -1, the starting pc is the function entry.
	// The table ends at a value delta of 0 except in the first pair.
	if(off == 0)
		return -1;
	p = pclntab + off;
	pc = f->entry;
	value = -1;

	if(debug && !runtime·panicking)
		runtime·printf("pcvalue start f=%s [%p] pc=%p targetpc=%p value=%d tab=%p\n",
			runtime·funcname(f), f, pc, targetpc, value, p);
	
	while(step(&p, &pc, &value, pc == f->entry)) {
		if(debug)
			runtime·printf("\tvalue=%d until pc=%p\n", value, pc);
		if(targetpc < pc)
			return value;
	}
	
	// If there was a table, it should have covered all program counters.
	// If not, something is wrong.
	if(runtime·panicking || !strict)
		return -1;
	runtime·printf("runtime: invalid pc-encoded table f=%s pc=%p targetpc=%p tab=%p\n",
		runtime·funcname(f), pc, targetpc, p);
	p = (byte*)f + off;
	pc = f->entry;
	value = -1;
	
	while(step(&p, &pc, &value, pc == f->entry))
		runtime·printf("\tvalue=%d until pc=%p\n", value, pc);
	
	runtime·throw("invalid runtime symbol table");
	return -1;
}

static String unknown = { (uint8*)"?", 1 };

int8*
runtime·funcname(Func *f)
{
	if(f == nil || f->nameoff == 0)
		return nil;
	return (int8*)(pclntab + f->nameoff);
}

static int32
funcline(Func *f, uintptr targetpc, String *file, bool strict)
{
	int32 line;
	int32 fileno;

	*file = unknown;
	fileno = pcvalue(f, f->pcfile, targetpc, strict);
	line = pcvalue(f, f->pcln, targetpc, strict);
	if(fileno == -1 || line == -1 || fileno >= nfiletab) {
		// runtime·printf("looking for %p in %S got file=%d line=%d\n", targetpc, *f->name, fileno, line);
		return 0;
	}
	*file = runtime·gostringnocopy(pclntab + filetab[fileno]);
	return line;
}

int32
runtime·funcline(Func *f, uintptr targetpc, String *file)
{
	return funcline(f, targetpc, file, true);
}

int32
runtime·funcspdelta(Func *f, uintptr targetpc)
{
	int32 x;
	
	x = pcvalue(f, f->pcsp, targetpc, true);
	if(x&(sizeof(void*)-1))
		runtime·printf("invalid spdelta %d %d\n", f->pcsp, x);
	return x;
}

int32
runtime·pcdatavalue(Func *f, int32 table, uintptr targetpc)
{
	if(table < 0 || table >= f->npcdata)
		return -1;
	return pcvalue(f, (&f->nfuncdata)[1+table], targetpc, true);
}

int32
runtime·funcarglen(Func *f, uintptr targetpc)
{
	if(targetpc == f->entry)
		return 0;
	return runtime·pcdatavalue(f, PCDATA_ArgSize, targetpc-PCQuantum);
}

void
runtime·funcline_go(Func *f, uintptr targetpc, String retfile, intgo retline)
{
	// Pass strict=false here, because anyone can call this function,
	// and they might just be wrong about targetpc belonging to f.
	retline = funcline(f, targetpc, &retfile, false);
	FLUSH(&retline);
}

void
runtime·funcname_go(Func *f, String ret)
{
	ret = runtime·gostringnocopy((uint8*)runtime·funcname(f));
	FLUSH(&ret);
}

void
runtime·funcentry_go(Func *f, uintptr ret)
{
	ret = f->entry;
	FLUSH(&ret);
}

Func*
runtime·findfunc(uintptr addr)
{
	Ftab *f;
	int32 nf, n;

	if(nftab == 0)
		return nil;
	if(addr < ftab[0].entry || addr >= ftab[nftab].entry)
		return nil;

	// binary search to find func with entry <= addr.
	f = ftab;
	nf = nftab;
	while(nf > 0) {
		n = nf/2;
		if(f[n].entry <= addr && addr < f[n+1].entry)
			return (Func*)(pclntab + f[n].funcoff);
		else if(addr < f[n].entry)
			nf = n;
		else {
			f += n+1;
			nf -= n+1;
		}
	}

	// can't get here -- we already checked above
	// that the address was in the table bounds.
	// this can only happen if the table isn't sorted
	// by address or if the binary search above is buggy.
	runtime·prints("findfunc unreachable\n");
	return nil;
}

static bool
hasprefix(String s, int8 *p)
{
	int32 i;

	for(i=0; i<s.len; i++) {
		if(p[i] == 0)
			return 1;
		if(p[i] != s.str[i])
			return 0;
	}
	return p[i] == 0;
}

static bool
contains(String s, int8 *p)
{
	int32 i;

	if(p[0] == 0)
		return 1;
	for(i=0; i<s.len; i++) {
		if(s.str[i] != p[0])
			continue;
		if(hasprefix((String){s.str + i, s.len - i}, p))
			return 1;
	}
	return 0;
}

bool
runtime·showframe(Func *f, G *gp)
{
	static int32 traceback = -1;
	String name;

	if(m->throwing > 0 && gp != nil && (gp == m->curg || gp == m->caughtsig))
		return 1;
	if(traceback < 0)
		traceback = runtime·gotraceback(nil);
	name = runtime·gostringnocopy((uint8*)runtime·funcname(f));

	// Special case: always show runtime.panic frame, so that we can
	// see where a panic started in the middle of a stack trace.
	// See golang.org/issue/5832.
	if(name.len == 7+1+5 && hasprefix(name, "runtime.panic"))
		return 1;

	return traceback > 1 || f != nil && contains(name, ".") && !hasprefix(name, "runtime.");
}
