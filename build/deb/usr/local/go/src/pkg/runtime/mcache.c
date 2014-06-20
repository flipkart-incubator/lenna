// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// Per-P malloc cache for small objects.
//
// See malloc.h for an overview.

#include "runtime.h"
#include "arch_GOARCH.h"
#include "malloc.h"

extern volatile intgo runtime·MemProfileRate;

// dummy MSpan that contains no free objects.
static MSpan emptymspan;

MCache*
runtime·allocmcache(void)
{
	intgo rate;
	MCache *c;
	int32 i;

	runtime·lock(&runtime·mheap);
	c = runtime·FixAlloc_Alloc(&runtime·mheap.cachealloc);
	runtime·unlock(&runtime·mheap);
	runtime·memclr((byte*)c, sizeof(*c));
	for(i = 0; i < NumSizeClasses; i++)
		c->alloc[i] = &emptymspan;

	// Set first allocation sample size.
	rate = runtime·MemProfileRate;
	if(rate > 0x3fffffff)	// make 2*rate not overflow
		rate = 0x3fffffff;
	if(rate != 0)
		c->next_sample = runtime·fastrand1() % (2*rate);

	return c;
}

void
runtime·freemcache(MCache *c)
{
	runtime·MCache_ReleaseAll(c);
	runtime·lock(&runtime·mheap);
	runtime·purgecachedstats(c);
	runtime·FixAlloc_Free(&runtime·mheap.cachealloc, c);
	runtime·unlock(&runtime·mheap);
}

// Gets a span that has a free object in it and assigns it
// to be the cached span for the given sizeclass.  Returns this span.
MSpan*
runtime·MCache_Refill(MCache *c, int32 sizeclass)
{
	MCacheList *l;
	MSpan *s;

	m->locks++;
	// Return the current cached span to the central lists.
	s = c->alloc[sizeclass];
	if(s->freelist != nil)
		runtime·throw("refill on a nonempty span");
	if(s != &emptymspan)
		runtime·MCentral_UncacheSpan(&runtime·mheap.central[sizeclass], s);

	// Push any explicitly freed objects to the central lists.
	// Not required, but it seems like a good time to do it.
	l = &c->free[sizeclass];
	if(l->nlist > 0) {
		runtime·MCentral_FreeList(&runtime·mheap.central[sizeclass], l->list);
		l->list = nil;
		l->nlist = 0;
	}

	// Get a new cached span from the central lists.
	s = runtime·MCentral_CacheSpan(&runtime·mheap.central[sizeclass]);
	if(s == nil)
		runtime·throw("out of memory");
	if(s->freelist == nil) {
		runtime·printf("%d %d\n", s->ref, (int32)((s->npages << PageShift) / s->elemsize));
		runtime·throw("empty span");
	}
	c->alloc[sizeclass] = s;
	m->locks--;
	return s;
}

void
runtime·MCache_Free(MCache *c, MLink *p, int32 sizeclass, uintptr size)
{
	MCacheList *l;

	// Put on free list.
	l = &c->free[sizeclass];
	p->next = l->list;
	l->list = p;
	l->nlist++;

	// We transfer a span at a time from MCentral to MCache,
	// so we'll do the same in the other direction.
	if(l->nlist >= (runtime·class_to_allocnpages[sizeclass]<<PageShift)/size) {
		runtime·MCentral_FreeList(&runtime·mheap.central[sizeclass], l->list);
		l->list = nil;
		l->nlist = 0;
	}
}

void
runtime·MCache_ReleaseAll(MCache *c)
{
	int32 i;
	MSpan *s;
	MCacheList *l;

	for(i=0; i<NumSizeClasses; i++) {
		s = c->alloc[i];
		if(s != &emptymspan) {
			runtime·MCentral_UncacheSpan(&runtime·mheap.central[i], s);
			c->alloc[i] = &emptymspan;
		}
		l = &c->free[i];
		if(l->nlist > 0) {
			runtime·MCentral_FreeList(&runtime·mheap.central[i], l->list);
			l->list = nil;
			l->nlist = 0;
		}
	}
}
