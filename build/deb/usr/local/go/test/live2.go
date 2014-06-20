// errorcheck -0 -live

// Copyright 2014 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// liveness tests with inlining ENABLED
// see also live.go.

package main

// issue 8142: lost 'addrtaken' bit on inlined variables.
// no inlining in this test, so just checking that non-inlined works.

type T40 struct {
	m map[int]int
}

func newT40() *T40 {
	ret := T40{ // ERROR "live at call to makemap: &ret"
		make(map[int]int),
	}
	return &ret
}

func bad40() {
	t := newT40() // ERROR "live at call to makemap: ret"
	println()     // ERROR "live at call to printnl: ret"
	_ = t
}

func good40() {
	ret := T40{ // ERROR "live at call to makemap: ret"
		make(map[int]int),
	}
	t := &ret
	println() // ERROR "live at call to printnl: ret"
	_ = t
}
