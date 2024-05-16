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

NTEST_SUITE_LIST:=$(patsubst %.c,$(BUILD_DIR)/%.suite,$(NTEST_SRC))
NTEST_SUITES:=$(BUILD_DIR)/$(NTEST_DIR)/ntest_.suites
NTEST_MAIN_C:=$(BUILD_DIR)/$(NTEST_DIR)/ntest_.c
NTEST_MAIN_O:=$(NTEST_MAIN_C:.c=.o)

$(NTEST_MAIN_C): $(NTEST_SUITES) ntest/ntest.h ntest/ntest.mk
	$(QUIET)echo "NTEST $@"; \
	$(MKDIR) && \
	(echo "#include <ntest/ntest.h>"; \
	 echo "#include <errno.h>"; \
	 echo "#include <fcntl.h>"; \
	 echo "NTEST_GLOBAL_;"; \
	 cat $< | awk '{print "NTEST_SUITE_PROTO_("$$0");"}'; \
	 echo "int main(void) {"; \
	 echo "NTEST_HEADER_;"; \
	 cat $< | awk '{print "NTEST_SUITE_CALL_("$$0");"}'; \
	 echo "NTEST_FOOTER_;"; \
	 echo "return !!g_ntest_try_fail_;}") > $@.tmp && \
	mv -f $@.tmp $@

$(NTEST_SUITES): $(NTEST_SUITE_LIST)
	$(QUIET)echo "SUITS $@"; \
	$(MKDIR) && \
	cat $^ > $@.tmp && \
	mv -f $@.tmp $@

$(BUILD_DIR)/%.suite: %.c ntest/ntest.mk $(NTEST_DEP)
	$(QUIET)echo "SUITE $@"; \
	$(MKDIR) && \
	$(CC) -E $(CPPFLAGS_) $(NCONF_CPPFLAGS) $< > $@.tmp && \
	grep "void ntest_suite_.*_(" $@.tmp | $(SED) 's/.*ntest_suite_\(.*\)_(.*/\1/' > $@.tmp2 && \
	rm -f $@.tmp && \
	mv -f $@.tmp2 $@
