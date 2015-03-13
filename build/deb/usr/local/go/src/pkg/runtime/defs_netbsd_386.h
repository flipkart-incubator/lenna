// Created by cgo -cdefs - DO NOT EDIT
// cgo -cdefs defs_netbsd.go defs_netbsd_386.go


enum {
	EINTR	= 0x4,
	EFAULT	= 0xe,

	PROT_NONE	= 0x0,
	PROT_READ	= 0x1,
	PROT_WRITE	= 0x2,
	PROT_EXEC	= 0x4,

	MAP_ANON	= 0x1000,
	MAP_PRIVATE	= 0x2,
	MAP_FIXED	= 0x10,

	MADV_FREE	= 0x6,

	SA_SIGINFO	= 0x40,
	SA_RESTART	= 0x2,
	SA_ONSTACK	= 0x1,

	SIGHUP		= 0x1,
	SIGINT		= 0x2,
	SIGQUIT		= 0x3,
	SIGILL		= 0x4,
	SIGTRAP		= 0x5,
	SIGABRT		= 0x6,
	SIGEMT		= 0x7,
	SIGFPE		= 0x8,
	SIGKILL		= 0x9,
	SIGBUS		= 0xa,
	SIGSEGV		= 0xb,
	SIGSYS		= 0xc,
	SIGPIPE		= 0xd,
	SIGALRM		= 0xe,
	SIGTERM		= 0xf,
	SIGURG		= 0x10,
	SIGSTOP		= 0x11,
	SIGTSTP		= 0x12,
	SIGCONT		= 0x13,
	SIGCHLD		= 0x14,
	SIGTTIN		= 0x15,
	SIGTTOU		= 0x16,
	SIGIO		= 0x17,
	SIGXCPU		= 0x18,
	SIGXFSZ		= 0x19,
	SIGVTALRM	= 0x1a,
	SIGPROF		= 0x1b,
	SIGWINCH	= 0x1c,
	SIGINFO		= 0x1d,
	SIGUSR1		= 0x1e,
	SIGUSR2		= 0x1f,

	FPE_INTDIV	= 0x1,
	FPE_INTOVF	= 0x2,
	FPE_FLTDIV	= 0x3,
	FPE_FLTOVF	= 0x4,
	FPE_FLTUND	= 0x5,
	FPE_FLTRES	= 0x6,
	FPE_FLTINV	= 0x7,
	FPE_FLTSUB	= 0x8,

	BUS_ADRALN	= 0x1,
	BUS_ADRERR	= 0x2,
	BUS_OBJERR	= 0x3,

	SEGV_MAPERR	= 0x1,
	SEGV_ACCERR	= 0x2,

	ITIMER_REAL	= 0x0,
	ITIMER_VIRTUAL	= 0x1,
	ITIMER_PROF	= 0x2,

	EV_ADD		= 0x1,
	EV_DELETE	= 0x2,
	EV_CLEAR	= 0x20,
	EV_RECEIPT	= 0,
	EV_ERROR	= 0x4000,
	EVFILT_READ	= 0x0,
	EVFILT_WRITE	= 0x1,
};

typedef struct Sigaltstack Sigaltstack;
typedef struct Sigset Sigset;
typedef struct Siginfo Siginfo;
typedef struct StackT StackT;
typedef struct Timespec Timespec;
typedef struct Timeval Timeval;
typedef struct Itimerval Itimerval;
typedef struct McontextT McontextT;
typedef struct UcontextT UcontextT;
typedef struct Kevent Kevent;

#pragma pack on

struct Sigaltstack {
	byte	*ss_sp;
	uint32	ss_size;
	int32	ss_flags;
};
struct Sigset {
	uint32	__bits[4];
};
struct Siginfo {
	int32	_signo;
	int32	_code;
	int32	_errno;
	byte	_reason[20];
};

struct StackT {
	byte	*ss_sp;
	uint32	ss_size;
	int32	ss_flags;
};

struct Timespec {
	int64	tv_sec;
	int32	tv_nsec;
};
struct Timeval {
	int64	tv_sec;
	int32	tv_usec;
};
struct Itimerval {
	Timeval	it_interval;
	Timeval	it_value;
};

struct McontextT {
	int32	__gregs[19];
	byte	__fpregs[644];
	int32	_mc_tlsbase;
};
struct UcontextT {
	uint32	uc_flags;
	UcontextT	*uc_link;
	Sigset	uc_sigmask;
	StackT	uc_stack;
	McontextT	uc_mcontext;
	int32	__uc_pad[4];
};

struct Kevent {
	uint32	ident;
	uint32	filter;
	uint32	flags;
	uint32	fflags;
	int64	data;
	int32	udata;
};


#pragma pack off
// Created by cgo -cdefs - DO NOT EDIT
// cgo -cdefs defs_netbsd.go defs_netbsd_386.go


enum {
	REG_GS		= 0x0,
	REG_FS		= 0x1,
	REG_ES		= 0x2,
	REG_DS		= 0x3,
	REG_EDI		= 0x4,
	REG_ESI		= 0x5,
	REG_EBP		= 0x6,
	REG_ESP		= 0x7,
	REG_EBX		= 0x8,
	REG_EDX		= 0x9,
	REG_ECX		= 0xa,
	REG_EAX		= 0xb,
	REG_TRAPNO	= 0xc,
	REG_ERR		= 0xd,
	REG_EIP		= 0xe,
	REG_CS		= 0xf,
	REG_EFL		= 0x10,
	REG_UESP	= 0x11,
	REG_SS		= 0x12,
};

