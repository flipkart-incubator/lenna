// Derived from Inferno utils/6l/obj.c and utils/6l/span.c
// http://code.google.com/p/inferno-os/source/browse/utils/6l/obj.c
// http://code.google.com/p/inferno-os/source/browse/utils/6l/span.c
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

#include	"l.h"
#include	"lib.h"
#include	"../ld/elf.h"
#include	"../../pkg/runtime/stack.h"
#include	"../../pkg/runtime/funcdata.h"

#include	<ar.h>

enum
{
	// Whether to assume that the external linker is "gold"
	// (http://sourceware.org/ml/binutils/2008-03/msg00162.html).
	AssumeGoldLinker = 0,
};

int iconv(Fmt*);

char	symname[]	= SYMDEF;
char	pkgname[]	= "__.PKGDEF";
char**	libdir;
int	nlibdir = 0;
static int	maxlibdir = 0;
static int	cout = -1;

// symbol version, incremented each time a file is loaded.
// version==1 is reserved for savehist.
enum
{
	HistVersion = 1,
};
int	version = HistVersion;

// Set if we see an object compiled by the host compiler that is not
// from a package that is known to support internal linking mode.
static int	externalobj = 0;

static	void	hostlinksetup(void);

char*	goroot;
char*	goarch;
char*	goos;
char*	theline;

void
Lflag(char *arg)
{
	char **p;

	if(nlibdir >= maxlibdir) {
		if (maxlibdir == 0)
			maxlibdir = 8;
		else
			maxlibdir *= 2;
		p = erealloc(libdir, maxlibdir * sizeof(*p));
		libdir = p;
	}
	libdir[nlibdir++] = arg;
}

void
libinit(void)
{
	char *suffix, *suffixsep;

	fmtinstall('i', iconv);
	fmtinstall('Y', Yconv);
	fmtinstall('Z', Zconv);
	mywhatsys();	// get goroot, goarch, goos
	if(strcmp(goarch, thestring) != 0)
		print("goarch is not known: %s\n", goarch);

	// add goroot to the end of the libdir list.
	suffix = "";
	suffixsep = "";
	if(flag_installsuffix != nil) {
		suffixsep = "_";
		suffix = flag_installsuffix;
	} else if(flag_race) {
		suffixsep = "_";
		suffix = "race";
	}
	Lflag(smprint("%s/pkg/%s_%s%s%s", goroot, goos, goarch, suffixsep, suffix));

	// Unix doesn't like it when we write to a running (or, sometimes,
	// recently run) binary, so remove the output file before writing it.
	// On Windows 7, remove() can force the following create() to fail.
#ifndef _WIN32
	remove(outfile);
#endif
	cout = create(outfile, 1, 0775);
	if(cout < 0) {
		diag("cannot create %s: %r", outfile);
		errorexit();
	}

	if(INITENTRY == nil) {
		INITENTRY = mal(strlen(goarch)+strlen(goos)+20);
		if(!flag_shared) {
			sprint(INITENTRY, "_rt0_%s_%s", goarch, goos);
		} else {
			sprint(INITENTRY, "_rt0_%s_%s_lib", goarch, goos);
		}
	}
	lookup(INITENTRY, 0)->type = SXREF;
}

void
errorexit(void)
{
	if(nerrors) {
		if(cout >= 0)
			remove(outfile);
		exits("error");
	}
	exits(0);
}

void
addlib(char *src, char *obj)
{
	char name[1024], pname[1024], comp[256], *p;
	int i, search;

	if(histfrogp <= 0)
		return;

	search = 0;
	if(histfrog[0]->name[1] == '/') {
		sprint(name, "");
		i = 1;
	} else
	if(isalpha((uchar)histfrog[0]->name[1]) && histfrog[0]->name[2] == ':') {
		strcpy(name, histfrog[0]->name+1);
		i = 1;
	} else
	if(histfrog[0]->name[1] == '.') {
		sprint(name, ".");
		i = 0;
	} else {
		sprint(name, "");
		i = 0;
		search = 1;
	}

	for(; i<histfrogp; i++) {
		snprint(comp, sizeof comp, "%s", histfrog[i]->name+1);
		for(;;) {
			p = strstr(comp, "$O");
			if(p == 0)
				break;
			memmove(p+1, p+2, strlen(p+2)+1);
			p[0] = thechar;
		}
		for(;;) {
			p = strstr(comp, "$M");
			if(p == 0)
				break;
			if(strlen(comp)+strlen(thestring)-2+1 >= sizeof comp) {
				diag("library component too long");
				return;
			}
			memmove(p+strlen(thestring), p+2, strlen(p+2)+1);
			memmove(p, thestring, strlen(thestring));
		}
		if(strlen(name) + strlen(comp) + 3 >= sizeof(name)) {
			diag("library component too long");
			return;
		}
		if(i > 0 || !search)
			strcat(name, "/");
		strcat(name, comp);
	}
	cleanname(name);
	
	// runtime.a -> runtime
	p = nil;
	if(strlen(name) > 2 && name[strlen(name)-2] == '.') {
		p = name+strlen(name)-2;
		*p = '\0';
	}
	
	// already loaded?
	for(i=0; i<libraryp; i++)
		if(strcmp(library[i].pkg, name) == 0)
			return;
	
	// runtime -> runtime.a for search
	if(p != nil)
		*p = '.';

	if(search) {
		// try dot, -L "libdir", and then goroot.
		for(i=0; i<nlibdir; i++) {
			snprint(pname, sizeof pname, "%s/%s", libdir[i], name);
			if(access(pname, AEXIST) >= 0)
				break;
		}
	}else
		strcpy(pname, name);
	cleanname(pname);

	/* runtime.a -> runtime */
	if(p != nil)
		*p = '\0';

	if(debug['v'] > 1)
		Bprint(&bso, "%5.2f addlib: %s %s pulls in %s\n", cputime(), obj, src, pname);

	addlibpath(src, obj, pname, name);
}

/*
 * add library to library list.
 *	srcref: src file referring to package
 *	objref: object file referring to package
 *	file: object file, e.g., /home/rsc/go/pkg/container/vector.a
 *	pkg: package import path, e.g. container/vector
 */
void
addlibpath(char *srcref, char *objref, char *file, char *pkg)
{
	int i;
	Library *l;
	char *p;

	for(i=0; i<libraryp; i++)
		if(strcmp(file, library[i].file) == 0)
			return;

	if(debug['v'] > 1)
		Bprint(&bso, "%5.2f addlibpath: srcref: %s objref: %s file: %s pkg: %s\n",
			cputime(), srcref, objref, file, pkg);

	if(libraryp == nlibrary){
		nlibrary = 50 + 2*libraryp;
		library = erealloc(library, sizeof library[0] * nlibrary);
	}

	l = &library[libraryp++];

	p = mal(strlen(objref) + 1);
	strcpy(p, objref);
	l->objref = p;

	p = mal(strlen(srcref) + 1);
	strcpy(p, srcref);
	l->srcref = p;

	p = mal(strlen(file) + 1);
	strcpy(p, file);
	l->file = p;

	p = mal(strlen(pkg) + 1);
	strcpy(p, pkg);
	l->pkg = p;
}

void
loadinternal(char *name)
{
	char pname[1024];
	int i, found;

	found = 0;
	for(i=0; i<nlibdir; i++) {
		snprint(pname, sizeof pname, "%s/%s.a", libdir[i], name);
		if(debug['v'])
			Bprint(&bso, "searching for %s.a in %s\n", name, pname);
		if(access(pname, AEXIST) >= 0) {
			addlibpath("internal", "internal", pname, name);
			found = 1;
			break;
		}
	}
	if(!found)
		Bprint(&bso, "warning: unable to find %s.a\n", name);
}

