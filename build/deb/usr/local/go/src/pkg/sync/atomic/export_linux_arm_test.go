// Copyright 2013 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package atomic

func generalCAS64(addr *uint64, old uint64, new uint64) bool

var GeneralCAS64 = generalCAS64
