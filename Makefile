# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2025
# Bastian Löher
# Håkan T Johansson
# Michael Munch
# Oliver Papst
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
# User config.
#

.SECONDARY:

-include local.mk

define infovar
$(info $(1) = "$($(1))")
endef

MODULE_LIST=caen_v1190 caen_v1290 caen_v1n90 \
	    caen_v767a \
	    caen_v775 caen_v785 caen_v785n caen_v792 caen_v7nn caen_v965 \
	    caen_v560 caen_v820 caen_v830 caen_v8n0 caen_v895 \
	    caen_v1725 \
	    dummy \
	    gsi_ctdc gsi_ctdc_proto gsi_etherbone gsi_febex gsi_kilom \
	    gsi_mppc_rob gsi_pex gsi_pexaria gsi_sam gsi_siderem \
	    gsi_tacquila gsi_tamex gsi_tridi gsi_triva gsi_vetar gsi_vftx2 \
	    gsi_vulom gsi_vuprom gsi_rfx1 \
	    mesytec_mxdc32 \
	    mesytec_madc32 mesytec_mqdc32 mesytec_mtdc32 \
	    mesytec_mdpp \
	    mesytec_mdpp16scp mesytec_mdpp16qdc \
	    mesytec_mdpp32qdc mesytec_mdpp32scp \
	    mesytec_vmmr8 \
	    pnpi_cros3 \
	    sis_3316 sis_3801 sis_3820 sis_3820_scaler \
	    trloii

AR=ar
BISON_:=$(shell bison -h 1>/dev/null 2>/dev/null && echo nurdlib_yes | grep nurdlib_yes)
ifeq (nurdlib_yes,$(BISON_))
 BISON=bison
else
 BISON=yacc
endif
CCACHE_:=$(shell ccache -h 1>/dev/null 2>/dev/null && echo nurdlib_yes | grep nurdlib_yes)
ifeq (nurdlib_yes,$(CCACHE_))
 CCACHE=ccache
endif
CPPCHECK=cppcheck
FLEX=flex
LATEX=latex
PYTHON=python
SED=sed

#
# Basic setup.
#

QUIET=@

include gmake/build_dir.mk
$(call infovar,BUILD_DIR)

MKDIR=[ -d $(@D) ] || mkdir -p $(@D)

# Don't use eg CPPFLAGS, will be overwritten if given on the cmd-line.
CPPFLAGS_:=$(CPPFLAGS) -I$(BUILD_DIR)/replacements -I$(BUILD_DIR) \
	-Iinclude -I.
CFLAGS_:=$(CFLAGS) \
	-ansi -pedantic-errors \
	-Wall -Wcast-qual -Wformat=2 \
	-Wmissing-prototypes -Wshadow -Wstrict-prototypes \
	-Wconversion
LDFLAGS_:=$(LDFLAGS)
LIBS_:=$(LIBS)

ifeq (debug,$(BUILD_MODE))
 CFLAGS_+=-ggdb
endif
ifeq (cov,$(BUILD_MODE))
 # Separate coverage flags, gcda/gcno files pollute when compiling + linking
 # in one step.
 ifneq (,$(filter clang%,$(CC)))
  COVIFIER:=llvm-cov
  COV_CFLAGS+=--coverage
  COV_LIBS+=--coverage -lLLVMInstrumentation
 else
  COVIFIER:=gcov
  COV_CFLAGS+=--coverage
  COV_LIBS+=--coverage
 endif
endif
ifeq (gprof,$(BUILD_MODE))
 CFLAGS_+=-ggdb -pg
 LIBS_+=-pg
endif
ifeq (pic,$(BUILD_MODE))
 CFLAGS_+=-fPIC
endif
ifeq (release,$(BUILD_MODE))
 CFLAGS_+=-O3
endif

