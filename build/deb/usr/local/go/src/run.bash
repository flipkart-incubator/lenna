#!/usr/bin/env bash
# Copyright 2009 The Go Authors. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

set -e

eval $(go env)

unset CDPATH	# in case user has it set
unset GOPATH    # we disallow local import for non-local packages, if $GOROOT happens
                # to be under $GOPATH, then some tests below will fail

# no core files, please
ulimit -c 0

# Raise soft limits to hard limits for NetBSD/OpenBSD.
# We need at least 256 files and ~300 MB of bss.
# On OS X ulimit -S -n rejects 'unlimited'.
[ "$(ulimit -H -n)" == "unlimited" ] || ulimit -S -n $(ulimit -H -n)
[ "$(ulimit -H -d)" == "unlimited" ] || ulimit -S -d $(ulimit -H -d)

# Thread count limit on NetBSD 7.
if ulimit -T &> /dev/null; then
	[ "$(ulimit -H -T)" == "unlimited" ] || ulimit -S -T $(ulimit -H -T)
fi

# allow all.bash to avoid double-build of everything
rebuild=true
if [ "$1" = "--no-rebuild" ]; then
	shift
else
	echo '# Building packages and commands.'
	time go install -a -v std
	echo
fi

# we must unset GOROOT_FINAL before tests, because runtime/debug requires
# correct access to source code, so if we have GOROOT_FINAL in effect,
# at least runtime/debug test will fail.
unset GOROOT_FINAL

# increase timeout for ARM up to 3 times the normal value
timeout_scale=1
[ "$GOARCH" == "arm" ] && timeout_scale=3

echo '# Testing packages.'
time go test std -short -timeout=$(expr 120 \* $timeout_scale)s
echo

echo '# GOMAXPROCS=2 runtime -cpu=1,2,4'
GOMAXPROCS=2 go test runtime -short -timeout=$(expr 300 \* $timeout_scale)s -cpu=1,2,4
echo

echo '# sync -cpu=10'
go test sync -short -timeout=$(expr 120 \* $timeout_scale)s -cpu=10

# Race detector only supported on Linux and OS X,
# and only on amd64, and only when cgo is enabled.
case "$GOHOSTOS-$GOOS-$GOARCH-$CGO_ENABLED" in
linux-linux-amd64-1 | darwin-darwin-amd64-1)
	echo
	echo '# Testing race detector.'
	go test -race -i runtime/race flag
	go test -race -run=Output runtime/race
	go test -race -short flag
esac

xcd() {
	echo
	echo '#' $1
	builtin cd "$GOROOT"/src/$1 || exit 1
}

# NOTE: "set -e" cannot help us in subshells. It works until you test it with ||.
#
#	$ bash --version
#	GNU bash, version 3.2.48(1)-release (x86_64-apple-darwin12)
#	Copyright (C) 2007 Free Software Foundation, Inc.
#
#	$ set -e; (set -e; false; echo still here); echo subshell exit status $?
#	subshell exit status 1
#	# subshell stopped early, set exit status, but outer set -e didn't stop.
#
#	$ set -e; (set -e; false; echo still here) || echo stopped
#	still here
#	# somehow the '|| echo stopped' broke the inner set -e.
#	
# To avoid this bug, every command in a subshell should have '|| exit 1' on it.
# Strictly speaking, the test may be unnecessary on the final command of
# the subshell, but it aids later editing and may avoid future bash bugs.

[ "$CGO_ENABLED" != 1 ] ||
[ "$GOHOSTOS" == windows ] ||
(xcd ../misc/cgo/stdio
go run $GOROOT/test/run.go - . || exit 1
) || exit $?

[ "$CGO_ENABLED" != 1 ] ||
(xcd ../misc/cgo/life
go run $GOROOT/test/run.go - . || exit 1
) || exit $?

[ "$CGO_ENABLED" != 1 ] ||
(xcd ../misc/cgo/test
go test -ldflags '-linkmode=auto' || exit 1
# linkmode=internal fails on dragonfly since errno is a TLS relocation.
[ "$GOHOSTOS" == dragonfly ] || go test -ldflags '-linkmode=internal' || exit 1
case "$GOHOSTOS-$GOARCH" in
openbsd-386 | openbsd-amd64)
	# test linkmode=external, but __thread not supported, so skip testtls.
	go test -ldflags '-linkmode=external' || exit 1
	;;
darwin-386 | darwin-amd64)
	# linkmode=external fails on OS X 10.6 and earlier == Darwin
	# 10.8 and earlier.
	case $(uname -r) in
	[0-9].* | 10.*) ;;
	*) go test -ldflags '-linkmode=external'  || exit 1;;
	esac
	;;
dragonfly-386 | dragonfly-amd64 | freebsd-386 | freebsd-amd64 | linux-386 | linux-amd64 | linux-arm | netbsd-386 | netbsd-amd64)
	go test -ldflags '-linkmode=external' || exit 1
	go test -ldflags '-linkmode=auto' ../testtls || exit 1
	go test -ldflags '-linkmode=external' ../testtls || exit 1
esac
) || exit $?

# This tests cgo -godefs. That mode is not supported,
# so it's okay if it doesn't work on some systems.
# In particular, it works badly with clang on OS X.
[ "$CGO_ENABLED" != 1 ] || [ "$GOOS" == darwin ] ||
(xcd ../misc/cgo/testcdefs
./test.bash || exit 1
) || exit $?

[ "$CGO_ENABLED" != 1 ] ||
[ "$GOHOSTOS" == windows ] ||
(xcd ../misc/cgo/testso
./test.bash || exit 1
) || exit $?

[ "$CGO_ENABLED" != 1 ] ||
[ "$GOHOSTOS-$GOARCH" != linux-amd64 ] ||
(xcd ../misc/cgo/testasan
go run main.go || exit 1
) || exit $?

[ "$CGO_ENABLED" != 1 ] ||
[ "$GOHOSTOS" == windows ] ||
(xcd ../misc/cgo/errors
./test.bash || exit 1
) || exit $?

(xcd ../doc/progs
time ./run || exit 1
) || exit $?

[ "$GOARCH" == arm ] ||  # uses network, fails under QEMU
(xcd ../doc/articles/wiki
make clean || exit 1
./test.bash || exit 1
) || exit $?

(xcd ../doc/codewalk
time ./run || exit 1
) || exit $?

echo
echo '#' ../misc/goplay
go build ../misc/goplay
rm -f goplay

[ "$GOARCH" == arm ] ||
(xcd ../test/bench/shootout
./timing.sh -test || exit 1
) || exit $?

[ "$GOOS" == openbsd ] || # golang.org/issue/5057
(
echo
echo '#' ../test/bench/go1
go test ../test/bench/go1 || exit 1
) || exit $?

(xcd ../test
unset GOMAXPROCS
time go run run.go || exit 1
) || exit $?

echo
echo '# Checking API compatibility.'
time go run $GOROOT/src/cmd/api/run.go

echo
echo ALL TESTS PASSED
