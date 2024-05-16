# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2021, 2024
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

NAME:=webdoc
DIR_$(NAME):=doc/web
SRC_$(NAME):=$(wildcard $(addprefix $(DIR_$(NAME))/source/,*.rst))

INKSCAPE=inkscape
SPHINX_BUILD=sphinx-build

.PHONY: webdoc
webdoc: $(BUILD_DIR)/webdoc/publish.sh

# TODO: Should we really have GSI-specific stuff here?
$(BUILD_DIR)/webdoc/publish.sh: $(BUILD_DIR)/webdoc
	$(QUIET)echo "WDP  $@" &&\
	echo "rsync -aP $(BUILD_DIR)/webdoc/* land@lx-pool.gsi.de:~/web-docs/nurdlib" > $@ &&\
	chmod +x $@

# The copying is ugly, but I don't know how to give search paths to Sphinxdoc.
$(BUILD_DIR)/webdoc: $(DIR_$(NAME))/source $(SRC_$(NAME)) $(BUILD_DIR)/$(DIR_$(NAME))/nurdlib.png
	$(QUIET)echo "WD   $@" &&\
	cp $(BUILD_DIR)/doc/web/nurdlib.png doc/web/source && \
	$(SPHINX_BUILD) $< $@
	rm doc/web/source/nurdlib.png

$(BUILD_DIR)/$(DIR_$(NAME))/nurdlib.png: $(DIR_$(NAME))/source/nurdlib.svg $(DIR_$(NAME))/rules.mk
	$(QUIET)echo "PNG  $@" &&\
	$(MKDIR) &&\
	$(INKSCAPE) -D -o $@ -d 150 $<

include gmake/close.mk
