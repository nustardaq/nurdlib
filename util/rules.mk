# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2019, 2021, 2023-2024
# Bastian Löher
# Michael Munch
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

NAME:=util
DIR_$(NAME):=util
C_$(NAME):=$(filter-out util/md5%,$(wildcard $(DIR_$(NAME))/*.c))

support_libmoto:=$(shell $(CC) -c util/libmotovec_memcpy.S -mregnames -o /dev/null 2> /dev/null && echo yes)
ifeq (yes,$(support_libmoto))
 S_$(NAME)+=util/libmotovec_memcpy.S
endif

include gmake/s.mk
include gmake/c.mk
include gmake/close.mk

$(BUILD_DIR)/util/md5sum.h: $(BUILD_DIR)/md5summer config/config.c config/kwarray.h ctrl/ctrl.c ctrl/ctrl.h
	$(QUIET)echo "MD5   $@" &&\
	echo "#ifndef UTIL_MD5SUM_H" > $@.tmp &&\
	echo "#define UTIL_MD5SUM_H" >> $@.tmp &&\
	echo "#define NURDLIB_MD5 0x$(shell cat $(filter-out %md5summer,$^) | $<)" >> $@.tmp &&\
	echo "#endif" >> $@.tmp &&\
	mv -f $@.tmp $@

$(BUILD_DIR)/util/md5.o: util/md5.c Makefile util/rules.mk
	$(QUIET)echo "CC    $@" &&\
	$(MKDIR) &&\
	$(CC) -c -o $@ $< -I. $(CFLAGS_)

$(BUILD_DIR)/util/md5summer.o: util/md5summer.c Makefile util/rules.mk
	$(QUIET)echo "CC    $@" &&\
	$(MKDIR) &&\
	$(CC) -c -o $@ $< -I. $(CFLAGS_)

$(BUILD_DIR)/md5summer: $(BUILD_DIR)/util/md5summer.o $(BUILD_DIR)/util/md5.o
	$(QUIET)echo "LD    $@" &&\
	$(CC) -o $@ $^

$(BUILD_DIR)/ctrl/ctrl.d: $(BUILD_DIR)/util/md5sum.h
$(BUILD_DIR)/util/pack.d: $(BUILD_DIR)/util/md5sum.h

$(VIMSYN): util/nurdlib.vim config/kwarray.h util/rules.mk
	$(QUIET)echo "VIM   $@" &&\
	$(MKDIR) &&\
	list=$$($(SED) -n 's/.*\"\([A-Za-z].*\)\".*/\1/p' config/kwarray.h | tr '\n' ' ') &&\
	$(SED) "s/\(nurdlibFunctions\)$$/\1 $$list/" util/nurdlib.vim > $@.tmp2 &&\
	mv $@.tmp2 $@ &&\
	d=$$HOME/.vim/syntax &&\
	[ -d $$d ] || mkdir -p $$d;\
	cp $@ $$d
