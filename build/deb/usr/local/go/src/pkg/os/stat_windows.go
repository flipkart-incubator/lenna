// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package os

import (
	"syscall"
	"unsafe"
)

// Stat returns the FileInfo structure describing file.
// If there is an error, it will be of type *PathError.
func (file *File) Stat() (fi FileInfo, err error) {
	if file == nil {
		return nil, ErrInvalid
	}
	if file == nil || file.fd < 0 {
		return nil, syscall.EINVAL
	}
	if file.isdir() {
		// I don't know any better way to do that for directory
		return Stat(file.name)
	}
	if file.name == DevNull {
		return &devNullStat, nil
	}
	var d syscall.ByHandleFileInformation
	e := syscall.GetFileInformationByHandle(syscall.Handle(file.fd), &d)
	if e != nil {
		return nil, &PathError{"GetFileInformationByHandle", file.name, e}
	}
	return &fileStat{
		name: basename(file.name),
		sys: syscall.Win32FileAttributeData{
			FileAttributes: d.FileAttributes,
			CreationTime:   d.CreationTime,
			LastAccessTime: d.LastAccessTime,
			LastWriteTime:  d.LastWriteTime,
			FileSizeHigh:   d.FileSizeHigh,
			FileSizeLow:    d.FileSizeLow,
		},
		vol:   d.VolumeSerialNumber,
		idxhi: d.FileIndexHigh,
		idxlo: d.FileIndexLow,
	}, nil
}

// Stat returns a FileInfo structure describing the named file.
// If there is an error, it will be of type *PathError.
func Stat(name string) (fi FileInfo, err error) {
	if len(name) == 0 {
		return nil, &PathError{"Stat", name, syscall.Errno(syscall.ERROR_PATH_NOT_FOUND)}
	}
	if name == DevNull {
		return &devNullStat, nil
	}
	fs := &fileStat{name: basename(name)}
	namep, e := syscall.UTF16PtrFromString(name)
	if e != nil {
		return nil, &PathError{"Stat", name, e}
	}
	e = syscall.GetFileAttributesEx(namep, syscall.GetFileExInfoStandard, (*byte)(unsafe.Pointer(&fs.sys)))
	if e != nil {
		return nil, &PathError{"GetFileAttributesEx", name, e}
	}
	fs.path = name
	if !isAbs(fs.path) {
		cwd, _ := Getwd()
		fs.path = cwd + `\` + fs.path
	}
	return fs, nil
}

// Lstat returns the FileInfo structure describing the named file.
// If the file is a symbolic link, the returned FileInfo
// describes the symbolic link.  Lstat makes no attempt to follow the link.
// If there is an error, it will be of type *PathError.
func Lstat(name string) (fi FileInfo, err error) {
	// No links on Windows
	return Stat(name)
}

// basename removes trailing slashes and the leading
// directory name and drive letter from path name.
func basename(name string) string {
	// Remove drive letter
	if len(name) == 2 && name[1] == ':' {
		name = "."
	} else if len(name) > 2 && name[1] == ':' {
		name = name[2:]
	}
	i := len(name) - 1
	// Remove trailing slashes
	for ; i > 0 && (name[i] == '/' || name[i] == '\\'); i-- {
		name = name[:i]
	}
	// Remove leading directory name
	for i--; i >= 0; i-- {
		if name[i] == '/' || name[i] == '\\' {
			name = name[i+1:]
			break
		}
	}
	return name
}

func isSlash(c uint8) bool {
	return c == '\\' || c == '/'
}

func isAbs(path string) (b bool) {
	v := volumeName(path)
	if v == "" {
		return false
	}
	path = path[len(v):]
	if path == "" {
		return false
	}
	return isSlash(path[0])
}

func volumeName(path string) (v string) {
	if len(path) < 2 {
		return ""
	}
	// with drive letter
	c := path[0]
	if path[1] == ':' &&
		('0' <= c && c <= '9' || 'a' <= c && c <= 'z' ||
			'A' <= c && c <= 'Z') {
		return path[:2]
	}
	// is it UNC
	if l := len(path); l >= 5 && isSlash(path[0]) && isSlash(path[1]) &&
		!isSlash(path[2]) && path[2] != '.' {
		// first, leading `\\` and next shouldn't be `\`. its server name.
		for n := 3; n < l-1; n++ {
			// second, next '\' shouldn't be repeated.
			if isSlash(path[n]) {
				n++
				// third, following something characters. its share name.
				if !isSlash(path[n]) {
					if path[n] == '.' {
						break
					}
					for ; n < l; n++ {
						if isSlash(path[n]) {
							break
						}
					}
					return path[:n]
				}
				break
			}
		}
	}
	return ""
}
