# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2018-2024
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

NAME:=gsi_etherbone
DIR_$(NAME):=module/$(NAME)
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

ifeq (Linux,$(GSI_OS))

ifeq (,$(WR_SYS))
# WR_SYS not set? List WR releases by increasing age.
WR_RELEASES=fallout enigma cherry balloon
WR_SYSES:=$(patsubst %,/mbs/driv/white_rabbit/%/$(GSI_CPU_PLATFORM)_$(GSI_OS)_$(GSI_OS_VERSIONX)_$(GSI_OS_TYPE),$(WR_RELEASES))
WR_SYS:=$(firstword $(wildcard $(WR_SYSES)))
export WR_SYS
endif

$(BUILD_DIR)/replacements/wb_vendors.h: Makefile $(DIR_$(NAME))/rules.mk
	$(QUIET)$(REPL) &&\
	$(MKDIR) &&\
	src=$(WR_SYS)/include/wb_devices/wb_vendors.h;\
	if [ -f $$src ]; then\
		$(SED) "s#//\(vendor IDs\)#/* \1 */#" $$src > $@.tmp && \
		mv -f $@.tmp $@;\
	else\
		touch $@;\
	fi

$(BUILD_DIR)/replacements/gsi_tm_latch.h: Makefile $(DIR_$(NAME))/rules.mk
	$(QUIET)$(REPL) &&\
	$(MKDIR) &&\
	src=$(WR_SYS)/include/wb_devices/gsi_tm_latch.h;\
	if [ -f $$src ]; then\
		$(SED) "s#//.*##" $$src > $@.tmp && \
		mv -f $@.tmp $@;\
	else\
		touch $@;\
	fi

$(NCONF_ARGS): $(BUILD_DIR)/replacements/gsi_tm_latch.h

$(BUILD_DIR)/replacements/gsi_tm_latch.h: \
	$(BUILD_DIR)/replacements/wb_vendors.h

endif

include gmake/c.mk
include gmake/close.mk
include module/reggen/module.mk
