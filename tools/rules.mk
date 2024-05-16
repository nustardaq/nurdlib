# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2016-2018, 2021, 2023-2024
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

NAME:=tools
DIR_$(NAME):=tools
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

include gmake/c.mk
include gmake/close.mk

$(MEMTEST_TARGET): $(BUILD_DIR)/tools/memtest.o $(NURDLIB_TARGET) bin/memtest $(HWMAP_ERROR_INTERNAL_O)
	$(NCONF_LD)

$(RWDUMP_TARGET): $(BUILD_DIR)/tools/rwdump.o $(NURDLIB_TARGET) bin/rwdump $(HWMAP_ERROR_INTERNAL_O)
	$(NCONF_LD)

$(WRSLEW_TARGET): $(BUILD_DIR)/tools/wrslew.o $(NURDLIB_TARGET) bin/wrslew $(HWMAP_ERROR_INTERNAL_O)
	$(NCONF_LD)

$(DEP_$(NAME)): $(BUILD_DIR)/module/genlist.h
