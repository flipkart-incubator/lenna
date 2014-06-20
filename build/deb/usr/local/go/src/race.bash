#!/usr/bin/env bash
# Copyright 2013 The Go Authors. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# race.bash tests the standard library under the race detector.
# http://golang.org/doc/articles/race_detector.html

set -e

function usage {
	echo 'race detector is only supported on linux/amd64 and darwin/amd64' 1>&2
	exit 1
}

case $(uname) in
"Darwin")
	# why Apple? why?
	if sysctl machdep.cpu.extfeatures | grep -qv EM64T; then
		usage
	fi 
	;;
"Linux")
	if [ $(uname -m) != "x86_64" ]; then
		usage
	fi
	;;
*)
	usage
	;;
esac

if [ ! -f make.bash ]; then
	echo 'race.bash must be run from $GOROOT/src' 1>&2
	exit 1
fi
. ./make.bash --no-banner
# golang.org/issue/5537 - we must build a race enabled cmd/cgo before trying to use it.
go install -race cmd/cgo
go install -race std

# we must unset GOROOT_FINAL before tests, because runtime/debug requires
# correct access to source code, so if we have GOROOT_FINAL in effect,
# at least runtime/debug test will fail.
unset GOROOT_FINAL

go test -race -short std
go test -race -run=nothingplease -bench=.* -benchtime=.1s -cpu=4 std