void
loadlib(void)
{
	int i, w, x;
	Sym *s, *gmsym;

	if(flag_shared) {
		s = lookup("runtime.islibrary", 0);
		s->dupok = 1;
		adduint8(s, 1);
	}

	loadinternal("runtime");
	if(thechar == '5')
		loadinternal("math");
	if(flag_race)
		loadinternal("runtime/race");
	if(linkmode == LinkExternal) {
		// This indicates a user requested -linkmode=external.
		// The startup code uses an import of runtime/cgo to decide
		// whether to initialize the TLS.  So give it one.  This could
		// be handled differently but it's an unusual case.
		loadinternal("runtime/cgo");
		// Pretend that we really imported the package.
		// This will do no harm if we did in fact import it.
		s = lookup("go.importpath.runtime/cgo.", 0);
		s->type = SDATA;
		s->dupok = 1;
		s->reachable = 1;
	}

	for(i=0; i<libraryp; i++) {
		if(debug['v'] > 1)
			Bprint(&bso, "%5.2f autolib: %s (from %s)\n", cputime(), library[i].file, library[i].objref);
		iscgo |= strcmp(library[i].pkg, "runtime/cgo") == 0;
		objfile(library[i].file, library[i].pkg);
	}
	
	if(linkmode == LinkAuto) {
		if(iscgo && externalobj)
			linkmode = LinkExternal;
		else
			linkmode = LinkInternal;
	}

	if(linkmode == LinkInternal) {
		// Drop all the cgo_import_static declarations.
		// Turns out we won't be needing them.
		for(s = allsym; s != S; s = s->allsym)
			if(s->type == SHOSTOBJ) {
				// If a symbol was marked both
				// cgo_import_static and cgo_import_dynamic,
				// then we want to make it cgo_import_dynamic
				// now.
				if(s->extname != nil && s->dynimplib != nil && s->cgoexport == 0) {
					s->type = SDYNIMPORT;
				} else
					s->type = 0;
			}
	}
	
	gmsym = lookup("runtime.tlsgm", 0);
	gmsym->type = STLSBSS;
	gmsym->size = 2*PtrSize;
	gmsym->hide = 1;
	if(linkmode == LinkExternal && iself && HEADTYPE != Hopenbsd)
		gmsym->reachable = 1;
	else
		gmsym->reachable = 0;

	// Now that we know the link mode, trim the dynexp list.
	x = CgoExportDynamic;
	if(linkmode == LinkExternal)
		x = CgoExportStatic;
	w = 0;
	for(i=0; i<ndynexp; i++)
		if(dynexp[i]->cgoexport & x)
			dynexp[w++] = dynexp[i];
	ndynexp = w;
	
	// In internal link mode, read the host object files.
	if(linkmode == LinkInternal)
		hostobjs();
	else
		hostlinksetup();

	// We've loaded all the code now.
	// If there are no dynamic libraries needed, gcc disables dynamic linking.
	// Because of this, glibc's dynamic ELF loader occasionally (like in version 2.13)
	// assumes that a dynamic binary always refers to at least one dynamic library.
	// Rather than be a source of test cases for glibc, disable dynamic linking
	// the same way that gcc would.
	//
	// Exception: on OS X, programs such as Shark only work with dynamic
	// binaries, so leave it enabled on OS X (Mach-O) binaries.
	if(!flag_shared && !havedynamic && HEADTYPE != Hdarwin)
		debug['d'] = 1;
	
	importcycles();
}

/*
 * look for the next file in an archive.
 * adapted from libmach.
 */
int
nextar(Biobuf *bp, int off, struct ar_hdr *a)
{
	int r;
	int32 arsize;
	char *buf;

	if (off&01)
		off++;
	Bseek(bp, off, 0);
	buf = Brdline(bp, '\n');
	r = Blinelen(bp);
	if(buf == nil) {
		if(r == 0)
			return 0;
		return -1;
	}
	if(r != SAR_HDR)
		return -1;
	memmove(a, buf, SAR_HDR);
	if(strncmp(a->fmag, ARFMAG, sizeof a->fmag))
		return -1;
	arsize = strtol(a->size, 0, 0);
	if (arsize&1)
		arsize++;
	return arsize + r;
}

void
objfile(char *file, char *pkg)
{
	int32 off, l;
	Biobuf *f;
	char magbuf[SARMAG];
	char pname[150];
	struct ar_hdr arhdr;

	pkg = smprint("%i", pkg);

	if(debug['v'] > 1)
		Bprint(&bso, "%5.2f ldobj: %s (%s)\n", cputime(), file, pkg);
	Bflush(&bso);
	f = Bopen(file, 0);
	if(f == nil) {
		diag("cannot open file: %s", file);
		errorexit();
	}
	l = Bread(f, magbuf, SARMAG);
	if(l != SARMAG || strncmp(magbuf, ARMAG, SARMAG)){
		/* load it as a regular file */
		l = Bseek(f, 0L, 2);
		Bseek(f, 0L, 0);
		ldobj(f, pkg, l, file, file, FileObj);
		Bterm(f);
		free(pkg);
		return;
	}
	
	/* skip over __.GOSYMDEF */
	off = Boffset(f);
	if((l = nextar(f, off, &arhdr)) <= 0) {
		diag("%s: short read on archive file symbol header", file);
		goto out;
	}
	if(strncmp(arhdr.name, symname, strlen(symname))) {
		diag("%s: first entry not symbol header", file);
		goto out;
	}
	off += l;
	
	/* skip over (or process) __.PKGDEF */
	if((l = nextar(f, off, &arhdr)) <= 0) {
		diag("%s: short read on archive file symbol header", file);
		goto out;
	}
	if(strncmp(arhdr.name, pkgname, strlen(pkgname))) {
		diag("%s: second entry not package header", file);
		goto out;
	}
	off += l;

	if(debug['u'])
		ldpkg(f, pkg, atolwhex(arhdr.size), file, Pkgdef);

	/*
	 * load all the object files from the archive now.
	 * this gives us sequential file access and keeps us
	 * from needing to come back later to pick up more
	 * objects.  it breaks the usual C archive model, but
	 * this is Go, not C.  the common case in Go is that
	 * we need to load all the objects, and then we throw away
	 * the individual symbols that are unused.
	 *
	 * loading every object will also make it possible to
	 * load foreign objects not referenced by __.GOSYMDEF.
	 */
	for(;;) {
		l = nextar(f, off, &arhdr);
		if(l == 0)
			break;
		if(l < 0) {
			diag("%s: malformed archive", file);
			goto out;
		}
		off += l;

		l = SARNAME;
		while(l > 0 && arhdr.name[l-1] == ' ')
			l--;
		snprint(pname, sizeof pname, "%s(%.*s)", file, utfnlen(arhdr.name, l), arhdr.name);
		l = atolwhex(arhdr.size);
		ldobj(f, pkg, l, pname, file, ArchiveObj);
	}

out:
	Bterm(f);
	free(pkg);
}

static void
dowrite(int fd, char *p, int n)
{
	int m;
	
	while(n > 0) {
		m = write(fd, p, n);
		if(m <= 0) {
			cursym = S;
			diag("write error: %r");
			errorexit();
		}
		n -= m;
		p += m;
	}
}

typedef struct Hostobj Hostobj;

struct Hostobj
{
	void (*ld)(Biobuf*, char*, int64, char*);
	char *pkg;
	char *pn;
	char *file;
	int64 off;
	int64 len;
};

Hostobj *hostobj;
int nhostobj;
int mhostobj;

// These packages can use internal linking mode.
// Others trigger external mode.
const char *internalpkg[] = {
	"crypto/x509",
	"net",
	"os/user",
	"runtime/cgo",
	"runtime/race"
};

void
ldhostobj(void (*ld)(Biobuf*, char*, int64, char*), Biobuf *f, char *pkg, int64 len, char *pn, char *file)
{
	int i, isinternal;
	Hostobj *h;

	isinternal = 0;
	for(i=0; i<nelem(internalpkg); i++) {
		if(strcmp(pkg, internalpkg[i]) == 0) {
			isinternal = 1;
			break;
		}
	}

	// DragonFly declares errno with __thread, which results in a symbol
	// type of R_386_TLS_GD or R_X86_64_TLSGD. The Go linker does not
	// currently know how to handle TLS relocations, hence we have to
	// force external linking for any libraries that link in code that
	// uses errno. This can be removed if the Go linker ever supports
	// these relocation types.
	if(HEADTYPE == Hdragonfly)
	if(strcmp(pkg, "net") == 0 || strcmp(pkg, "os/user") == 0)
		isinternal = 0;

	if(!isinternal)
		externalobj = 1;

	if(nhostobj >= mhostobj) {
		if(mhostobj == 0)
			mhostobj = 16;
		else
			mhostobj *= 2;
		hostobj = erealloc(hostobj, mhostobj*sizeof hostobj[0]);
	}
	h = &hostobj[nhostobj++];
	h->ld = ld;
	h->pkg = estrdup(pkg);
	h->pn = estrdup(pn);
	h->file = estrdup(file);
	h->off = Boffset(f);
	h->len = len;
}

void
hostobjs(void)
{
	int i;
	Biobuf *f;
	Hostobj *h;
	
	for(i=0; i<nhostobj; i++) {
		h = &hostobj[i];
		f = Bopen(h->file, OREAD);
		if(f == nil) {
			cursym = S;
			diag("cannot reopen %s: %r", h->pn);
			errorexit();
		}
		Bseek(f, h->off, 0);
		h->ld(f, h->pkg, h->len, h->pn);
		Bterm(f);
	}
}

