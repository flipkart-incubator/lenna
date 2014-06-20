// Copyright 2010 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "runtime.h"
#include "defs_GOOS_GOARCH.h"
#include "os_GOOS.h"
#include "arch_GOARCH.h"
#include "../../cmd/ld/textflag.h"
#include "stack.h"

int8 *goos = "nacl";
extern SigTab runtime·sigtab[];

void runtime·sigtramp(void);

// Called to initialize a new m (including the bootstrap m).
// Called on the parent thread (main thread in case of bootstrap), can allocate memory.
void
runtime·mpreinit(M *mp)
{
	mp->gsignal = runtime·malg(32*1024);	// OS X wants >=8K, Linux >=2K
}

// Called to initialize a new m (including the bootstrap m).
// Called on the new thread, can not allocate memory.
void
runtime·minit(void)
{
	int32 ret;

	// Initialize signal handling
	ret = runtime·nacl_exception_stack((byte*)m->gsignal->stackguard - StackGuard, 32*1024);
	if(ret < 0)
		runtime·printf("runtime: nacl_exception_stack: error %d\n", -ret);

	ret = runtime·nacl_exception_handler(runtime·sigtramp, nil);
	if(ret < 0)
		runtime·printf("runtime: nacl_exception_handler: error %d\n", -ret);
}

// Called from dropm to undo the effect of an minit.
void
runtime·unminit(void)
{
}

int8 runtime·sigtrampf[] = "runtime: signal at PC=%X AX=%X CX=%X DX=%X BX=%X DI=%X R15=%X *SP=%X\n";
int8 runtime·sigtrampp[] = "runtime: sigtramp";

extern byte runtime·tls0[];

void
runtime·osinit(void)
{
	runtime·ncpu = 1;
	m->procid = 2;
//runtime·nacl_exception_handler(runtime·sigtramp, nil);
}

void
runtime·crash(void)
{
	*(int32*)0 = 0;
}

void
runtime·get_random_data(byte **rnd, int32 *rnd_len)
{
	*rnd = nil;
	*rnd_len = 0;
}

void
runtime·goenvs(void)
{
	runtime·goenvs_unix();
}

void
runtime·initsig(void)
{
}

#pragma textflag NOSPLIT
void
runtime·usleep(uint32 us)
{
	Timespec ts;
	
	ts.tv_sec = us/1000000;
	ts.tv_nsec = (us%1000000)*1000;
	runtime·nacl_nanosleep(&ts, nil);
}

void runtime·mstart_nacl(void);

void
runtime·newosproc(M *mp, void *stk)
{
	int32 ret;
	void **tls;

	tls = (void**)mp->tls;
	tls[0] = mp->g0;
	tls[1] = mp;
	ret = runtime·nacl_thread_create(runtime·mstart_nacl, stk, tls+2, 0);
	if(ret < 0) {
		runtime·printf("nacl_thread_create: error %d\n", -ret);
		runtime·throw("newosproc");
	}
}

uintptr
runtime·semacreate(void)
{
	int32 mu, cond;
	
	mu = runtime·nacl_mutex_create(0);
	if(mu < 0) {
		runtime·printf("nacl_mutex_create: error %d\n", -mu);
		runtime·throw("semacreate");
	}
	cond = runtime·nacl_cond_create(0);
	if(cond < 0) {
		runtime·printf("nacl_cond_create: error %d\n", -cond);
		runtime·throw("semacreate");
	}
	m->waitsemalock = mu;
	return cond; // assigned to m->waitsema
}

