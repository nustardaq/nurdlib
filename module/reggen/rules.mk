# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2017, 2021, 2023-2024
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

NAME:=reggen
DIR_$(NAME):=module/reggen
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

include gmake/c.mk
include gmake/close.mk

$(BUILD_DIR)/$(DIR_$(NAME))/reggen: $(OBJ_$(NAME)) $(addprefix $(BUILD_DIR)/util/,strlcat.o strlcpy.o)
	$(NCONF_LD)
