# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2018, 2021, 2023-2024
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

NAME:=ctrl
DIR_$(NAME):=ctrl
C_$(NAME):=ctrl/ctrl.c

include gmake/c.mk
include gmake/close.mk

NAME:=nurdctrl
DIR_$(NAME):=ctrl
C_$(NAME):=ctrl/nurdctrl.c

include gmake/c.mk
include gmake/close.mk

$(NURDCTRL_TARGET): $(BUILD_DIR)/ctrl/nurdctrl.o $(NURDLIB_TARGET) bin/nurdctrl $(HWMAP_ERROR_INTERNAL_O)
	$(NCONF_LD)
