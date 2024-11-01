# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2023-2024
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

NAME:=fuser
DIR_$(NAME):=fuser
C_$(NAME):=$(DIR_$(NAME))/fuser.c

ifeq (fuser_drasi,$(MAKECMDGOALS))
DRASI_PATH=../drasi
OBJ_$(NAME):=$(BUILD_DIR)/$(DIR_$(NAME))/fuser_drasi.o
DRASI_CONFIG=$(DRASI_PATH)/bin/drasi-config.sh
FUSER_CPPFLAGS:=$(CPPFLAGS_) -DFUSER_DRASI=1 $(shell $(DRASI_CONFIG) --f-user-header --mbscompat --cflags)
FUSER_CFLAGS:=$(CFLAGS_)
FUSER_LIBS:=$(shell $(DRASI_CONFIG) --f-user-daq --mbscompat --libs)
FUSER_BASE:=m_read_meb.drasi
FUSER_STAMP:=$(shell $(DRASI_CONFIG) --stamp)
endif

ifeq (fuser_mbs,$(MAKECMDGOALS))
OBJ_$(NAME):=$(BUILD_DIR)/$(DIR_$(NAME))/fuser_mbs.o
MBSBUILD:=$(shell echo $(MBSBIN) | $(SED) 's,.*/bin,,')
FUSER_CPPFLAGS:=$(CPPFLAGS_) -DFUSER_MBS=1 -D$(GSI_OS) -D$(GSI_CPU_ENDIAN) -I$(MBSROOT)/inc
FUSER_CFLAGS:=$(filter -M% -O%,$(CFLAGS_))
FUSER_LIBS:=$(MBSROOT)/obj$(MBSBUILD)/m_read_meb.o -L$(MBSROOT)/lib$(MBSBUILD) -l_mbs -l_tools -l_mbs -lreadline -lncurses
FUSER_BASE:=m_read_meb.mbs
endif

ifneq (,$(FUSER_BASE))

FUSER_RULE:=$(MAKECMDGOALS)
FUSER_TARGET:=$(BUILD_DIR)/$(FUSER_BASE)

.PHONY: $(FUSER_RULE)
$(FUSER_RULE): all $(FUSER_TARGET)

include gmake/close.mk

# Both trloii and drasi may provide -lhwmap, remove from NCONF_LIBS.
$(FUSER_TARGET): $(OBJ_$(NAME)) $(NURDLIB_TARGET) bin/$(FUSER_BASE) $(HWMAP_ERROR_INTERNAL_O) $(FUSER_STAMP)
	$(QUIET)echo "LD    $@"; \
	libs="$(NCONF_LIBS)"; \
	lhwmap=$(echo $(FUSER_LIBS) | grep -- -lhwmap && echo yes); \
	if [ "yes" = "$$lhwmap" ]; then \
		libs="$$(echo $$libs | $(SED) 's/-lhwmap//')"; fi; \
	$(CC) -o $@ $(filter %.o %.a,$^) $(LDFLAGS_) $(NCONF_LDFLAGS) $$libs $(COV_LIBS) $(FUSER_LIBS) $(LIBS_)

$(OBJ_$(NAME)): $(C_$(NAME)) Makefile fuser/rules.mk $(NCONF_ARGS)
	$(QUIET)echo "FUSER $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c -o $@ $< $(FUSER_CPPFLAGS) $(NCONF_CPPFLAGS) $(NCONF_CFLAGS) $(COV_CFLAGS) $(FUSER_CFLAGS)

endif