# TRLO II.
$(call infovar,TRLOII_PATH)
ifneq (,$(TRLOII_PATH))
 INC:=$(wildcard $(TRLOII_PATH)/dtc_arch/arch_suffix_inc.mk)
 ifneq (,$(INC))
  include $(INC)
  export TRLOII_ARCH_SUFFIX:=$(ARCH_SUFFIX)
  $(call infovar,TRLOII_ARCH_SUFFIX)
  CPPFLAGS_:=$(CPPFLAGS_) \
	  -DTRLOII_PATH=$(TRLOII_PATH) \
	  -DTRLOII_ARCH_SUFFIX=$(TRLOII_ARCH_SUFFIX)
  # This finds whatever firmware is first.  If they are just aliases to
  # the same version, it is ok.  If not - no joy...
  ifeq (,$(TRIDI_FW))
   TRIDI_FW:=$(shell ls $(TRLOII_PATH)/trloctrl/ | grep _tridi | head -n1 | $(SED) 's/fw_\(.*\)_tridi/\1/')
  endif
  ifeq (,$(VULOM4_FW))
   VULOM4_FW:=$(shell ls $(TRLOII_PATH)/trloctrl/ | grep _trlo | head -n1 | $(SED) 's/fw_\(.*\)_trlo/\1/')
  endif
  ifeq (,$(RFX1_FW))
   RFX1_FW:=$(shell ls $(TRLOII_PATH)/trloctrl/ | grep _rfx1 | head -n1 | $(SED) 's/fw_\(.*\)_rfx1/\1/')
  endif
  TRIDI_DEFS:=$(wildcard $(TRLOII_PATH)/trloctrl/fw_$(TRIDI_FW)_tridi/tridi_defs.h)
  VULOM4_DEFS:=$(wildcard $(TRLOII_PATH)/trloctrl/fw_$(VULOM4_FW)_trlo/trlo_defs.h)
  RFX1_DEFS:=$(wildcard $(TRLOII_PATH)/trloctrl/fw_$(RFX1_FW)_rfx1/rfx1_defs.h)
  HWMAP_ERROR_INTERNAL_O:=$(BUILD_DIR)/tools/hwmap_error_internal.o
 endif
 export TRIDI_FW
 export VULOM4_FW
 export RFX1_FW
 $(call infovar,TRIDI_FW)
 $(call infovar,VULOM4_FW)
 $(call infovar,RFX1_FW)
endif

# Nurdlib!

NURDLIB_TARGET:=$(BUILD_DIR)/./libnurdlib.a
NURDCTRL_TARGET:=$(BUILD_DIR)/nurdctrl
MEMTEST_TARGET:=$(BUILD_DIR)/tools/memtest
RWDUMP_TARGET:=$(BUILD_DIR)/tools/rwdump
WRSLEW_TARGET:=$(BUILD_DIR)/tools/wrslew
ifeq (pic,$(BUILD_MODE))
 PYNURDLIBCTRL_INSTALL=pynurdlibctrl_install
endif
VIMSYN:=$(BUILD_DIR)/nurdlib.vim
NCONF_ARGS:=$(BUILD_DIR)/nconf.args

.PHONY: all
all: lib $(MEMTEST_TARGET) $(NURDCTRL_TARGET) $(RWDUMP_TARGET) $(WRSLEW_TARGET)
	$(QUIET)j=$(BUILD_DIR)/nconf/module/map/map.h; \
		grep " NCONF_m[^ ]*" $$j | \
		$(SED) 's,.*NCONF_m.*_b\(.*\) .*,Mapping = \1,'
	@echo "$(BUILD_DIR): Simon says: Alles wird gut ;o)"

bin/m_read_meb.drasi:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)
bin/m_read_meb.mbs:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)
bin/memtest:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)
bin/nurdctrl:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)
bin/rwdump:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)
bin/wrslew:
	$(QUIET)echo "LN    $@" &&\
	cd $(@D) && ln -s caller.sh $(@F)

.PHONY: lib
lib: $(NURDLIB_TARGET)

.PHONY: vim
vim: $(VIMSYN)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: help
help:
	@echo "Regular targets:"
	@echo " make (all)           - Build the whole shebang, except the f-users below."
	@echo " make fuser_drasi     - Build minimal MBS-style f_user with drasi."
	@echo " make fuser_mbs       - Build minimal MBS-style f_user with mbs."
	@echo " make nconf           - Produces nconf only."
	@echo " make showconfig      - Show important values used in the build process."
	@echo " make test            - Build and run tests."
	@echo " make test_ntest      - Build and run ntest tests."
	@echo " make cppcheck        - cppcheck all source files without NCONF macros."
	@echo " make pynurdlibctrl_install"
	@echo "                      - Install Python ctrl package."
	@echo " make vim             - Install vim syntax file."
	@echo " make clean           - Remove everything that has been built."
	@echo " make help            - Print what you are now reading."
	@echo
	@echo " make QUIET=          - Turn on verbose output."
	@echo " make NURDLIB_DEF_PATH"
	@echo "                      - Override path to default configurations."
	@echo " make BUILD_MODE=type - release, pic, cov, anything else = debug."
	@echo
	@echo "Coverage analysis, only available in cov build mode:"
	@echo " 1) make BUILD_MODE=cov test      - Build and run tests."
	@echo " 2) make BUILD_MODE=cov cov       - Print total summary."
	@echo " 3) make BUILD_MODE=cov cov_files - File-level summary."
	@echo " 4) make BUILD_MODE=cov cov_funcs - Function-level summary."
	@echo " 4) make BUILD_MODE=cov cov_anno  - Annotated sources."
	@echo
	@echo "CPPFLAGS and LDFLAGS prepended, CFLAGS and LIBS appended."
	@echo
	@echo "nconf intermediary files are in \"$(BUILD_DIR)/nconfing/\"."
	@echo "nconf results and logs are in \"$(BUILD_DIR)/nconf/\"."
	@echo "nconf flags are in \"$(NCONF_ARGS)\"."

