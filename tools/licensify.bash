#!/bin/bash

# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2024
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

LC_ALL=C

cd $(dirname $0)/..

function author {
	nick="$1"
	full="$2"
	if [ -n "$(echo $auths_f | grep -E $nick)" ]
	then
		authored=1
		return
	fi
	if [ -n "$(echo $auths_git | grep -E $nick)" ]
	then
		authored=1
		echo "$prefix $full" >> $i.tmp
	fi
}

function print_years {
	if [ -n "$first_year" ]
	then
		first_year=
	else
		list+=", "
	fi
	if [ $y0 -eq $y1 ]
	then
		list+="$y0"
	else
		list+="$y0-$y1"
	fi
}

for i in $(git ls-files)
do
	echo $i...

	# Skip some files.
	[ $i = "COPYING" -o \
	  $i = "README" -o \
	  $i = "mock/README" -o \
	  $i = "doc/web/README" -o \
	  $i = "util/libmotovec_memcpy.S" -o \
	  $i = "util/md5.c" -o \
	  $i = "util/md5.h" -o \
	  $i = "util/queue.h" -o \
	  $i = "util/strlcat.c" -o \
	  $i = "util/strlcpy.c" -o \
	  $i = "doc/web/source/conf.py" ] && continue

	# Skip register-lists, can be copied from manuals.
	[ -n "$(echo $i | grep register_list)" ] && continue

	ext=$(echo $i | sed 's,.*\.,,')
	header=
	footer=
	if [ $ext = "c" -o $ext = "h" -o $ext = "l" -o $ext = "y" ]
	then
		header="/*
"
		prefix=" *"
		footer="
 */"
	elif [ $ext = "svg" ]
	then
		header="<!--
"
		prefix=
		footer="
-->"
	elif [ $ext = "tex" ]
	then
		prefix="%"
	else
		prefix="#"
	fi

	cr=$(head -n10 $i | grep "Copyright (C)")

	if [ -z "$cr" ]
	then
		# No copyright.
		cat > $i.tmp << EOF
$header$prefix nurdlib, NUstar ReaDout LIBrary
$prefix
$prefix Copyright (C)
$prefix
$prefix This library is free software; you can redistribute it and/or
$prefix modify it under the terms of the GNU Lesser General Public
$prefix License as published by the Free Software Foundation; either
$prefix version 2.1 of the License, or (at your option) any later version.
$prefix
$prefix This library is distributed in the hope that it will be useful,
$prefix but WITHOUT ANY WARRANTY; without even the implied warranty of
$prefix MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
$prefix Lesser General Public License for more details.
$prefix
$prefix You should have received a copy of the GNU Lesser General Public
$prefix License along with this library; if not, write to the Free Software
$prefix Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
$prefix MA  02110-1301  USA$footer

EOF
		cat $i >> $i.tmp
		cat $i.tmp > $i && rm $i.tmp
	fi

	# Check years.
	years=$(git log $i | grep Date | awk '{print $6}')
	status=$(git status $i)
	cur_year="
$(date +%Y)"
	[ -n "$(echo $status | grep 'modified:')" ] && years+=$cur_year
	[ -n "$(echo $status | grep 'new file:')" ] && years+=$cur_year
	years=$(echo "$years" | sort -u)
	list=
	first_year=1
	y0=
	y1=
	for y in $years
	do
		if [ -z "$y0" ]
		then
			y0=$y
			y1=$y
		elif [ $y -eq $((y1+1)) ]
		then
			y1=$y
		else
			print_years
			y0=$y
			y1=$y
		fi
	done
	print_years
	sed '1,10s/Copyright (C).*/Copyright (C) '"$list"'/' $i > $i.tmp
	cat $i.tmp > $i && rm $i.tmp

	# Check and add authors, don't remove.
	# All authors ever: git shortlog -sne
	authored=
	auths_f=
	auths_git=$(git log $i | grep Author: | sort -u)
	linen=1
	linen0=
	while read -r line
	do
		if [ "$(echo $line | grep 'Copyright (C)')" ]
		then
			linen0=$linen
		elif [ -n "$linen0" ]
		then
			if [ $(echo $line | wc -c) -lt 5 ]
			then
				break
			fi
			auths_f+="$(echo $line | sed 's/^[^A-Za-z]*//')
"
		fi
		((++linen))
	done < $i
	head -n$((linen-1)) $i > $i.tmp
	author Basti "Bastian Löher"
	author Weber "Günter Weber"
	author "Johansson|HTJ" "Håkan T Johansson"
	author Xarepe "Manuel Xarepe"
	author "Munch|munk" "Michael Munch"
	author Papst "Oliver Papst"
	author Pietri "Stephane Pietri"
	author rnqvist "Hans Toshihide Törnqvist"
	[ -z "$authored" ] && echo "$prefix Hans Toshihide Törnqvist" >> $i.tmp
	tail -n+$linen $i >> $i.tmp
	cat $i.tmp > $i && rm $i.tmp
done
