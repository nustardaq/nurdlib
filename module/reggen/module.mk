# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2017, 2021, 2023-2024
# Bastian Löher
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

# Makefile snippet for simple generation of register code.

OFS_$(NAME):=$(BUILD_DIR)/$(DIR_$(NAME))/offsets$(REGGEN_SUFFIX).h
REGGEN_OFS_ACC:=$(REGGEN_OFS_ACC) $(OFS_$(NAME))

$(DEP_$(NAME)): $(OFS_$(NAME))

$(BUILD_DIR)/$(DIR_$(NAME))/registerlist$(REGGEN_SUFFIX).c: $(OFS_$(NAME))

$(OFS_$(NAME)): $(DIR_$(NAME))/register_list$(REGGEN_SUFFIX).txt $(BUILD_DIR)/module/reggen/reggen module/reggen/module.mk
	$(QUIET)echo "REG   $@" &&\
	$(MKDIR) &&\
	./$(BUILD_DIR)/module/reggen/reggen $(@D) $<

REGGEN_SUFFIX=