#
# External header replacements.
#  Not all system headers adhere to strict ANSI C standards, so in order not
#  to forfeit strict checking of the nurdlib source code, we make fixed up
#  local copies.
#

REPL=echo "REPL  $@" && $(MKDIR)

$(BUILD_DIR)/replacements/proc.h: Makefile
	$(QUIET)$(REPL);\
	src=/usr/include/proc.h;\
	if [ -f $$src ]; then\
		$(SED) "s,FREE,FUREE,;s/handler)()/handler)(void)/" $$src > $@.tmp && \
		mv -f $@.tmp $@; \
	else\
		touch $@;\
	fi

$(BUILD_DIR)/replacements/bsd/in.h: Makefile
	$(QUIET)$(REPL);\
	src=/usr/include/bsd/in.h;\
	if [ -f $$src ]; then\
		$(SED) "s,static,," $$src > $@.tmp && \
		mv -f $@.tmp $@; \
	else\
		touch $@;\
	fi

$(BUILD_DIR)/replacements/ces/bmalib.h: Makefile
	$(QUIET)$(REPL);\
	src=/usr/include/ces/bmalib.h;\
	if [ -f $$src ]; then\
		$(SED) "s,\(#endif\) \(_BMACMD_H_\),\1 /* \2 */," $$src > $@.tmp && \
		$(SED) "s/BMA_M_VszMax,/BMA_M_VszMax/" $@.tmp > $@.tmp2 && \
		mv -f $@.tmp2 $@; \
	else\
		touch $@;\
	fi

$(BUILD_DIR)/replacements/ces/cesTypes.h: Makefile
	$(QUIET)$(REPL);\
	src=/usr/include/ces/cesOsApi/ces/cesTypes.h;\
	if [ -f $$src ]; then\
		$(SED) "3i #include <stdint.h>" $$src > $@.tmp && \
		$(SED) "s/unsigned long long/uint64_t/;s/typedef long long/typedef int64_t/" $@.tmp > $@.tmp2 && \
		mv -f $@.tmp2 $@; \
	else\
		touch $@;\
	fi

$(BUILD_DIR)/replacements/vme/vme.h: Makefile
	$(QUIET)$(REPL);\
	src=/usr/include/vme/vme.h;\
	if [ -f $$src ]; then\
		$(SED) "s/VME_CTL_PWEN = 0x40000000,/VME_CTL_PWEN = 0x40000000/" $$src > $@.tmp;\
		$(SED) "s/VME_CTL_LEGACY_WINNUM = 0x80000000//" $@.tmp > $@.tmp2; \
		$(SED) "s/VME_DMA_DW_64 = 0x00C00000,/VME_DMA_DW_64 = 0x00C00000/" $@.tmp2 > $@.tmp; \
		$(SED) "s/vme_master_ctl_flags_t;/vme_master_ctl_flags_t;\n#define VME_CTL_LEGACY_WINNUM 0x80000000/" $@.tmp > $@.tmp2; \
		mv -f $@.tmp2 $@; \
	else\
		touch $@;\
	fi
#
# Nurdlib auto-confing.
#

NCONF_H:=\
	util/compiler.h \
	util/endian.h \
	util/stdint.h \
	util/fmtmod.h \
	util/funcattr.h \
	util/fs.c \
	util/limits.h \
	util/memcpy.c \
	util/thread.h \
	util/time.c \
	util/udp.c \
	util/pack.c \
	util/math.c \
	module/map/map.h \
	module/gsi_pex/nconf.h \
	module/gsi_etherbone/nconf.h \
	include/nurdlib/serialio.h \
	include/nurdlib/trloii.h