// provided by lib9
int runcmd(char**);
char* mktempdir(void);
void removeall(char*);

static void
rmtemp(void)
{
	removeall(tmpdir);
}

static void
hostlinksetup(void)
{
	char *p;

	if(linkmode != LinkExternal)
		return;

	// create temporary directory and arrange cleanup
	if(tmpdir == nil) {
		tmpdir = mktempdir();
		atexit(rmtemp);
	}

	// change our output to temporary object file
	close(cout);
	p = smprint("%s/go.o", tmpdir);
	cout = create(p, 1, 0775);
	if(cout < 0) {
		diag("cannot create %s: %r", p);
		errorexit();
	}
	free(p);
}

void
hostlink(void)
{
	char *p, **argv;
	int c, i, w, n, argc, len;
	Hostobj *h;
	Biobuf *f;
	static char buf[64<<10];

	if(linkmode != LinkExternal || nerrors > 0)
		return;

	c = 0;
	p = extldflags;
	while(p != nil) {
		while(*p == ' ')
			p++;
		if(*p == '\0')
			break;
		c++;
		p = strchr(p + 1, ' ');
	}

	argv = malloc((13+nhostobj+nldflag+c)*sizeof argv[0]);
	argc = 0;
	if(extld == nil)
		extld = "gcc";
	argv[argc++] = extld;
	switch(thechar){
	case '8':
		argv[argc++] = "-m32";
		break;
	case '6':
		argv[argc++] = "-m64";
		break;
	case '5':
		argv[argc++] = "-marm";
		break;
	}
	if(!debug['s'] && !debug_s) {
		argv[argc++] = "-gdwarf-2"; 
	} else {
		argv[argc++] = "-s";
	}
	if(HEADTYPE == Hdarwin)
		argv[argc++] = "-Wl,-no_pie,-pagezero_size,4000000";
	
	if(iself && AssumeGoldLinker)
		argv[argc++] = "-Wl,--rosegment";

	if(flag_shared) {
		argv[argc++] = "-Wl,-Bsymbolic";
		argv[argc++] = "-shared";
	}
	argv[argc++] = "-o";
	argv[argc++] = outfile;
	
	if(rpath)
		argv[argc++] = smprint("-Wl,-rpath,%s", rpath);

	// Force global symbols to be exported for dlopen, etc.
	if(iself)
		argv[argc++] = "-rdynamic";

	// already wrote main object file
	// copy host objects to temporary directory
	for(i=0; i<nhostobj; i++) {
		h = &hostobj[i];
		f = Bopen(h->file, OREAD);
		if(f == nil) {
			cursym = S;
			diag("cannot reopen %s: %r", h->pn);
			errorexit();
		}
		Bseek(f, h->off, 0);
		p = smprint("%s/%06d.o", tmpdir, i);
		argv[argc++] = p;
		w = create(p, 1, 0775);
		if(w < 0) {
			cursym = S;
			diag("cannot create %s: %r", p);
			errorexit();
		}
		len = h->len;
		while(len > 0 && (n = Bread(f, buf, sizeof buf)) > 0){
			if(n > len)
				n = len;
			dowrite(w, buf, n);
			len -= n;
		}
		if(close(w) < 0) {
			cursym = S;
			diag("cannot write %s: %r", p);
			errorexit();
		}
		Bterm(f);
	}
	
	argv[argc++] = smprint("%s/go.o", tmpdir);
	for(i=0; i<nldflag; i++)
		argv[argc++] = ldflag[i];

	p = extldflags;
	while(p != nil) {
		while(*p == ' ')
			*p++ = '\0';
		if(*p == '\0')
			break;
		argv[argc++] = p;
		p = strchr(p + 1, ' ');
	}

	argv[argc] = nil;

	quotefmtinstall();
	if(debug['v']) {
		Bprint(&bso, "host link:");
		for(i=0; i<argc; i++)
			Bprint(&bso, " %q", argv[i]);
		Bprint(&bso, "\n");
		Bflush(&bso);
	}

	if(runcmd(argv) < 0) {
		cursym = S;
		diag("%s: running %s failed: %r", argv0, argv[0]);
		errorexit();
	}
}

void
ldobj(Biobuf *f, char *pkg, int64 len, char *pn, char *file, int whence)
{
	char *line;
	int n, c1, c2, c3, c4;
	uint32 magic;
	vlong import0, import1, eof;
	char *t;

	eof = Boffset(f) + len;

	pn = estrdup(pn);

	c1 = BGETC(f);
	c2 = BGETC(f);
	c3 = BGETC(f);
	c4 = BGETC(f);
	Bungetc(f);
	Bungetc(f);
	Bungetc(f);
	Bungetc(f);

	magic = c1<<24 | c2<<16 | c3<<8 | c4;
	if(magic == 0x7f454c46) {	// \x7F E L F
		ldhostobj(ldelf, f, pkg, len, pn, file);
		return;
	}
	if((magic&~1) == 0xfeedface || (magic&~0x01000000) == 0xcefaedfe) {
		ldhostobj(ldmacho, f, pkg, len, pn, file);
		return;
	}
	if(c1 == 0x4c && c2 == 0x01 || c1 == 0x64 && c2 == 0x86) {
		ldhostobj(ldpe, f, pkg, len, pn, file);
		return;
	}

	/* check the header */
	line = Brdline(f, '\n');
	if(line == nil) {
		if(Blinelen(f) > 0) {
			diag("%s: not an object file", pn);
			return;
		}
		goto eof;
	}
	n = Blinelen(f) - 1;
	line[n] = '\0';
	if(strncmp(line, "go object ", 10) != 0) {
		if(strlen(pn) > 3 && strcmp(pn+strlen(pn)-3, ".go") == 0) {
			print("%cl: input %s is not .%c file (use %cg to compile .go files)\n", thechar, pn, thechar, thechar);
			errorexit();
		}
		if(strcmp(line, thestring) == 0) {
			// old header format: just $GOOS
			diag("%s: stale object file", pn);
			return;
		}
		diag("%s: not an object file", pn);
		free(pn);
		return;
	}
	
	// First, check that the basic goos, string, and version match.
	t = smprint("%s %s %s ", goos, thestring, getgoversion());
	line[n] = ' ';
	if(strncmp(line+10, t, strlen(t)) != 0 && !debug['f']) {
		line[n] = '\0';
		diag("%s: object is [%s] expected [%s]", pn, line+10, t);
		free(t);
		free(pn);
		return;
	}
	
	// Second, check that longer lines match each other exactly,
	// so that the Go compiler and write additional information
	// that must be the same from run to run.
	line[n] = '\0';
	if(n-10 > strlen(t)) {
		if(theline == nil)
			theline = estrdup(line+10);
		else if(strcmp(theline, line+10) != 0) {
			line[n] = '\0';
			diag("%s: object is [%s] expected [%s]", pn, line+10, theline);
			free(t);
			free(pn);
			return;
		}
	}
	free(t);
	line[n] = '\n';

	/* skip over exports and other info -- ends with \n!\n */
	import0 = Boffset(f);
	c1 = '\n';	// the last line ended in \n
	c2 = BGETC(f);
	c3 = BGETC(f);
	while(c1 != '\n' || c2 != '!' || c3 != '\n') {
		c1 = c2;
		c2 = c3;
		c3 = BGETC(f);
		if(c3 == Beof)
			goto eof;
	}
	import1 = Boffset(f);

	Bseek(f, import0, 0);
	ldpkg(f, pkg, import1 - import0 - 2, pn, whence);	// -2 for !\n
	Bseek(f, import1, 0);

	ldobj1(f, pkg, eof - Boffset(f), pn);
	free(pn);
	return;

eof:
	diag("truncated object file: %s", pn);
	free(pn);
}

Sym*
newsym(char *symb, int v)
{
	Sym *s;
	int l;

	l = strlen(symb) + 1;
	s = mal(sizeof(*s));
	if(debug['v'] > 1)
		Bprint(&bso, "newsym %s\n", symb);

	s->dynid = -1;
	s->plt = -1;
	s->got = -1;
	s->name = mal(l + 1);
	memmove(s->name, symb, l);

	s->type = 0;
	s->version = v;
	s->value = 0;
	s->sig = 0;
	s->size = 0;
	nsymbol++;

	s->allsym = allsym;
	allsym = s;

	return s;
}

static Sym*
_lookup(char *symb, int v, int creat)
{
	Sym *s;
	char *p;
	uint32 h;
	int c;

	h = v;
	for(p=symb; c = *p; p++)
		h = h+h+h + c;
	// not if(h < 0) h = ~h, because gcc 4.3 -O2 miscompiles it.
	h &= 0xffffff;
	h %= NHASH;
	for(s = hash[h]; s != S; s = s->hash)
		if(strcmp(s->name, symb) == 0)
			return s;
	if(!creat)
		return nil;

	s = newsym(symb, v);
	s->extname = s->name;
	s->hash = hash[h];
	hash[h] = s;

	return s;
}

