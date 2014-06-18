// Copyright 2011 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/*
Stack layout parameters.
Included both by runtime (compiled via 6c) and linkers (compiled via gcc).

The per-goroutine g->stackguard is set to point StackGuard bytes
above the bottom of the stack.  Each function compares its stack
pointer against g->stackguard to check for overflow.  To cut one
instruction from the check sequence for functions with tiny frames,
the stack is allowed to protrude StackSmall bytes below the stack
guard.  Functions with large frames don't bother with the check and
always call morestack.  The sequences are (for amd64, others are
similar):
 
	guard = g->stackguard
	frame = function's stack frame size
	argsize = size of function arguments (call + return)

	stack frame size <= StackSmall:
		CMPQ guard, SP
		JHI 3(PC)
		MOVQ m->morearg, $(argsize << 32)
		CALL morestack(SB)

	stack frame size > StackSmall but < StackBig
		LEAQ (frame-StackSmall)(SP), R0
		CMPQ guard, R0
		JHI 3(PC)
		MOVQ m->morearg, $(argsize << 32)
		CALL morestack(SB)

	stack frame size >= StackBig:
		MOVQ m->morearg, $((argsize << 32) | frame)
		CALL morestack(SB)

The bottom StackGuard - StackSmall bytes are important: there has
to be enough room to execute functions that refuse to check for
stack overflow, either because they need to be adjacent to the
actual caller's frame (deferproc) or because they handle the imminent
stack overflow (morestack).

For example, deferproc might call malloc, which does one of the
above checks (without allocating a full frame), which might trigger
a call to morestack.  This sequence needs to fit in the bottom
section of the stack.  On amd64, morestack's frame is 40 bytes, and
deferproc's frame is 56 bytes.  That fits well within the
StackGuard - StackSmall = 128 bytes at the bottom.  
The linkers explore all possible call traces involving non-splitting
functions to make sure that this limit cannot be violated.
 */

enum {
	// StackSystem is a number of additional bytes to add
	// to each stack below the usual guard area for OS-specific
	// purposes like signal handling. Used on Windows and on
	// Plan 9 because they do not use a separate stack.
#ifdef GOOS_windows
	StackSystem = 512 * sizeof(uintptr),
#else
#ifdef GOOS_plan9
	// The size of the note handler frame varies among architectures,
	// but 512 bytes should be enough for every implementation.
	StackSystem = 512,
#else
	StackSystem = 0,
#endif	// Plan 9
#endif	// Windows

	// The amount of extra stack to allocate beyond the size
	// needed for the single frame that triggered the split.
	StackExtra = 2048,

	// The minimum stack segment size to allocate.
	// If the amount needed for the splitting frame + StackExtra
	// is less than this number, the stack will have this size instead.
	StackMin = 8192,
	FixedStack = StackMin + StackSystem,

	// Functions that need frames bigger than this use an extra
	// instruction to do the stack split check, to avoid overflow
	// in case SP - framesize wraps below zero.
	// This value can be no bigger than the size of the unmapped
	// space at zero.
	StackBig = 4096,

	// The stack guard is a pointer this many bytes above the
	// bottom of the stack.
	StackGuard = 256 + StackSystem,

	// After a stack split check the SP is allowed to be this
	// many bytes below the stack guard.  This saves an instruction
	// in the checking sequence for tiny frames.
	StackSmall = 128,

	// The maximum number of bytes that a chain of NOSPLIT
	// functions can use.
	StackLimit = StackGuard - StackSystem - StackSmall,
	
	// The assumed size of the top-of-stack data block.
	// The actual size can be smaller than this but cannot be larger.
	// Checked in proc.c's runtime.malg.
	StackTop = 96,
};

// Goroutine preemption request.
// Stored into g->stackguard0 to cause split stack check failure.
// Must be greater than any real sp.
// 0xfffffade in hex.
#define StackPreempt ((uint64)-1314)
