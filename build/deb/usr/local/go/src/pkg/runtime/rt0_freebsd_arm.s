// Copyright 2012 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "../../cmd/ld/textflag.h"

// FreeBSD and Linux use the same linkage to main

TEXT _rt0_arm_freebsd(SB),NOSPLIT,$-4
	MOVW	(R13), R0	// argc
	MOVW	$4(R13), R1		// argv
	MOVM.DB.W [R0-R1], (R13)
	B	_rt0_go(SB)
