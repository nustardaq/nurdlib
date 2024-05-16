# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2017, 2023-2024
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

# Makefile snippet for register generating things.

OFS_$(NAME):=$(BUILD_DIR)/$(DIR_$(NAME))/offsets$(REGGEN_SUFFIX).h
REGGEN_OFS_ACC:=$(REGGEN_OFS_ACC) $(OFS_$(NAME))

$(DEP_$(NAME)): $(OFS_$(NAME))

$(BUILD_DIR)/$(DIR_$(NAME))/offsets.h: $(DIR_$(NAME))/register_list2.txt $(BUILD_DIR)/module/reggen2/reggen2 $(DIR_$(NAME))/rules.mk module/reggen2/module.mk
	$(QUIET)echo "REG2  $@" && \
	$(MKDIR) && \
	./$(BUILD_DIR)/module/reggen2/reggen2 --offsets $(word 3,$(subst /, ,$(@D))) < $< > $@.tmp && \
	mv -f $@.tmp $@

$(BUILD_DIR)/$(DIR_$(NAME))/init.c: $(DIR_$(NAME))/register_list2.txt $(BUILD_DIR)/module/reggen2/reggen2 $(DIR_$(NAME))/rules.mk module/reggen2/module.mk
	$(QUIET)echo "REG2  $@" && \
	$(MKDIR) && \
	./$(BUILD_DIR)/module/reggen2/reggen2 --functions $(word 3,$(subst /, ,$(@D))) < $< > $@.tmp && \
	mv -f $@.tmp $@
