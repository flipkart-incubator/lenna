// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"

void
runtime·dumpregs(Context *r)
{
	runtime·printf("eax     %x\n", r->Eax);
	runtime·printf("ebx     %x\n", r->Ebx);
	runtime·printf("ecx     %x\n", r->Ecx);
	runtime·printf("edx     %x\n", r->Edx);
	runtime·printf("edi     %x\n", r->Edi);
	runtime·printf("esi     %x\n", r->Esi);
	runtime·printf("ebp     %x\n", r->Ebp);
	runtime·printf("esp     %x\n", r->Esp);
	runtime·printf("eip     %x\n", r->Eip);
	runtime·printf("eflags  %x\n", r->EFlags);
	runtime·printf("cs      %x\n", r->SegCs);
	runtime·printf("fs      %x\n", r->SegFs);
	runtime·printf("gs      %x\n", r->SegGs);
}

#define DBG_PRINTEXCEPTION_C 0x40010006

// Called by sigtramp from Windows VEH handler.
// Return value signals whether the exception has been handled (-1)
// or should be made available to other handlers in the chain (0).
uint32
runtime·sighandler(ExceptionRecord *info, Context *r, G *gp)
{
	bool crash;
	uintptr *sp;
	extern byte text[], etext[];

	if(info->ExceptionCode == DBG_PRINTEXCEPTION_C) {
		// This exception is intended to be caught by debuggers.
		// There is a not-very-informational message like
		// "Invalid parameter passed to C runtime function"
		// sitting at info->ExceptionInformation[0] (a wchar_t*),
		// with length info->ExceptionInformation[1].
		// The default behavior is to ignore this exception,
		// but somehow returning 0 here (meaning keep going)
		// makes the program crash instead. Maybe Windows has no
		// other handler registered? In any event, ignore it.
		return -1;
	}

	// Only handle exception if executing instructions in Go binary
	// (not Windows library code). 
	if(r->Eip < (uint32)text || (uint32)etext < r->Eip)
		return 0;

	switch(info->ExceptionCode) {
	case EXCEPTION_BREAKPOINT:
		// It is unclear whether this is needed, unclear whether it
		// would work, and unclear how to test it. Leave out for now.
		// This only handles breakpoint instructions written in the
		// assembly sources, not breakpoints set by a debugger, and
		// there are very few of the former.
		//
		// r->Eip--;	// because 8l generates 2 bytes for INT3
		// return 0;
		break;
	}

	if(gp != nil && runtime·issigpanic(info->ExceptionCode)) {
		// Make it look like a call to the signal func.
		// Have to pass arguments out of band since
		// augmenting the stack frame would break
		// the unwinding code.
		gp->sig = info->ExceptionCode;
		gp->sigcode0 = info->ExceptionInformation[0];
		gp->sigcode1 = info->ExceptionInformation[1];
		gp->sigpc = r->Eip;

		// Only push runtime·sigpanic if r->eip != 0.
		// If r->eip == 0, probably panicked because of a
		// call to a nil func.  Not pushing that onto sp will
		// make the trace look like a call to runtime·sigpanic instead.
		// (Otherwise the trace will end at runtime·sigpanic and we
		// won't get to see who faulted.)
		if(r->Eip != 0) {
			sp = (uintptr*)r->Esp;
			*--sp = r->Eip;
			r->Esp = (uintptr)sp;
		}
		r->Eip = (uintptr)runtime·sigpanic;
		return -1;
	}

	if(runtime·panicking)	// traceback already printed
		runtime·exit(2);
	runtime·panicking = 1;

	runtime·printf("Exception %x %p %p %p\n", info->ExceptionCode,
		info->ExceptionInformation[0], info->ExceptionInformation[1], r->Eip);

	runtime·printf("PC=%x\n", r->Eip);
	if(m->lockedg != nil && m->ncgo > 0 && gp == m->g0) {
		runtime·printf("signal arrived during cgo execution\n");
		gp = m->lockedg;
	}
	runtime·printf("\n");

	if(runtime·gotraceback(&crash)){
		runtime·traceback(r->Eip, r->Esp, 0, gp);
		runtime·tracebackothers(gp);
		runtime·dumpregs(r);
	}
	
	if(crash)
		runtime·crash();

	runtime·exit(2);
	return -1; // not reached
}

void
runtime·sigenable(uint32 sig)
{
	USED(sig);
}

void
runtime·sigdisable(uint32 sig)
{
	USED(sig);
}

void
runtime·dosigprof(Context *r, G *gp, M *mp)
{
	runtime·sigprof((uint8*)r->Eip, (uint8*)r->Esp, nil, gp, mp);
}
