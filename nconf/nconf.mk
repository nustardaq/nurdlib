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

#
# Role of "typical" vars:
#  CPPFLAGS_, CFLAGS_, LDFLAGS_, LIBS_
# These are used during nconfing but are not saved in the output, apply them
# again afterwards if needed!
#
# Example for LIBS_:
#  LIBS_=-lm
#  NCONF_LIBS=-labc
# The linker will call:
#  ld ... $(NCONF_LIBS) $(LIBS_)
# On successful nconf, only NCONF_LIBS will be written.
#
# Note that the order of some flags is important:
#  $(CPPFLAGS_) $(PREV_CPPFLAGS) $(CUR_CPPFLAGS)
#  $(CFLAGS_) $(PREV_CFLAGS) $(CUR_CFLAGS)
#  $(LDFLAGS_) $(PREV_LDFLAGS) $(CUR_LDFLAGS)
#   The above allows the user to override nconf:ed paths, and later nconf:ed
#   flags should not destroy previous ones.
#  $(CUR_LIBS) $(PREV_LIBS) $(LIBS_)
#   This allows the user to fix missing references, and later nconf:ed libs
#   can rely on previous ones.
# One could do "$(CPPFLAGS_BEFORE) $(NCONF_CPPFLAGS) $(CPPFLAGS_AFTER)" for
# maximum power and confusion, but nah.
#
# Auto-configuring is a pain...
#

NCONFER:=$(NCONF_PATH)$(BUILD_DIR)/nconfer

# Re-configs dirty files in NCONF_H in order.
$(NCONF_ARGS): $(NCONF_H) $(NCONFER) $(NCONF_PREV)
	$(QUIET)$(MKDIR) && \
	echo "NCONF $@"; \
	echo "For nconf results and logs, see $(BUILD_DIR)/nconf*."; \
	export CPPFLAGS="$(CPPFLAGS_)"; \
	export CFLAGS="$(CFLAGS_)"; \
	export LDFLAGS="$(LDFLAGS_)"; \
	export LIBS="$(LIBS_)"; \
	force=; \
	prev_args=$@.1st; \
	if [ -n "$(NCONF_PREV)" ]; then \
		cp $(NCONF_PREV) $$prev_args; \
	else \
		echo > $$prev_args; \
		echo >> $$prev_args; \
		echo >> $$prev_args; \
		echo >> $$prev_args; \
	fi; \
	for i in $(NCONF_H); do \
		nconfed=$(BUILD_DIR)/nconf/$$i; \
		if [ "$$force" -o ! -f $$nconfed -o ! -f $$nconfed.args -o \
		    $$i -nt $$nconfed -o \
		    $$i -nt $$nconfed.args -o \
		    $(NCONFER) -nt $$nconfed ]; then \
			rm -f $$nconfed.args; \
			$(NCONFER) -c$(CC) -i$$i -o$(BUILD_DIR) \
			    -a$$prev_args || exit 1; \
			force=1; \
		fi; \
		prev_args=$$nconfed.args; \
	done; \
	cp $$prev_args $@; \
	echo "$@ done."

# nconf:ed headers should exist if $(NCONF_ARGS) does.
define nconf_rule
$(BUILD_DIR)/nconf/$(1): $(NCONF_ARGS)
endef
$(foreach f,$(NCONF_H),$(eval $(call nconf_rule,$(f))))

# Extract flags from $(NCONF_ARGS).
ifeq (,$(SED))
SED=sed
endif
NCONF_CPPFLAGS=$(shell [ -f $(NCONF_ARGS) ] && $(SED) -n 1p $(NCONF_ARGS))
NCONF_CFLAGS=$(shell [ -f $(NCONF_ARGS) ] && $(SED) -n 2p $(NCONF_ARGS))
NCONF_LDFLAGS=$(shell [ -f $(NCONF_ARGS) ] && $(SED) -n 3p $(NCONF_ARGS))
NCONF_LIBS=$(shell [ -f $(NCONF_ARGS) ] && $(SED) -n 4p $(NCONF_ARGS))

# Common linking rule.
NCONF_LD=$(QUIET)echo "LD    $@"; \
$(MKDIR) && \
$(CC) -o $@ $(filter %.o %.a,$^) $(LDFLAGS_) $(NCONF_LDFLAGS) $(NCONF_LIBS) $(COV_LIBS) $(LIBS_)