Sym*
lookup(char *name, int v)
{
	return _lookup(name, v, 1);
}

// read-only lookup
Sym*
rlookup(char *name, int v)
{
	return _lookup(name, v, 0);
}

void
copyhistfrog(char *buf, int nbuf)
{
	char *p, *ep;
	int i;

	p = buf;
	ep = buf + nbuf;
	for(i=0; i<histfrogp; i++) {
		p = seprint(p, ep, "%s", histfrog[i]->name+1);
		if(i+1<histfrogp && (p == buf || p[-1] != '/'))
			p = seprint(p, ep, "/");
	}
}

void
addhist(int32 line, int type)
{
	Auto *u;
	Sym *s;
	int i, j, k;

	u = mal(sizeof(Auto));
	s = mal(sizeof(Sym));
	s->name = mal(2*(histfrogp+1) + 1);

	u->asym = s;
	u->type = type;
	u->aoffset = line;
	u->link = curhist;
	curhist = u;

	s->name[0] = 0;
	j = 1;
	for(i=0; i<histfrogp; i++) {
		k = histfrog[i]->value;
		s->name[j+0] = k>>8;
		s->name[j+1] = k;
		j += 2;
	}
	s->name[j] = 0;
	s->name[j+1] = 0;
}

void
histtoauto(void)
{
	Auto *l;

	while(l = curhist) {
		curhist = l->link;
		l->link = curauto;
		curauto = l;
	}
}

void
collapsefrog(Sym *s)
{
	int i;

	/*
	 * bad encoding of path components only allows
	 * MAXHIST components. if there is an overflow,
	 * first try to collapse xxx/..
	 */
	for(i=1; i<histfrogp; i++)
		if(strcmp(histfrog[i]->name+1, "..") == 0) {
			memmove(histfrog+i-1, histfrog+i+1,
				(histfrogp-i-1)*sizeof(histfrog[0]));
			histfrogp--;
			goto out;
		}

	/*
	 * next try to collapse .
	 */
	for(i=0; i<histfrogp; i++)
		if(strcmp(histfrog[i]->name+1, ".") == 0) {
			memmove(histfrog+i, histfrog+i+1,
				(histfrogp-i-1)*sizeof(histfrog[0]));
			goto out;
		}

	/*
	 * last chance, just truncate from front
	 */
	memmove(histfrog+0, histfrog+1,
		(histfrogp-1)*sizeof(histfrog[0]));

out:
	histfrog[histfrogp-1] = s;
}

void
nuxiinit(void)
{
	int i, c;

	for(i=0; i<4; i++) {
		c = find1(0x04030201L, i+1);
		if(i < 2)
			inuxi2[i] = c;
		if(i < 1)
			inuxi1[i] = c;
		inuxi4[i] = c;
		if(c == i) {
			inuxi8[i] = c;
			inuxi8[i+4] = c+4;
		} else {
			inuxi8[i] = c+4;
			inuxi8[i+4] = c;
		}
		fnuxi4[i] = c;
		fnuxi8[i] = c;
		fnuxi8[i+4] = c+4;
	}
	if(debug['v']) {
		Bprint(&bso, "inuxi = ");
		for(i=0; i<1; i++)
			Bprint(&bso, "%d", inuxi1[i]);
		Bprint(&bso, " ");
		for(i=0; i<2; i++)
			Bprint(&bso, "%d", inuxi2[i]);
		Bprint(&bso, " ");
		for(i=0; i<4; i++)
			Bprint(&bso, "%d", inuxi4[i]);
		Bprint(&bso, " ");
		for(i=0; i<8; i++)
			Bprint(&bso, "%d", inuxi8[i]);
		Bprint(&bso, "\nfnuxi = ");
		for(i=0; i<4; i++)
			Bprint(&bso, "%d", fnuxi4[i]);
		Bprint(&bso, " ");
		for(i=0; i<8; i++)
			Bprint(&bso, "%d", fnuxi8[i]);
		Bprint(&bso, "\n");
	}
	Bflush(&bso);
}

int
find1(int32 l, int c)
{
	char *p;
	int i;

	p = (char*)&l;
	for(i=0; i<4; i++)
		if(*p++ == c)
			return i;
	return 0;
}

int
find2(int32 l, int c)
{
	union {
		int32 l;
		short p[2];
	} u;
	short *p;
	int i;

	u.l = l;
	p = u.p;
	for(i=0; i<4; i+=2) {
		if(((*p >> 8) & 0xff) == c)
			return i;
		if((*p++ & 0xff) == c)
			return i+1;
	}
	return 0;
}

int32
ieeedtof(Ieee *e)
{
	int exp;
	int32 v;

	if(e->h == 0)
		return 0;
	exp = (e->h>>20) & ((1L<<11)-1L);
	exp -= (1L<<10) - 2L;
	v = (e->h & 0xfffffL) << 3;
	v |= (e->l >> 29) & 0x7L;
	if((e->l >> 28) & 1) {
		v++;
		if(v & 0x800000L) {
			v = (v & 0x7fffffL) >> 1;
			exp++;
		}
	}
	if(-148 <= exp && exp <= -126) {
		v |= 1<<23;
		v >>= -125 - exp;
		exp = -126;
	}
	else if(exp < -148 || exp >= 130)
		diag("double fp to single fp overflow: %.17g", ieeedtod(e));
	v |= ((exp + 126) & 0xffL) << 23;
	v |= e->h & 0x80000000L;
	return v;
}

double
ieeedtod(Ieee *ieeep)
{
	Ieee e;
	double fr;
	int exp;

	if(ieeep->h & (1L<<31)) {
		e.h = ieeep->h & ~(1L<<31);
		e.l = ieeep->l;
		return -ieeedtod(&e);
	}
	if(ieeep->l == 0 && ieeep->h == 0)
		return 0;
	exp = (ieeep->h>>20) & ((1L<<11)-1L);
	exp -= (1L<<10) - 2L;
	fr = ieeep->l & ((1L<<16)-1L);
	fr /= 1L<<16;
	fr += (ieeep->l>>16) & ((1L<<16)-1L);
	fr /= 1L<<16;
	if(exp == -(1L<<10) - 2L) {
		fr += (ieeep->h & (1L<<20)-1L);
		exp++;
	} else
		fr += (ieeep->h & (1L<<20)-1L) | (1L<<20);
	fr /= 1L<<21;
	return ldexp(fr, exp);
}

void
zerosig(char *sp)
{
	Sym *s;

	s = lookup(sp, 0);
	s->sig = 0;
}

void
mywhatsys(void)
{
	goroot = getgoroot();
	goos = getgoos();
	goarch = thestring;	// ignore $GOARCH - we know who we are
}

int
pathchar(void)
{
	return '/';
}

static	uchar*	hunk;
static	uint32	nhunk;
#define	NHUNK	(10UL<<20)

void*
mal(uint32 n)
{
	void *v;

	n = (n+7)&~7;
	if(n > NHUNK) {
		v = malloc(n);
		if(v == nil) {
			diag("out of memory");
			errorexit();
		}
		memset(v, 0, n);
		return v;
	}
	if(n > nhunk) {
		hunk = malloc(NHUNK);
		if(hunk == nil) {
			diag("out of memory");
			errorexit();
		}
		nhunk = NHUNK;
	}

	v = hunk;
	nhunk -= n;
	hunk += n;

	memset(v, 0, n);
	return v;
}

void
unmal(void *v, uint32 n)
{
	n = (n+7)&~7;
	if(hunk - n == v) {
		hunk -= n;
		nhunk += n;
	}
}

// Copied from ../gc/subr.c:/^pathtoprefix; must stay in sync.
/*
 * Convert raw string to the prefix that will be used in the symbol table.
 * Invalid bytes turn into %xx.	 Right now the only bytes that need
 * escaping are %, ., and ", but we escape all control characters too.
 *
 * Must be same as ../gc/subr.c:/^pathtoprefix.
 */
