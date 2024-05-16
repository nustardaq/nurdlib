# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2020-2021, 2023-2024
# Michael Munch
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

NAME:=map
DIR_$(NAME):=module/$(NAME)
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

include gmake/c.mk
include gmake/close.mk

# Special rules for CAEN user code with lenient flags.
$(BUILD_DIR)/module/map/map_caen.o: module/map/map_caen.c $(BUILD_DIR)/module/map/map_caen.d
	$(QUIET)echo "CC    $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c -o $@ $< $(CPPFLAGS_) $(NCONF_CPPFLAGS)
$(BUILD_DIR)/module/map/map_caen.d: module/map/map_caen.c Makefile module/map/rules.mk $(NCONF_ARGS)
	$(QUIET)echo "DEP   $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c $< -MM $(CPPFLAGS_) $(NCONF_CPPFLAGS) > $@.tmp && \
	$(SED) '1s,.*,$@ $(@D)/&,' $@.tmp > $@

# And for Universe headers.
$(BUILD_DIR)/module/map/map_sicy_universe.o: module/map/map_sicy_universe.c $(BUILD_DIR)/module/map/map_sicy_universe.d
	$(QUIET)echo "CC    $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c -o $@ $< $(CPPFLAGS_) $(NCONF_CPPFLAGS)
$(BUILD_DIR)/module/map/map_sicy_universe.d: module/map/map_sicy_universe.c Makefile module/map/rules.mk $(NCONF_ARGS)
	$(QUIET)echo "CC    $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c $< -MM $(CPPFLAGS_) $(NCONF_CPPFLAGS) > $@.tmp && \
	$(SED) '1s,.*,$@ $(@D)/&,' $@.tmp > $@
