# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2021-2024
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

SRC_ACC:=$(SRC_ACC) $(C_$(NAME))
OBJ_$(NAME):=$(OBJ_$(NAME)) $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_$(NAME)))

$(BUILD_DIR)/$(DIR_$(NAME))/%.o: $(DIR_$(NAME))/%.c $(BUILD_DIR)/$(DIR_$(NAME))/%.d
	$(QUIET)echo "CC    $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c -o $@ $< $(CPPFLAGS_) $(NCONF_CPPFLAGS) $(COV_CFLAGS) $(NCONF_CFLAGS) $(CFLAGS_)

$(BUILD_DIR)/$(DIR_$(NAME))/%.d: $(DIR_$(NAME))/%.c Makefile gmake/c.mk $(DIR_$(NAME))/rules.mk $(NCONF_ARGS)
	$(QUIET)echo "DEP   $@";\
	$(MKDIR) && \
	$(CCACHE) $(CC) -c $< -MM $(CPPFLAGS_) $(NCONF_CPPFLAGS) $(NCONF_CFLAGS) $(CFLAGS_) > $@.tmp && \
	$(SED) '1s,.*,$@ $(@D)/&,' $@.tmp > $@