static char*
pathtoprefix(char *s)
{
	static char hex[] = "0123456789abcdef";
	char *p, *r, *w, *l;
	int n;

	// find first character past the last slash, if any.
	l = s;
	for(r=s; *r; r++)
		if(*r == '/')
			l = r+1;

	// check for chars that need escaping
	n = 0;
	for(r=s; *r; r++)
		if(*r <= ' ' || (*r == '.' && r >= l) || *r == '%' || *r == '"' || *r >= 0x7f)
			n++;

	// quick exit
	if(n == 0)
		return s;

	// escape
	p = mal((r-s)+1+2*n);
	for(r=s, w=p; *r; r++) {
		if(*r <= ' ' || (*r == '.' && r >= l) || *r == '%' || *r == '"' || *r >= 0x7f) {
			*w++ = '%';
			*w++ = hex[(*r>>4)&0xF];
			*w++ = hex[*r&0xF];
		} else
			*w++ = *r;
	}
	*w = '\0';
	return p;
}

int
iconv(Fmt *fp)
{
	char *p;

	p = va_arg(fp->args, char*);
	if(p == nil) {
		fmtstrcpy(fp, "<nil>");
		return 0;
	}
	p = pathtoprefix(p);
	fmtstrcpy(fp, p);
	return 0;
}

void
mangle(char *file)
{
	fprint(2, "%s: mangled input file\n", file);
	errorexit();
}

Section*
addsection(Segment *seg, char *name, int rwx)
{
	Section **l;
	Section *sect;
	
	for(l=&seg->sect; *l; l=&(*l)->next)
		;
	sect = mal(sizeof *sect);
	sect->rwx = rwx;
	sect->name = name;
	sect->seg = seg;
	sect->align = PtrSize; // everything is at least pointer-aligned
	*l = sect;
	return sect;
}

void
addvarint(Sym *s, uint32 val)
{
	int32 n;
	uint32 v;
	uchar *p;

	n = 0;
	for(v = val; v >= 0x80; v >>= 7)
		n++;
	n++;

	symgrow(s, s->np+n);

	p = s->p + s->np - n;
	for(v = val; v >= 0x80; v >>= 7)
		*p++ = v | 0x80;
	*p = v;
}

// funcpctab appends to dst a pc-value table mapping the code in func to the values
// returned by valfunc parameterized by arg. The invocation of valfunc to update the
// current value is, for each p,
//
//	val = valfunc(func, val, p, 0, arg);
//	record val as value at p->pc;
//	val = valfunc(func, val, p, 1, arg);
//
// where func is the function, val is the current value, p is the instruction being
// considered, and arg can be used to further parameterize valfunc.
void
funcpctab(Sym *dst, Sym *func, char *desc, int32 (*valfunc)(Sym*, int32, Prog*, int32, int32), int32 arg)
{
	int dbg, i, start;
	int32 oldval, val, started;
	uint32 delta;
	vlong pc;
	Prog *p;

	// To debug a specific function, uncomment second line and change name.
	dbg = 0;
	//dbg = strcmp(func->name, "main.main") == 0;

	debug['O'] += dbg;

	start = dst->np;

	if(debug['O'])
		Bprint(&bso, "funcpctab %s -> %s [valfunc=%s]\n", func->name, dst->name, desc);

	val = -1;
	oldval = val;
	pc = func->value;
	
	if(debug['O'])
		Bprint(&bso, "%6llux %6d %P\n", pc, val, func->text);

	started = 0;
	for(p=func->text; p != P; p = p->link) {
		// Update val. If it's not changing, keep going.
		val = valfunc(func, val, p, 0, arg);
		if(val == oldval && started) {
			val = valfunc(func, val, p, 1, arg);
			if(debug['O'])
				Bprint(&bso, "%6llux %6s %P\n", (vlong)p->pc, "", p);
			continue;
		}

		// If the pc of the next instruction is the same as the
		// pc of this instruction, this instruction is not a real
		// instruction. Keep going, so that we only emit a delta
		// for a true instruction boundary in the program.
		if(p->link && p->link->pc == p->pc) {
			val = valfunc(func, val, p, 1, arg);
			if(debug['O'])
				Bprint(&bso, "%6llux %6s %P\n", (vlong)p->pc, "", p);
			continue;
		}

		// The table is a sequence of (value, pc) pairs, where each
		// pair states that the given value is in effect from the current position
		// up to the given pc, which becomes the new current position.
		// To generate the table as we scan over the program instructions,
		// we emit a "(value" when pc == func->value, and then
		// each time we observe a change in value we emit ", pc) (value".
		// When the scan is over, we emit the closing ", pc)".
		//
		// The table is delta-encoded. The value deltas are signed and
		// transmitted in zig-zag form, where a complement bit is placed in bit 0,
		// and the pc deltas are unsigned. Both kinds of deltas are sent
		// as variable-length little-endian base-128 integers,
		// where the 0x80 bit indicates that the integer continues.

		if(debug['O'])
			Bprint(&bso, "%6llux %6d %P\n", (vlong)p->pc, val, p);

		if(started) {
			addvarint(dst, (p->pc - pc) / MINLC);
			pc = p->pc;
		}
		delta = val - oldval;
		if(delta>>31)
			delta = 1 | ~(delta<<1);
		else
			delta <<= 1;
		addvarint(dst, delta);
		oldval = val;
		started = 1;
		val = valfunc(func, val, p, 1, arg);
	}

	if(started) {
		if(debug['O'])
			Bprint(&bso, "%6llux done\n", (vlong)func->value+func->size);
		addvarint(dst, (func->value+func->size - pc) / MINLC);
		addvarint(dst, 0); // terminator
	}

	if(debug['O']) {
		Bprint(&bso, "wrote %d bytes\n", dst->np - start);
		for(i=start; i<dst->np; i++)
			Bprint(&bso, " %02ux", dst->p[i]);
		Bprint(&bso, "\n");
	}

	debug['O'] -= dbg;
}

// pctofileline computes either the file number (arg == 0)
// or the line number (arg == 1) to use at p.
// Because p->lineno applies to p, phase == 0 (before p)
// takes care of the update.
static int32
pctofileline(Sym *sym, int32 oldval, Prog *p, int32 phase, int32 arg)
{
	int32 f, l;

	if(p->as == ATEXT || p->as == ANOP || p->as == AUSEFIELD || p->line == 0 || phase == 1)
		return oldval;
	getline(sym->hist, p->line, &f, &l);
	if(f == 0) {
	//	print("getline failed for %s %P\n", cursym->name, p);
		return oldval;
	}
	if(arg == 0)
		return f;
	return l;
}

// pctospadj computes the sp adjustment in effect.
// It is oldval plus any adjustment made by p itself.
// The adjustment by p takes effect only after p, so we
// apply the change during phase == 1.
static int32
pctospadj(Sym *sym, int32 oldval, Prog *p, int32 phase, int32 arg)
{
	USED(arg);
	USED(sym);

	if(oldval == -1) // starting
		oldval = 0;
	if(phase == 0)
		return oldval;
	if(oldval + p->spadj < -10000 || oldval + p->spadj > 1100000000) {
		diag("overflow in spadj: %d + %d = %d", oldval, p->spadj, oldval + p->spadj);
		errorexit();
	}
	return oldval + p->spadj;
}

// pctopcdata computes the pcdata value in effect at p.
// A PCDATA instruction sets the value in effect at future
// non-PCDATA instructions.
// Since PCDATA instructions have no width in the final code,
// it does not matter which phase we use for the update.
static int32
pctopcdata(Sym *sym, int32 oldval, Prog *p, int32 phase, int32 arg)
{
	USED(sym);

	if(phase == 0 || p->as != APCDATA || p->from.offset != arg)
		return oldval;
	if((int32)p->to.offset != p->to.offset) {
		diag("overflow in PCDATA instruction: %P", p);
		errorexit();
	}
	return p->to.offset;
}

#define	LOG	5
void
mkfwd(void)
{
	Prog *p;
	int i;
	int32 dwn[LOG], cnt[LOG];
	Prog *lst[LOG];

	for(i=0; i<LOG; i++) {
		if(i == 0)
			cnt[i] = 1;
		else
			cnt[i] = LOG * cnt[i-1];
		dwn[i] = 1;
		lst[i] = P;
	}
	i = 0;
	for(cursym = textp; cursym != nil; cursym = cursym->next) {
		for(p = cursym->text; p != P; p = p->link) {
			if(p->link == P) {
				if(cursym->next)
					p->forwd = cursym->next->text;
				break;
			}
			i--;
			if(i < 0)
				i = LOG-1;
			p->forwd = P;
			dwn[i]--;
			if(dwn[i] <= 0) {
				dwn[i] = cnt[i];
				if(lst[i] != P)
					lst[i]->forwd = p;
				lst[i] = p;
			}
		}
	}
}

uint16
le16(uchar *b)
{
	return b[0] | b[1]<<8;
}

uint32
le32(uchar *b)
{
	return b[0] | b[1]<<8 | b[2]<<16 | (uint32)b[3]<<24;
}

uint64
le64(uchar *b)
{
	return le32(b) | (uint64)le32(b+4)<<32;
}