#pragma textflag NOSPLIT
int32
runtime·semasleep(int64 ns)
{
	int32 ret;
	
	ret = runtime·nacl_mutex_lock(m->waitsemalock);
	if(ret < 0) {
		//runtime·printf("nacl_mutex_lock: error %d\n", -ret);
		runtime·throw("semasleep");
	}
	if(m->waitsemacount > 0) {
		m->waitsemacount = 0;
		runtime·nacl_mutex_unlock(m->waitsemalock);
		return 0;
	}

	while(m->waitsemacount == 0) {
		if(ns < 0) {
			ret = runtime·nacl_cond_wait(m->waitsema, m->waitsemalock);
			if(ret < 0) {
				//runtime·printf("nacl_cond_wait: error %d\n", -ret);
				runtime·throw("semasleep");
			}
		} else {
			Timespec ts;
			
			ns += runtime·nanotime();
			ts.tv_sec = runtime·timediv(ns, 1000000000, (int32*)&ts.tv_nsec);
			ret = runtime·nacl_cond_timed_wait_abs(m->waitsema, m->waitsemalock, &ts);
			if(ret == -ETIMEDOUT) {
				runtime·nacl_mutex_unlock(m->waitsemalock);
				return -1;
			}
			if(ret < 0) {
				//runtime·printf("nacl_cond_timed_wait_abs: error %d\n", -ret);
				runtime·throw("semasleep");
			}
		}
	}
			
	m->waitsemacount = 0;
	runtime·nacl_mutex_unlock(m->waitsemalock);
	return 0;
}

void
runtime·semawakeup(M *mp)
{
	int32 ret;
	
	ret = runtime·nacl_mutex_lock(mp->waitsemalock);
	if(ret < 0) {
		//runtime·printf("nacl_mutex_lock: error %d\n", -ret);
		runtime·throw("semawakeup");
	}
	if(mp->waitsemacount != 0) {
		//runtime·printf("semawakeup: double wakeup\n");
		runtime·throw("semawakeup");
	}
	mp->waitsemacount = 1;
	runtime·nacl_cond_signal(mp->waitsema);
	runtime·nacl_mutex_unlock(mp->waitsemalock);
}

void
os·sigpipe(void)
{
	runtime·throw("too many writes on closed pipe");
}

uintptr
runtime·memlimit(void)
{
	runtime·printf("memlimit\n");
	return 0;
}

#pragma dataflag NOPTR
static int8 badsignal[] = "runtime: signal received on thread not created by Go.\n";

// This runs on a foreign stack, without an m or a g.  No stack split.
#pragma textflag NOSPLIT
void
runtime·badsignal2(void)
{
	runtime·write(2, badsignal, sizeof badsignal - 1);
	runtime·exit(2);
}

void	runtime·madvise(byte*, uintptr, int32) { }
void runtime·munmap(byte*, uintptr) {}

void
runtime·resetcpuprofiler(int32 hz)
{
	USED(hz);
}

void
runtime·sigdisable(uint32)
{
}

void
runtime·sigenable(uint32)
{
}

void
runtime·closeonexec(int32)
{
}

void
runtime·sigpanic(void)
{
	if(!runtime·canpanic(g))
		runtime·throw("unexpected signal during runtime execution");

	// Native Client only invokes the exception handler for memory faults.
	g->sig = SIGSEGV;
	if(g->sigpc == 0)
		runtime·panicstring("call of nil func value");
	runtime·panicstring("invalid memory address or nil pointer dereference");
}

uint32 runtime·writelock; // test-and-set spin lock for runtime.write

/*
An attempt at IRT. Doesn't work. See end of sys_nacl_amd64.s.

void (*runtime·nacl_irt_query)(void);

int8 runtime·nacl_irt_basic_v0_1_str[] = "nacl-irt-basic-0.1";
void *runtime·nacl_irt_basic_v0_1[6]; // exit, gettod, clock, nanosleep, sched_yield, sysconf
int32 runtime·nacl_irt_basic_v0_1_size = sizeof(runtime·nacl_irt_basic_v0_1);

int8 runtime·nacl_irt_memory_v0_3_str[] = "nacl-irt-memory-0.3";
void *runtime·nacl_irt_memory_v0_3[3]; // mmap, munmap, mprotect
int32 runtime·nacl_irt_memory_v0_3_size = sizeof(runtime·nacl_irt_memory_v0_3);

int8 runtime·nacl_irt_thread_v0_1_str[] = "nacl-irt-thread-0.1";
void *runtime·nacl_irt_thread_v0_1[3]; // thread_create, thread_exit, thread_nice
int32 runtime·nacl_irt_thread_v0_1_size = sizeof(runtime·nacl_irt_thread_v0_1);
*/