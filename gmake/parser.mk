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

# Parser (flex/bison) rules.

FLEX_$(NAME)_C:=$(patsubst %.l,$(BUILD_DIR)/%.yy.c,$(FLEX_$(NAME)))
BISON_$(NAME)_C:=$(patsubst %.y,$(BUILD_DIR)/%.tab.c,$(BISON_$(NAME)))
FLEX_$(NAME)_O:=$(FLEX_$(NAME)_C:.c=.o)
BISON_$(NAME)_O:=$(BISON_$(NAME)_C:.c=.o)
CGEN_$(NAME):=$(CGEN_$(NAME)) $(FLEX_$(NAME)_C) $(BISON_$(NAME)_C)

$(BISON_$(NAME)_C): $(BISON_$(NAME)) Makefile gmake/parser.mk $(DIR_$(NAME))/rules.mk
	$(QUIET)echo "BISON $@";\
	$(MKDIR) && \
	$(BISON) -o$@ -d $<
#	$(BISON) -o$@ -dtv $<

$(FLEX_$(NAME)_C): $(FLEX_$(NAME)) $(BISON_$(NAME)_C)
	$(QUIET)echo "FLEX  $@";\
	$(MKDIR) && \
	$(FLEX) -o$@.tmp $< && \
	$(SED) 's/ECHO (void) fwrite\(.*\))/ECHO do { if (fwrite\1)) {} } while (0)/' $@.tmp > $@.tmp2 && \
	rm -f $@.tmp && \
	mv -f $@.tmp2 $@
