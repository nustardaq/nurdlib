# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2021-2022, 2024
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

# Common transformations and rules, after all other gmake/*.mk!

# Check if $(NAME) was already used.
ifneq (,$(DUP_$(NAME)))
 $(error NAME=$(NAME) already used!)
endif
DUP_$(NAME)=1

TARGETS:=$(TARGETS) $(NAME)

DEP_$(NAME):=$(filter %.d,$(OBJ_$(NAME):.o=.d))

$(DEP_$(NAME)): $(FLEX_$(NAME)_C) $(BISON_$(NAME)_C)
