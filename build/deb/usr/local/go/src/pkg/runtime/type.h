// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/*
 * Runtime type representation; master is type.go
 *
 * The Type*s here correspond 1-1 to type.go's *rtype.
 */

typedef struct Type Type;
typedef struct UncommonType UncommonType;
typedef struct InterfaceType InterfaceType;
typedef struct Method Method;
typedef struct IMethod IMethod;
typedef struct SliceType SliceType;
typedef struct FuncType FuncType;

// Needs to be in sync with ../../cmd/ld/decodesym.c:/^commonsize
struct Type
{
	uintptr size;
	uint32 hash;
	uint8 _unused;
	uint8 align;
	uint8 fieldAlign;
	uint8 kind;
	Alg *alg;
	void *gc;
	String *string;
	UncommonType *x;
	Type *ptrto;
	byte *zero;  // ptr to the zero value for this type
};

struct Method
{
	String *name;
	String *pkgPath;
	Type	*mtyp;
	Type *typ;
	void (*ifn)(void);
	void (*tfn)(void);
};

struct UncommonType
{
	String *name;
	String *pkgPath;
	Slice mhdr;
	Method m[];
};

struct IMethod
{
	String *name;
	String *pkgPath;
	Type *type;
};

struct InterfaceType
{
	Type;
	Slice mhdr;
	IMethod m[];
};

struct MapType
{
	Type;
	Type *key;
	Type *elem;
	Type *bucket; // internal type representing a hash bucket
	Type *hmap;   // internal type representing a Hmap
};

struct ChanType
{
	Type;
	Type *elem;
	uintptr dir;
};

struct SliceType
{
	Type;
	Type *elem;
};

struct FuncType
{
	Type;
	bool dotdotdot;
	Slice in;
	Slice out;
};

struct PtrType
{
	Type;
	Type *elem;
};