include nconf/nconf.mk
$(NCONFER): nconf/nconf.c nconf/nconf.mk Makefile $(BUILD_DIR)/config/kwenum.h
	$(QUIET)echo "CCLD  $@"; \
	$(MKDIR); \
	$(CC) -ggdb -o $@ $<

$(NCONF_ARGS): $(BUILD_DIR)/replacements/proc.h \
		$(BUILD_DIR)/replacements/bsd/in.h \
		$(BUILD_DIR)/replacements/ces/bmalib.h \
		$(BUILD_DIR)/replacements/ces/cesTypes.h \
		$(BUILD_DIR)/replacements/vme/vme.h

#
# All specific rules.
#

# Cancel built-in c -> o recipe.
%.o: %.c

include util/rules.mk
include config/rules.mk
include crate/rules.mk
include ctrl/rules.mk
ifeq (pic,$(BUILD_MODE))
 include ctrl/pynurdlibctrl/rules.mk
endif
include doc/rules.mk
include doc/web/rules.mk
include module/rules.mk
include tools/rules.mk
include $(foreach module,$(MODULE_LIST),module/$(module)/rules.mk)
include fuser/rules.mk

# Must be last because it needs the listings of all the others.
include rules.mk

#
# Testing.
#

TEST_TARGET:=$(BUILD_DIR)/test
TEST_CTRL_TARGET:=$(BUILD_DIR)/test_ctrl
NTEST_TARGET:=$(BUILD_DIR)/test_ntest
TEST_OK:=$(TEST_TARGET)_ok
TEST_CTRL_OK:=$(TEST_CTRL_TARGET)_ok
NTEST_OK:=$(NTEST_TARGET)_ok

$(NURDLIB_TARGET): $(TEST_OK) $(TEST_CTRL_OK) $(NTEST_OK)

export NURDLIB_DEF_PATH=cfg/default

.PHONY: test
test: $(TEST_TARGET)
	$(QUIET)export MY_TEST_PATH=$(shell pwd)/tests; \
	./$<

.PHONY: test_ctrl
test_ctrl: $(TEST_CTRL_TARGET) tests/test_ctrl/server.cfg
	$(QUIET)./$<

$(TEST_OK): $(TEST_TARGET)
	$(QUIET)echo "TEST  $@"; \
	export MY_TEST_PATH=$(shell pwd)/tests; \
	if ./$< > $<.log 2>&1; then touch $@; else cat $<.log; exit 1; fi
$(TEST_CTRL_OK): $(TEST_CTRL_TARGET) tests/test_ctrl/server.cfg
	$(QUIET)echo "TEST  $@"; \
	if ./$< > $<.log 2>&1; then touch $@; else cat $<.log; exit 1; fi
$(TEST_TARGET): $(OBJ_nurdlib) $(NTEST_OK)
$(TEST_CTRL_TARGET): $(OBJ_nurdlib) $(NTEST_OK)
include tests/rules.mk
include tests/test_ctrl/rules.mk

.PHONY: test_ntest
test_ntest: $(NTEST_TARGET)
	$(QUIET)./$< || true;\
	echo "Results should be 7/10 and 21/29!"

$(NTEST_OK): $(NTEST_TARGET)
	$(QUIET)echo "TEST  $@"; \
	./$< > $<.log 2>&1 || true;\
	[ "$$(tail -2 $<.log | grep 'Tests: 7/10') && echo yes" ];\
	[ "$$(tail -2 $<.log | grep 'Tries: 21/29') && echo yes" ];\
	touch $@
include ntest/tests/rules.mk

#
# Gcoving.
#

ifeq (cov,$(BUILD_MODE))

COV_SRC:=$(SRC_ACC)

.PHONY: cov
cov:
	$(QUIET)echo Collecting coverage info...;\
	lines_total=0;\
	lines_ok_total=0;\
	for i in $(COV_SRC); do\
		dir=`dirname $$i`;\
		numbers=`$(COVIFIER) -n $$i -o $(BUILD_DIR)/$$dir 2> /dev/null | grep "%" | $(SED) 's/[^0-9\. ]//g'`;\
		[ "$$numbers" ] || continue;\
		percentage=`echo $$numbers | awk '{print $$1}'`;\
		lines=`echo $$numbers | awk '{print $$2}'`;\
		lines_ok=`echo "$$percentage * $$lines / 100+1" | bc`;\
		lines_ok_total=`expr $$lines_ok_total + $$lines_ok`;\
		lines_total=`expr $$lines_total + $$lines`;\
	done;\
	percentage_total=`echo "100 * $$lines_ok_total / $$lines_total" | bc`;\
	echo Coverage: $$percentage_total% \(~$$lines_ok_total/$$lines_total lines\)

