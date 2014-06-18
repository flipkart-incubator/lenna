// Copyright 2013 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// +build race

#include "../../cmd/ld/textflag.h"

// func runtime·racefuncenter(pc uintptr)
TEXT	runtime·racefuncenter(SB), NOSPLIT, $16-8
	MOVQ	DX, saved-8(SP) // save function entry context (for closures)
	MOVQ	pc+0(FP), DX
	MOVQ	DX, arg-16(SP)
	CALL	runtime·racefuncenter1(SB)
	MOVQ	saved-8(SP), DX
	RET