uint16
be16(uchar *b)
{
	return b[0]<<8 | b[1];
}

uint32
be32(uchar *b)
{
	return (uint32)b[0]<<24 | b[1]<<16 | b[2]<<8 | b[3];
}

uint64
be64(uchar *b)
{
	return (uvlong)be32(b)<<32 | be32(b+4);
}

Endian be = { be16, be32, be64 };
Endian le = { le16, le32, le64 };

typedef struct Chain Chain;
struct Chain
{
	Sym *sym;
	Chain *up;
	int limit;  // limit on entry to sym
};

static int stkcheck(Chain*, int);
static void stkprint(Chain*, int);
static void stkbroke(Chain*, int);
static Sym *morestack;
static Sym *newstack;

enum
{
	HasLinkRegister = (thechar == '5'),
	CallSize = (!HasLinkRegister)*PtrSize,	// bytes of stack required for a call
};

void
dostkcheck(void)
{
	Chain ch;
	Sym *s;
	
	morestack = lookup("runtime.morestack", 0);
	newstack = lookup("runtime.newstack", 0);

	// First the nosplits on their own.
	for(s = textp; s != nil; s = s->next) {
		if(s->text == nil || s->text->link == nil || (s->text->textflag & NOSPLIT) == 0)
			continue;
		cursym = s;
		ch.up = nil;
		ch.sym = s;
		ch.limit = StackLimit - CallSize;
		stkcheck(&ch, 0);
		s->stkcheck = 1;
	}
	
	// Check calling contexts.
	// Some nosplits get called a little further down,
	// like newproc and deferproc.	We could hard-code
	// that knowledge but it's more robust to look at
	// the actual call sites.
	for(s = textp; s != nil; s = s->next) {
		if(s->text == nil || s->text->link == nil || (s->text->textflag & NOSPLIT) != 0)
			continue;
		cursym = s;
		ch.up = nil;
		ch.sym = s;
		ch.limit = StackLimit - CallSize;
		stkcheck(&ch, 0);
	}
}

static int
stkcheck(Chain *up, int depth)
{
	Chain ch, ch1;
	Prog *p;
	Sym *s;
	int limit, prolog;
	
	limit = up->limit;
	s = up->sym;
	p = s->text;
	
	// Small optimization: don't repeat work at top.
	if(s->stkcheck && limit == StackLimit-CallSize)
		return 0;
	
	if(depth > 100) {
		diag("nosplit stack check too deep");
		stkbroke(up, 0);
		return -1;
	}

	if(p == nil || p->link == nil) {
		// external function.
		// should never be called directly.
		// only diagnose the direct caller.
		if(depth == 1)
			diag("call to external function %s", s->name);
		return -1;
	}

	if(limit < 0) {
		stkbroke(up, limit);
		return -1;
	}

	// morestack looks like it calls functions,
	// but it switches the stack pointer first.
	if(s == morestack)
		return 0;

	ch.up = up;
	prolog = (s->text->textflag & NOSPLIT) == 0;
	for(p = s->text; p != P; p = p->link) {
		limit -= p->spadj;
		if(prolog && p->spadj != 0) {
			// The first stack adjustment in a function with a
			// split-checking prologue marks the end of the
			// prologue.  Assuming the split check is correct,
			// after the adjustment there should still be at least
			// StackLimit bytes available below the stack pointer.
			// If this is not the top call in the chain, no need
			// to duplicate effort, so just stop.
			if(depth > 0)
				return 0;
			prolog = 0;
			limit = StackLimit;
		}
		if(limit < 0) {
			stkbroke(up, limit);
			return -1;
		}
		if(iscall(p)) {
			limit -= CallSize;
			ch.limit = limit;
			if(p->to.type == D_BRANCH) {
				// Direct call.
				ch.sym = p->to.sym;
				if(stkcheck(&ch, depth+1) < 0)
					return -1;
			} else {
				// Indirect call.  Assume it is a splitting function,
				// so we have to make sure it can call morestack.
				limit -= CallSize;
				ch.sym = nil;
				ch1.limit = limit;
				ch1.up = &ch;
				ch1.sym = morestack;
				if(stkcheck(&ch1, depth+2) < 0)
					return -1;
				limit += CallSize;
			}
			limit += CallSize;
		}
		
	}
	return 0;
}

static void
stkbroke(Chain *ch, int limit)
{
	diag("nosplit stack overflow");
	stkprint(ch, limit);
}

static void
stkprint(Chain *ch, int limit)
{
	char *name;

	if(ch->sym)
		name = ch->sym->name;
	else
		name = "function pointer";

	if(ch->up == nil) {
		// top of chain.  ch->sym != nil.
		if(ch->sym->text->textflag & NOSPLIT)
			print("\t%d\tassumed on entry to %s\n", ch->limit, name);
		else
			print("\t%d\tguaranteed after split check in %s\n", ch->limit, name);
	} else {
		stkprint(ch->up, ch->limit + (!HasLinkRegister)*PtrSize);
		if(!HasLinkRegister)
			print("\t%d\ton entry to %s\n", ch->limit, name);
	}
	if(ch->limit != limit)
		print("\t%d\tafter %s uses %d\n", limit, name, ch->limit - limit);
}

int
headtype(char *name)
{
	int i;

	for(i=0; headers[i].name; i++)
		if(strcmp(name, headers[i].name) == 0) {
			headstring = headers[i].name;
			return headers[i].val;
		}
	fprint(2, "unknown header type -H %s\n", name);
	errorexit();
	return -1;  // not reached
}

char*
headstr(int v)
{
	static char buf[20];
	int i;

	for(i=0; headers[i].name; i++)
		if(v == headers[i].val)
			return headers[i].name;
	snprint(buf, sizeof buf, "%d", v);
	return buf;
}

void
undef(void)
{
	Sym *s;

	for(s = allsym; s != S; s = s->allsym)
		if(s->type == SXREF)
			diag("%s(%d): not defined", s->name, s->version);
}

int
Yconv(Fmt *fp)
{
	Sym *s;
	Fmt fmt;
	int i;
	char *str;

	s = va_arg(fp->args, Sym*);
	if (s == S) {
		fmtprint(fp, "<nil>");
	} else {
		fmtstrinit(&fmt);
		fmtprint(&fmt, "%s @0x%08llx [%lld]", s->name, (vlong)s->value, (vlong)s->size);
		for (i = 0; i < s->size; i++) {
			if (!(i%8)) fmtprint(&fmt,  "\n\t0x%04x ", i);
			fmtprint(&fmt, "%02x ", s->p[i]);
		}
		fmtprint(&fmt, "\n");
		for (i = 0; i < s->nr; i++) {
			fmtprint(&fmt, "\t0x%04x[%x] %d %s[%llx]\n",
			      s->r[i].off,
			      s->r[i].siz,
			      s->r[i].type,
			      s->r[i].sym->name,
			      (vlong)s->r[i].add);
		}
		str = fmtstrflush(&fmt);
		fmtstrcpy(fp, str);
		free(str);
	}

	return 0;
}

vlong coutpos;

void
cflush(void)
{
	int n;

	if(cbpmax < cbp)
		cbpmax = cbp;
	n = cbpmax - buf.cbuf;
	dowrite(cout, buf.cbuf, n);
	coutpos += n;
	cbp = buf.cbuf;
	cbc = sizeof(buf.cbuf);
	cbpmax = cbp;
}

vlong
cpos(void)
{
	return coutpos + cbp - buf.cbuf;
}

void
cseek(vlong p)
{
	vlong start;
	int delta;

	if(cbpmax < cbp)
		cbpmax = cbp;
	start = coutpos;
	if(start <= p && p <= start+(cbpmax - buf.cbuf)) {
//print("cseek %lld in [%lld,%lld] (%lld)\n", p, start, start+sizeof(buf.cbuf), cpos());
		delta = p - (start + cbp - buf.cbuf);
		cbp += delta;
		cbc -= delta;
//print("now at %lld\n", cpos());
		return;
	}

	cflush();
	seek(cout, p, 0);
	coutpos = p;
}

void
cwrite(void *buf, int n)
{
	cflush();
	if(n <= 0)
		return;
	dowrite(cout, buf, n);
	coutpos += n;
}

void
usage(void)
{
	fprint(2, "usage: %cl [options] main.%c\n", thechar, thechar);
	flagprint(2);
	exits("usage");
}

void
setheadtype(char *s)
{
	HEADTYPE = headtype(s);
}

void
setinterp(char *s)
{
	debug['I'] = 1; // denote cmdline interpreter override
	interpreter = s;
}

void
doversion(void)
{
	print("%cl version %s\n", thechar, getgoversion());
	errorexit();
}

