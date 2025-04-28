" nurdlib, NUstar ReaDout LIBrary
"
" Copyright (C) 2025
" Hans Toshihide Toernqvist <hans.tornqvist@chalmers.se>
"
" This library is free software; you can redistribute it and/or
" modify it under the terms of the GNU Lesser General Public
" License as published by the Free Software Foundation; either
" version 2.1 of the License, or (at your option) any later version.
"
" This library is distributed in the hope that it will be useful,
" but WITHOUT ANY WARRANTY; without even the implied warranty of
" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
" Lesser General Public License for more details.
"
" You should have received a copy of the GNU Lesser General Public
" License along with this library; if not, write to the Free Software
" Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
" MA  02110-1301  USA

" Vim syntax file
" Language:	    nurdlib
" Maintainer:	    Hans Toernqvist <hans.tornqvist@chalmers.se>
" Latest Revision:  2025-04-28

if exists("b:current_syntax")
  finish
endif

syn match nurdlibComment "#.*$"
syn match nurdlibComment "\/\/.*$"
syn match nurdlibDelimiter "[=(){}]"
syn match nurdlibNumber "\<\d\+\>"
syn match nurdlibNumber "\<0x[0-9a-f]\+\>"
syn match nurdlibString "\"[^\"]*\""

syn keyword nurdlibFunctions

hi def link nurdlibComment Comment
hi def link nurdlibDelimiter Special
hi def link nurdlibFunctions Statement
hi def link nurdlibNumber Number
hi def link nurdlibString String

let b:current_syntax = "nurdlib"
