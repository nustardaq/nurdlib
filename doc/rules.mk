# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2021, 2024-2025
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

NAME:=doc
DIR_$(NAME):=doc
SRC_$(NAME):=$(wildcard $(addprefix $(DIR_$(NAME))/source/,*))

INKSCAPE=inkscape
SPHINX_BUILD=sphinx-build

.PHONY: doc
doc: $(BUILD_DIR)/doc/web/index.html

$(BUILD_DIR)/doc/web/index.html: $(SRC_$(NAME)) $(BUILD_DIR)/$(DIR_$(NAME))/source/nurdlib.png
	$(QUIET)echo "WD    $@" &&\
	cp -r $(dir $<)/* $(BUILD_DIR)/doc/source/ && \
	$(SPHINX_BUILD) $(BUILD_DIR)/doc/source/ $(dir $@)

$(BUILD_DIR)/$(DIR_$(NAME))/source/nurdlib.png: $(DIR_$(NAME))/source/nurdlib.svg $(DIR_$(NAME))/rules.mk
	$(QUIET)echo "PNG   $@" &&\
	$(MKDIR) &&\
	$(INKSCAPE) -D -o $@ -d 150 $<

include gmake/close.mk