void
genasmsym(void (*put)(Sym*, char*, int, vlong, vlong, int, Sym*))
{
	Auto *a;
	Sym *s;
	int32 off;

	// These symbols won't show up in the first loop below because we
	// skip STEXT symbols. Normal STEXT symbols are emitted by walking textp.
	s = lookup("text", 0);
	if(s->type == STEXT)
		put(s, s->name, 'T', s->value, s->size, s->version, 0);
	s = lookup("etext", 0);
	if(s->type == STEXT)
		put(s, s->name, 'T', s->value, s->size, s->version, 0);

	for(s=allsym; s!=S; s=s->allsym) {
		if(s->hide || (s->name[0] == '.' && s->version == 0 && strcmp(s->name, ".rathole") != 0))
			continue;
		switch(s->type&SMASK) {
		case SCONST:
		case SRODATA:
		case SSYMTAB:
		case SPCLNTAB:
		case SDATA:
		case SNOPTRDATA:
		case SELFROSECT:
		case SMACHOGOT:
		case STYPE:
		case SSTRING:
		case SGOSTRING:
		case SWINDOWS:
			if(!s->reachable)
				continue;
			put(s, s->name, 'D', symaddr(s), s->size, s->version, s->gotype);
			continue;

		case SBSS:
		case SNOPTRBSS:
			if(!s->reachable)
				continue;
			if(s->np > 0)
				diag("%s should not be bss (size=%d type=%d special=%d)", s->name, (int)s->np, s->type, s->special);
			put(s, s->name, 'B', symaddr(s), s->size, s->version, s->gotype);
			continue;

		case SFILE:
			put(nil, s->name, 'f', s->value, 0, s->version, 0);
			continue;
		}
	}

	for(s = textp; s != nil; s = s->next) {
		if(s->text == nil)
			continue;

		put(s, s->name, 'T', s->value, s->size, s->version, s->gotype);

		for(a=s->autom; a; a=a->link) {
			// Emit a or p according to actual offset, even if label is wrong.
			// This avoids negative offsets, which cannot be encoded.
			if(a->type != D_AUTO && a->type != D_PARAM)
				continue;
			
			// compute offset relative to FP
			if(a->type == D_PARAM)
				off = a->aoffset;
			else
				off = a->aoffset - PtrSize;
			
			// FP
			if(off >= 0) {
				put(nil, a->asym->name, 'p', off, 0, 0, a->gotype);
				continue;
			}
			
			// SP
			if(off <= -PtrSize) {
				put(nil, a->asym->name, 'a', -(off+PtrSize), 0, 0, a->gotype);
				continue;
			}
			
			// Otherwise, off is addressing the saved program counter.
			// Something underhanded is going on. Say nothing.
		}
	}
	if(debug['v'] || debug['n'])
		Bprint(&bso, "%5.2f symsize = %ud\n", cputime(), symsize);
	Bflush(&bso);
}

char*
estrdup(char *p)
{
	p = strdup(p);
	if(p == nil) {
		cursym = S;
		diag("out of memory");
		errorexit();
	}
	return p;
}

void*
erealloc(void *p, long n)
{
	p = realloc(p, n);
	if(p == nil) {
		cursym = S;
		diag("out of memory");
		errorexit();
	}
	return p;
}

// Saved history stacks encountered while reading archives.
// Keeping them allows us to answer virtual lineno -> file:line
// queries.
//
// The history stack is a complex data structure, described best at the
// bottom of http://plan9.bell-labs.com/magic/man2html/6/a.out.
// One of the key benefits of interpreting it here is that the runtime
// does not have to. Perhaps some day the compilers could generate
// a simpler linker input too.

struct Hist
{
	int32 line;
	int32 off;
	Sym *file;
};

static Hist *histcopy;
static Hist *hist;
static int32 nhist;
static int32 maxhist;
static int32 histdepth;
static int32 nhistfile;
static Sym *filesyms;

// savehist processes a single line, off history directive
// found in the input object file.
void
savehist(int32 line, int32 off)
{
	char tmp[1024];
	Sym *file;
	Hist *h;

	// NOTE(rsc): We used to do the copyhistfrog first and this
	// condition was if(tmp[0] != '\0') to check for an empty string,
	// implying that histfrogp == 0, implying that this is a history pop.
	// However, on Windows in the misc/cgo test, the linker is
	// presented with an ANAME corresponding to an empty string,
	// that ANAME ends up being the only histfrog, and thus we have
	// a situation where histfrogp > 0 (not a pop) but the path we find
	// is the empty string. Really that shouldn't happen, but it doesn't
	// seem to be bothering anyone yet, and it's easier to fix the condition
	// to test histfrogp than to track down where that empty string is
	// coming from. Probably it is coming from go tool pack's P command.
	if(histfrogp > 0) {
		tmp[0] = '\0';
		copyhistfrog(tmp, sizeof tmp);
		file = lookup(tmp, HistVersion);
		if(file->type != SFILEPATH) {
			file->value = ++nhistfile;
			file->type = SFILEPATH;
			file->next = filesyms;
			filesyms = file;
		}
	} else
		file = nil;

	if(file != nil && line == 1 && off == 0) {
		// start of new stack
		if(histdepth != 0) {
			diag("history stack phase error: unexpected start of new stack depth=%d file=%s", histdepth, tmp);
			errorexit();
		}
		nhist = 0;
		histcopy = nil;
	}
	
	if(nhist >= maxhist) {
		if(maxhist == 0)
			maxhist = 1;
		maxhist *= 2;
		hist = erealloc(hist, maxhist*sizeof hist[0]);
	}
	h = &hist[nhist++];
	h->line = line;
	h->off = off;
	h->file = file;
	
	if(file != nil) {
		if(off == 0)
			histdepth++;
	} else {
		if(off != 0) {
			diag("history stack phase error: bad offset in pop");
			errorexit();
		}
		histdepth--;
	}
}

// gethist returns the history stack currently in effect.
// The result is valid indefinitely.
Hist*
gethist(void)
{
	if(histcopy == nil) {
		if(nhist == 0)
			return nil;
		histcopy = mal((nhist+1)*sizeof hist[0]);
		memmove(histcopy, hist, nhist*sizeof hist[0]);
		histcopy[nhist].line = -1;
	}
	return histcopy;
}

typedef struct Hstack Hstack;
struct Hstack
{
	Hist *h;
	int delta;
};

// getline sets *f to the file number and *l to the line number
// of the virtual line number line according to the history stack h.
void
getline(Hist *h, int32 line, int32 *f, int32 *l)
{
	Hstack stk[100];
	int nstk, start;
	Hist *top, *h0;
	static Hist *lasth;
	static int32 laststart, lastend, lastdelta, lastfile;

	h0 = h;
	*f = 0;
	*l = 0;
	start = 0;
	if(h == nil || line == 0) {
		print("%s: getline: h=%p line=%d\n", cursym->name, h, line);
		return;
	}

	// Cache span used during last lookup, so that sequential
	// translation of line numbers in compiled code is efficient.
	if(!debug['O'] && lasth == h && laststart <= line && line < lastend) {
		*f = lastfile;
		*l = line - lastdelta;
		return;
	}

	if(debug['O'])
		print("getline %d laststart=%d lastend=%d\n", line, laststart, lastend);
	
	nstk = 0;
	for(; h->line != -1; h++) {
		if(debug['O'])
			print("\t%s %d %d\n", h->file ? h->file->name : "?", h->line, h->off);

		if(h->line > line) {
			if(nstk == 0) {
				diag("history stack phase error: empty stack at line %d", (int)line);
				errorexit();
			}
			top = stk[nstk-1].h;
			lasth = h;
			lastfile = top->file->value;
			laststart = start;
			lastend = h->line;
			lastdelta = stk[nstk-1].delta;
			*f = lastfile;
			*l = line - lastdelta;
			if(debug['O'])
				print("\tgot %d %d [%d %d %d]\n", *f, *l, laststart, lastend, lastdelta);
			return;
		}
		if(h->file == nil) {
			// pop included file
			if(nstk == 0) {
				diag("history stack phase error: stack underflow");
				errorexit();
			}
			nstk--;
			if(nstk > 0)
				stk[nstk-1].delta += h->line - stk[nstk].h->line;
			start = h->line;
		} else if(h->off == 0) {
			// push included file
			if(nstk >= nelem(stk)) {
				diag("history stack phase error: stack overflow");
				errorexit();
			}
			start = h->line;
			stk[nstk].h = h;
			stk[nstk].delta = h->line - 1;
			nstk++;
		} else {
			// #line directive
			if(nstk == 0) {
				diag("history stack phase error: stack underflow");
				errorexit();
			}
			stk[nstk-1].h = h;
			stk[nstk-1].delta = h->line - h->off;
			start = h->line;
		}
		if(debug['O'])
			print("\t\tnstk=%d delta=%d\n", nstk, stk[nstk].delta);
	}

	diag("history stack phase error: cannot find line for %d", line);
	nstk = 0;
	for(h = h0; h->line != -1; h++) {
		print("\t%d %d %s\n", h->line, h->off, h->file ? h->file->name : "");
		if(h->file == nil)
			nstk--;
		else if(h->off == 0)
			nstk++;
	}
}

