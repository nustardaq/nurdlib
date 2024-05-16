# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017-2019, 2021, 2024
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

# A fake module is a near complete module used for sharing code, e.g. v1n90.
# It must not be listed by module/rules.mk.
# NOTE: This depends on object names so must be included after gmake/*.mk.

$(DEP_$(NAME)): $(BUILD_DIR)/$(DIR_$(NAME))/fake
$(BUILD_DIR)/$(DIR_$(NAME))/fake:
	$(QUIET)echo "FAKE  $@" &&\
	$(MKDIR) &&\
	touch $@
