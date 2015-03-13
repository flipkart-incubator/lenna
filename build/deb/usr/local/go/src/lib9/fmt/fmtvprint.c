/*
 * The authors of this software are Rob Pike and Ken Thompson,
 * with contributions from Mike Burrows and Sean Dorward.
 *
 *     Copyright (c) 2002-2006 by Lucent Technologies.
 *     Portions Copyright (c) 2004 Google Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES 
 * NOR GOOGLE INC MAKE ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING 
 * THE MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#include <u.h>
#include <libc.h>
#include "fmtdef.h"


/*
 * Format a string into the output buffer.
 * Designed for formats which themselves call fmt.
 * Flags, precision and width are preserved.
 */
int
fmtvprint(Fmt *f, char *fmt, va_list args)
{
	va_list va;
	int n, w, p;
	unsigned long fl;

	w = f->width;
	p = f->prec;
	fl = f->flags;
	VA_COPY(va, f->args);
	VA_END(f->args);
	VA_COPY(f->args, args);
	n = dofmt(f, fmt);
	VA_END(f->args);
	VA_COPY(f->args, va);
	VA_END(va);
	f->width = w;
	f->prec = p;
	f->flags = fl;
	if(n >= 0)
		return 0;
	return n;
}
