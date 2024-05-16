# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2021, 2023-2024
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

NAME:=test_ntest
DIR_$(NAME):=ntest/tests
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

include gmake/c.mk

NTEST_SRC:=$(C_$(NAME))
NTEST_DIR:=$(DIR_$(NAME))
NTEST_DEP:=$(REGGEN_OFS_ACC)
include ntest/ntest.mk
$(NTEST_SUITE_LIST): ntest/tests/rules.mk

CGEN_$(NAME):=$(NTEST_MAIN_C)

include gmake/cgen.mk
include gmake/close.mk

$(DEP_$(NAME)): ntest/ntest.h ntest/ntest.mk

$(NTEST_TARGET): $(OBJ_$(NAME)) $(NTEST_MAIN_O)
	$(NCONF_LD) &&\
	find $(BUILD_DIR) -name \*.gcda -exec rm {} \;
