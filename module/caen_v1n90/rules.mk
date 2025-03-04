# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2017, 2021, 2024
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

NAME:=caen_v1n90
DIR_$(NAME):=module/$(NAME)
C_$(NAME):=$(addprefix $(DIR_$(NAME))/,caen_v1n90.c micro.c)

include gmake/c.mk
include gmake/close.mk
include module/reggen/module.mk

$(BUILD_DIR)/$(DIR_$(NAME))/microdumper: $(BUILD_DIR)/$(DIR_$(NAME))/microdump.o $(BUILD_DIR)/$(DIR_$(NAME))/micro.o $(BUILD_DIR)/module/libmodule.a $(BUILD_DIR)/config/libconfig.a $(BUILD_DIR)/util/libutil.a
	$(NCONF_LD)
