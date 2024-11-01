# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2024
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

TRLOII_DEFS:=$($(TRLOII_TYPE)_DEFS)
ifneq (,$(TRLOII_DEFS))
TRLOII_REGLIST:=$(BUILD_DIR)/$(DIR_$(NAME))/register_list.txt
$(TRLOII_REGLIST): $(TRLOII_DEFS) module/trloii/reglist.mk
	$(MKDIR)
	echo "anbm" > $@.tmp
	$(SED) -n '/_scaler_map_t/,/_scaler_map;/p' $< | grep uint32_t | \
	$(SED) 's/.* \(0x[0-9a-f]*\) .*_t \(.*\);/\1 \2 32 R/' >> $@.tmp && \
	mv -f $@.tmp $@
REGGEN_PREFIX:=$(BUILD_DIR)/
include module/reggen/module.mk
endif
