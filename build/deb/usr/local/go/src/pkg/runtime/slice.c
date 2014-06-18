// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "type.h"
#include "typekind.h"
#include "malloc.h"
#include "race.h"
#include "../../cmd/ld/textflag.h"

enum
{
	debug = 0
};

static	void	makeslice1(SliceType*, intgo, intgo, Slice*);
static	void	growslice1(SliceType*, Slice, intgo, Slice *);
	void	runtime·copy(Slice to, Slice fm, uintptr width, intgo ret);

// see also unsafe·NewArray
// makeslice(typ *Type, len, cap int64) (ary []any);
void
runtime·makeslice(SliceType *t, int64 len, int64 cap, Slice ret)
{
	// NOTE: The len > MaxMem/elemsize check here is not strictly necessary,
	// but it produces a 'len out of range' error instead of a 'cap out of range' error
	// when someone does make([]T, bignumber). 'cap out of range' is true too,
	// but since the cap is only being supplied implicitly, saying len is clearer.
	// See issue 4085.
	if(len < 0 || (intgo)len != len || t->elem->size > 0 && len > MaxMem / t->elem->size)
		runtime·panicstring("makeslice: len out of range");

	if(cap < len || (intgo)cap != cap || t->elem->size > 0 && cap > MaxMem / t->elem->size)
		runtime·panicstring("makeslice: cap out of range");

	makeslice1(t, len, cap, &ret);

	if(debug) {
		runtime·printf("makeslice(%S, %D, %D); ret=",
			*t->string, len, cap);
		runtime·printslice(ret);
	}
}

// Dummy word to use as base pointer for make([]T, 0).
// Since you cannot take the address of such a slice,
// you can't tell that they all have the same base pointer.
uintptr runtime·zerobase;

static void
makeslice1(SliceType *t, intgo len, intgo cap, Slice *ret)
{
	ret->len = len;
	ret->cap = cap;
	ret->array = runtime·cnewarray(t->elem, cap);
}

// growslice(type *Type, x, []T, n int64) []T
void
runtime·growslice(SliceType *t, Slice old, int64 n, Slice ret)
{
	int64 cap;
	void *pc;

	if(n < 1)
		runtime·panicstring("growslice: invalid n");

	cap = old.cap + n;

	if((intgo)cap != cap || cap < old.cap || (t->elem->size > 0 && cap > MaxMem/t->elem->size))
		runtime·panicstring("growslice: cap out of range");

	if(raceenabled) {
		pc = runtime·getcallerpc(&t);
		runtime·racereadrangepc(old.array, old.len*t->elem->size, pc, runtime·growslice);
	}

	growslice1(t, old, cap, &ret);

	FLUSH(&ret);

	if(debug) {
		runtime·printf("growslice(%S,", *t->string);
		runtime·printslice(old);
		runtime·printf(", new cap=%D) =", cap);
		runtime·printslice(ret);
	}
}

static void
growslice1(SliceType *t, Slice x, intgo newcap, Slice *ret)
{
	intgo m;

	m = x.cap;
	
	// Using newcap directly for m+m < newcap handles
	// both the case where m == 0 and also the case where
	// m+m/4 wraps around, in which case the loop
	// below might never terminate.
	if(m+m < newcap)
		m = newcap;
	else {
		do {
			if(x.len < 1024)
				m += m;
			else
				m += m/4;
		} while(m < newcap);
	}
	makeslice1(t, x.len, m, ret);
	runtime·memmove(ret->array, x.array, ret->len * t->elem->size);
}

// copy(to any, fr any, wid uintptr) int
#pragma textflag NOSPLIT
void
runtime·copy(Slice to, Slice fm, uintptr width, intgo ret)
{
	void *pc;

	if(fm.len == 0 || to.len == 0 || width == 0) {
		ret = 0;
		goto out;
	}

	ret = fm.len;
	if(to.len < ret)
		ret = to.len;

	if(raceenabled) {
		pc = runtime·getcallerpc(&to);
		runtime·racewriterangepc(to.array, ret*width, pc, runtime·copy);
		runtime·racereadrangepc(fm.array, ret*width, pc, runtime·copy);
	}

	if(ret == 1 && width == 1) {	// common case worth about 2x to do here
		*to.array = *fm.array;	// known to be a byte pointer
	} else {
		runtime·memmove(to.array, fm.array, ret*width);
	}

out:
	FLUSH(&ret);

	if(debug) {
		runtime·prints("main·copy: to=");
		runtime·printslice(to);
		runtime·prints("; fm=");
		runtime·printslice(fm);
		runtime·prints("; width=");
		runtime·printint(width);
		runtime·prints("; ret=");
		runtime·printint(ret);
		runtime·prints("\n");
	}
}

#pragma textflag NOSPLIT
void
runtime·slicestringcopy(Slice to, String fm, intgo ret)
{
	void *pc;

	if(fm.len == 0 || to.len == 0) {
		ret = 0;
		goto out;
	}

	ret = fm.len;
	if(to.len < ret)
		ret = to.len;

	if(raceenabled) {
		pc = runtime·getcallerpc(&to);
		runtime·racewriterangepc(to.array, ret, pc, runtime·slicestringcopy);
	}

	runtime·memmove(to.array, fm.str, ret);

out:
	FLUSH(&ret);
}

void
runtime·printslice(Slice a)
{
	runtime·prints("[");
	runtime·printint(a.len);
	runtime·prints("/");
	runtime·printint(a.cap);
	runtime·prints("]");
	runtime·printpointer(a.array);
}
