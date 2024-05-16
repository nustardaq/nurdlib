#!/bin/sh

# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2019, 2021, 2023-2024
# Hans Toshihide Törnqvist
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

bname=$(basename $0)
dname=$(dirname $0)
[ "$CC" ] || CC=cc
for mode in release debug
do
	build_dir=build_${CC}_$($CC -dumpmachine)_$($CC -dumpversion)_$mode
	f=$dname/../$build_dir/$bname
	[ -f $f ] || f=$dname/../$build_dir/tools/$bname
	[ -f $f ] && break
done
if [ "$1" = "--gdb" ]
then
	PREFIX="gdb --args"
	shift
fi
if [ "$1" = "--valgrind" ]
then
	PREFIX="valgrind"
	shift
fi
if [ "$1" = "-h" -o "$1" = "--help" ]
then
	echo " --gdb       If first argument, invoke gdb."
	echo " --valgrind  If first argument, invoke valgrind."
fi
$PREFIX $f "$@"
