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

QUIET=@

include ../../gmake/build_dir.mk

TEST:=../../$(BUILD_DIR)/etherbone_test

CPPFLAGS:=-I../../include -I../.. -I../../$(BUILD_DIR) -I../etherbone/include
LDFLAGS:=-L../../$(BUILD_DIR) -L../etherbone/lib -L../cmvlc
LIBS:=-lnurdlib -letherbone -lcmvlc -lm -pthread

.PHONY: all
all: $(TEST)
	$(QUIET)echo TEST $<; \
	./$<

$(TEST): ../etherbone/test.c Makefile
	$(QUIET)echo LD $@; \
	$(CC) $(CPPFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)