.PHONY: cov_files
cov_files:
	$(QUIET)echo Dumping gcov file-level info...;\
	for i in $(COV_SRC); do\
		$(COVIFIER) -n $$i -o $(BUILD_DIR)/`dirname $$i`;\
	done

.PHONY: cov_funcs
cov_funcs:
	$(QUIET)echo Dumping gcov function-level info...;\
	for i in $(COV_SRC); do\
		$(COVIFIER) -fn $$i -o $(BUILD_DIR)/`dirname $$i`;\
	done

.PHONY: cov_anno
cov_anno:
	$(QUIET)echo Generating gcov annotated files...;\
	[ -d $(BUILD_DIR)/cov ] || mkdir -p $(BUILD_DIR)/cov;\
	for i in $(COV_SRC); do\
		$(COVIFIER) -lp $$i -o $(BUILD_DIR)/`dirname $$i`;\
		mv *.gcov $(BUILD_DIR)/cov;\
	done;\
	echo Files moved to $(BUILD_DIR)/cov/.

endif

#
# Cppcheck.
#

.PHONY: cppcheck
cppcheck:
	$(QUIET)o=$(BUILD_DIR)/cppcheck.supp;\
	find . -name "*.h" ! -path "./build*" -exec grep -o "NCONF_m[0-9A-Za-z_]*" {} \; | sort -u > $$o;\
	find . -name "*.h" ! -path "./build*" -exec grep -o "NCONFING_m[0-9A-Za-z_]*" {} \; | sort -u >> $$o;\
	$(CPPCHECK) --force --enable=all $(filter -I%,$(CPPFLAGS_)) $$($(SED) 's/^/-U/' $$o) $(SRC_ACC)

#
# Show variables and nconf results.
#

define printconfig
@echo $(1) = \"$($(1))\"
endef

.PHONY: nconf
nconf: $(NCONF_ARGS)

.PHONY: showconfig
showconfig:
	@echo --- Variables ---
	$(call printconfig,AR)
	$(call printconfig,BISON)
	$(call printconfig,CCACHE)
	$(call printconfig,CPPCHECK)
	$(call printconfig,FLEX)
	$(call printconfig,LATEX)
	$(call printconfig,PYTHON)
	$(call printconfig,SED)
	$(call printconfig,BUILD_DIR)
	$(call printconfig,CPPFLAGS_)
	$(call printconfig,CFLAGS_)
	$(call printconfig,LDFLAGS_)
	$(call printconfig,LIBS_)
	$(call printconfig,TRLOII_PATH)
	$(call printconfig,TRLOII_ARCH_SUFFIX)
	$(call printconfig,TRIDI_FW)
	$(call printconfig,TRIDI_DEFS)
	$(call printconfig,VULOM4_FW)
	$(call printconfig,VULOM4_DEFS)
	$(call printconfig,RFX1_FW)
	$(call printconfig,RFX1_DEFS)
	$(call printconfig,NCONFER)
	$(call printconfig,NCONF_ARGS)
	$(call printconfig,NCONF_PREV)
	$(call printconfig,NCONF_CPPFLAGS)
	$(call printconfig,NCONF_CFLAGS)
	$(call printconfig,NCONF_LDFLAGS)
	$(call printconfig,NCONF_LIBS)
	$(call printconfig,DRASI_CONFIG)
	$(call printconfig,FUSER_CPPFLAGS)
	$(call printconfig,FUSER_CFLAGS)
	$(call printconfig,FUSER_LDFLAGS)
	$(call printconfig,FUSER_LIBS)
	@echo --- nconf ---
	@for i in $(NCONF_H); do \
		j=$(BUILD_DIR)/nconf/$$i; \
		echo $$j:; \
		grep " NCONF_m[^ ]*" $$j | \
		    $(SED) 's,.*NCONF_m\(.*\)_b\(.*\) .*, \1 -- \2,'; \
	done
	@echo
	@echo "For nconf logs, see \"$(BUILD_DIR)/nconf/*.\""

ifeq (,$(filter clean nconf showconfig,$(MAKECMDGOALS)))
 -include $(DEP_nurdlib) $(DEP_tests) $(DEP_test_ntest)
endif
