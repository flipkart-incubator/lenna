// Copyright 2012 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package race_test

import (
	"testing"
)

func TestRaceMapRW(t *testing.T) {
	m := make(map[int]int)
	ch := make(chan bool, 1)
	go func() {
		_ = m[1]
		ch <- true
	}()
	m[1] = 1
	<-ch
}

func TestRaceMapRW2(t *testing.T) {
	m := make(map[int]int)
	ch := make(chan bool, 1)
	go func() {
		_, _ = m[1]
		ch <- true
	}()
	m[1] = 1
	<-ch
}

func TestRaceMapRWArray(t *testing.T) {
	// Check instrumentation of unaddressable arrays (issue 4578).
	m := make(map[int][2]int)
	ch := make(chan bool, 1)
	go func() {
		_ = m[1][1]
		ch <- true
	}()
	m[2] = [2]int{1, 2}
	<-ch
}

func TestNoRaceMapRR(t *testing.T) {
	m := make(map[int]int)
	ch := make(chan bool, 1)
	go func() {
		_, _ = m[1]
		ch <- true
	}()
	_ = m[1]
	<-ch
}

func TestRaceMapRange(t *testing.T) {
	m := make(map[int]int)
	ch := make(chan bool, 1)
	go func() {
		for _ = range m {
		}
		ch <- true
	}()
	m[1] = 1
	<-ch
}

func TestRaceMapRange2(t *testing.T) {
	m := make(map[int]int)
	ch := make(chan bool, 1)
	go func() {
		for _ = range m {
		}
		ch <- true
	}()
	m[1] = 1
	<-ch
}

func TestNoRaceMapRangeRange(t *testing.T) {
	m := make(map[int]int)
	// now the map is not empty and range triggers an event
	// should work without this (as in other tests)
	// so it is suspicious if this test passes and others don't
	m[0] = 0
	ch := make(chan bool, 1)
	go func() {
		for _ = range m {
		}
		ch <- true
	}()
	for _ = range m {
	}
	<-ch
}

func TestRaceMapLen(t *testing.T) {
	m := make(map[string]bool)
	ch := make(chan bool, 1)
	go func() {
		_ = len(m)
		ch <- true
	}()
	m[""] = true
	<-ch
}

func TestRaceMapDelete(t *testing.T) {
	m := make(map[string]bool)
	ch := make(chan bool, 1)
	go func() {
		delete(m, "")
		ch <- true
	}()
	m[""] = true
	<-ch
}

func TestRaceMapLenDelete(t *testing.T) {
	m := make(map[string]bool)
	ch := make(chan bool, 1)
	go func() {
		delete(m, "a")
		ch <- true
	}()
	_ = len(m)
	<-ch
}

func TestRaceMapVariable(t *testing.T) {
	ch := make(chan bool, 1)
	m := make(map[int]int)
	go func() {
		m = make(map[int]int)
		ch <- true
	}()
	m = make(map[int]int)
	<-ch
}

func TestRaceMapVariable2(t *testing.T) {
	ch := make(chan bool, 1)
	m := make(map[int]int)
	go func() {
		m[1] = 1
		ch <- true
	}()
	m = make(map[int]int)
	<-ch
}

func TestRaceMapVariable3(t *testing.T) {
	ch := make(chan bool, 1)
	m := make(map[int]int)
	go func() {
		_ = m[1]
		ch <- true
	}()
	m = make(map[int]int)
	<-ch
}

type Big struct {
	x [17]int32
}

func TestRaceMapLookupPartKey(t *testing.T) {
	k := &Big{}
	m := make(map[Big]bool)
	ch := make(chan bool, 1)
	go func() {
		k.x[8] = 1
		ch <- true
	}()
	_ = m[*k]
	<-ch
}

func TestRaceMapLookupPartKey2(t *testing.T) {
	k := &Big{}
	m := make(map[Big]bool)
	ch := make(chan bool, 1)
	go func() {
		k.x[8] = 1
		ch <- true
	}()
	_, _ = m[*k]
	<-ch
}
func TestRaceMapDeletePartKey(t *testing.T) {
	k := &Big{}
	m := make(map[Big]bool)
	ch := make(chan bool, 1)
	go func() {
		k.x[8] = 1
		ch <- true
	}()
	delete(m, *k)
	<-ch
}

func TestRaceMapInsertPartKey(t *testing.T) {
	k := &Big{}
	m := make(map[Big]bool)
	ch := make(chan bool, 1)
	go func() {
		k.x[8] = 1
		ch <- true
	}()
	m[*k] = true
	<-ch
}

func TestRaceMapInsertPartVal(t *testing.T) {
	v := &Big{}
	m := make(map[int]Big)
	ch := make(chan bool, 1)
	go func() {
		v.x[8] = 1
		ch <- true
	}()
	m[1] = *v
	<-ch
}

// Test for issue 7561.
func TestRaceMapAssignMultipleReturn(t *testing.T) {
	connect := func() (int, error) { return 42, nil }
	conns := make(map[int][]int)
	conns[1] = []int{0}
	ch := make(chan bool, 1)
	var err error
	go func() {
		conns[1][0], err = connect()
		ch <- true
	}()
	x := conns[1][0]
	_ = x
	<-ch
}