// defgostring returns a symbol for the Go string containing text.
Sym*
defgostring(char *text)
{
	char *p;
	Sym *s;
	int32 n;
	
	n = strlen(text);
	p = smprint("go.string.\"%Z\"", text);
	s = lookup(p, 0);
	if(s->size == 0) {
		s->type = SGOSTRING;
		s->reachable = 1;
		s->size = 2*PtrSize+n;
		symgrow(s, 2*PtrSize+n);
		setaddrplus(s, 0, s, 2*PtrSize);
		setuintxx(s, PtrSize, n, PtrSize);
		memmove(s->p+2*PtrSize, text, n);
	}
	s->reachable = 1;
	return s;
}

// addpctab appends to f a pc-value table, storing its offset at off.
// The pc-value table is for func and reports the value of valfunc
// parameterized by arg.
static int32
addpctab(Sym *f, int32 off, Sym *func, char *desc, int32 (*valfunc)(Sym*, int32, Prog*, int32, int32), int32 arg)
{
	int32 start;
	
	start = f->np;
	funcpctab(f, func, desc, valfunc, arg);
	if(start == f->np) {
		// no table
		return setuint32(f, off, 0);
	}
	if((int32)start > (int32)f->np) {
		diag("overflow adding pc-table: symbol too large");
		errorexit();
	}
	return setuint32(f, off, start);
}

static int32
ftabaddstring(Sym *ftab, char *s)
{
	int32 n, start;
	
	n = strlen(s)+1;
	start = ftab->np;
	symgrow(ftab, start+n+1);
	strcpy((char*)ftab->p + start, s);
	return start;
}

// pclntab initializes the pclntab symbol with
// runtime function and file name information.
void
pclntab(void)
{
	Prog *p;
	int32 i, n, nfunc, start, funcstart;
	uint32 *havepc, *havefunc;
	Sym *ftab, *s;
	int32 npcdata, nfuncdata, off, end;
	int64 funcdata_bytes;
	
	funcdata_bytes = 0;
	ftab = lookup("pclntab", 0);
	ftab->type = SPCLNTAB;
	ftab->reachable = 1;

	// See golang.org/s/go12symtab for the format. Briefly:
	//	8-byte header
	//	nfunc [PtrSize bytes]
	//	function table, alternating PC and offset to func struct [each entry PtrSize bytes]
	//	end PC [PtrSize bytes]
	//	offset to file table [4 bytes]
	nfunc = 0;
	for(cursym = textp; cursym != nil; cursym = cursym->next)
		nfunc++;
	symgrow(ftab, 8+PtrSize+nfunc*2*PtrSize+PtrSize+4);
	setuint32(ftab, 0, 0xfffffffb);
	setuint8(ftab, 6, MINLC);
	setuint8(ftab, 7, PtrSize);
	setuintxx(ftab, 8, nfunc, PtrSize);

	nfunc = 0;
	for(cursym = textp; cursym != nil; cursym = cursym->next, nfunc++) {
		funcstart = ftab->np;
		funcstart += -ftab->np & (PtrSize-1);

		setaddr(ftab, 8+PtrSize+nfunc*2*PtrSize, cursym);
		setuintxx(ftab, 8+PtrSize+nfunc*2*PtrSize+PtrSize, funcstart, PtrSize);

		npcdata = 0;
		nfuncdata = 0;
		for(p = cursym->text; p != P; p = p->link) {
			if(p->as == APCDATA && p->from.offset >= npcdata)
				npcdata = p->from.offset+1;
			if(p->as == AFUNCDATA && p->from.offset >= nfuncdata)
				nfuncdata = p->from.offset+1;
		}

		// fixed size of struct, checked below
		off = funcstart;
		end = funcstart + PtrSize + 3*4 + 5*4 + npcdata*4 + nfuncdata*PtrSize;
		if(nfuncdata > 0 && (end&(PtrSize-1)))
			end += 4;
		symgrow(ftab, end);

		// entry uintptr
		off = setaddr(ftab, off, cursym);

		// name int32
		off = setuint32(ftab, off, ftabaddstring(ftab, cursym->name));
		
		// args int32
		// TODO: Move into funcinfo.
		if(cursym->text == nil)
			off = setuint32(ftab, off, ArgsSizeUnknown);
		else
			off = setuint32(ftab, off, cursym->args);
	
		// frame int32
		// TODO: Remove entirely. The pcsp table is more precise.
		// This is only used by a fallback case during stack walking
		// when a called function doesn't have argument information.
		// We need to make sure everything has argument information
		// and then remove this.
		if(cursym->text == nil)
			off = setuint32(ftab, off, 0);
		else
			off = setuint32(ftab, off, (uint32)cursym->text->to.offset+PtrSize);

		// pcsp table (offset int32)
		off = addpctab(ftab, off, cursym, "pctospadj", pctospadj, 0);

		// pcfile table (offset int32)
		off = addpctab(ftab, off, cursym, "pctofileline file", pctofileline, 0);

		// pcln table (offset int32)
		off = addpctab(ftab, off, cursym, "pctofileline line", pctofileline, 1);
		
		// npcdata int32
		off = setuint32(ftab, off, npcdata);
		
		// nfuncdata int32
		off = setuint32(ftab, off, nfuncdata);
		
		// tabulate which pc and func data we have.
		n = ((npcdata+31)/32 + (nfuncdata+31)/32)*4;
		havepc = mal(n);
		havefunc = havepc + (npcdata+31)/32;
		for(p = cursym->text; p != P; p = p->link) {
			if(p->as == AFUNCDATA) {
				if((havefunc[p->from.offset/32]>>(p->from.offset%32))&1)
					diag("multiple definitions for FUNCDATA $%d", p->from.offset);
				havefunc[p->from.offset/32] |= 1<<(p->from.offset%32);
			}
			if(p->as == APCDATA)
				havepc[p->from.offset/32] |= 1<<(p->from.offset%32);
		}

		// pcdata.
		for(i=0; i<npcdata; i++) {
			if(!(havepc[i/32]>>(i%32))&1) {
				off = setuint32(ftab, off, 0);
				continue;
			}
			off = addpctab(ftab, off, cursym, "pctopcdata", pctopcdata, i);
		}
		
		unmal(havepc, n);
		
		// funcdata, must be pointer-aligned and we're only int32-aligned.
		// Unlike pcdata, can gather in a single pass.
		// Missing funcdata will be 0 (nil pointer).
		if(nfuncdata > 0) {
			if(off&(PtrSize-1))
				off += 4;
			for(p = cursym->text; p != P; p = p->link) {
				if(p->as == AFUNCDATA) {
					i = p->from.offset;
					if(p->to.type == D_CONST)
						setuintxx(ftab, off+PtrSize*i, p->to.offset, PtrSize);
					else {
						// TODO: Dedup.
						funcdata_bytes += p->to.sym->size;
						setaddrplus(ftab, off+PtrSize*i, p->to.sym, p->to.offset);
					}
				}
			}
			off += nfuncdata*PtrSize;
		}

		if(off != end) {
			diag("bad math in functab: funcstart=%d off=%d but end=%d (npcdata=%d nfuncdata=%d)", funcstart, off, end, npcdata, nfuncdata);
			errorexit();
		}
	
		// Final entry of table is just end pc.
		if(cursym->next == nil)
			setaddrplus(ftab, 8+PtrSize+(nfunc+1)*2*PtrSize, cursym, cursym->size);
	}
	
	// Start file table.
	start = ftab->np;
	start += -ftab->np & (PtrSize-1);
	setuint32(ftab, 8+PtrSize+nfunc*2*PtrSize+PtrSize, start);

	symgrow(ftab, start+(nhistfile+1)*4);
	setuint32(ftab, start, nhistfile);
	for(s = filesyms; s != S; s = s->next)
		setuint32(ftab, start + s->value*4, ftabaddstring(ftab, s->name));

	ftab->size = ftab->np;
	
	if(debug['v'])
		Bprint(&bso, "%5.2f pclntab=%lld bytes, funcdata total %lld bytes\n", cputime(), (vlong)ftab->size, (vlong)funcdata_bytes);
}	
