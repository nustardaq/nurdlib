# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2023-2024
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

QUIET=@
MKDIR=[ -d $(@D) ] || mkdir -p $(@D)
SED=sed

CHECK_MAP_PRE=dir=$$(BUILD_DIR_PREFIX=$@_ make -C .. -s -f gmake/build_dir_echo.mk) && nconf=../$$dir/nconf/module/map/map && if ! grep
CHECK_MAP_POST=$${nconf}.h; then cat $${nconf}.h.log; echo "$@ branch failed!" 1>&2; exit 1; fi

CHECK_EBONE_PRE=dir=$$(BUILD_DIR_PREFIX=$@_ make -C .. -s -f gmake/build_dir_echo.mk) && nconf=../$$dir/nconf/module/gsi_etherbone/nconf && if ! grep
CHECK_EBONE_POST=$${nconf}.h; then cat $${nconf}.h.log; echo "$@ branch failed!" 1>&2; exit 1; fi

CHECK_PEX_PRE=dir=$$(BUILD_DIR_PREFIX=$@_ make -C .. -s -f gmake/build_dir_echo.mk) && nconf=../$$dir/nconf/module/gsi_pex/nconf && if ! grep
CHECK_PEX_POST=$${nconf}.h; then cat $${nconf}.h.log; echo "$@ branch failed!" 1>&2; exit 1; fi

CHECK_TRLOII_PRE=dir=$$(BUILD_DIR_PREFIX=$@_ make -C .. -s -f gmake/build_dir_echo.mk) && nconf=../$$dir/nconf/include/nurdlib/trloii && if ! grep
CHECK_TRLOII_POST=$${nconf}.h; then cat $${nconf}.h.log; echo "$@ branch failed!" 1>&2; exit 1; fi

.PHONY: all
all: rio3find rio3smem rio4bma xpc326 xpc3310 rio2 universe rimfaxe \
	caen mvlc_config mvlc_env \
	etherbone pex \
	tridi trlo \
	drasi drasi_tridi drasi_trlo \
	mbs mbs_tridi mbs_trlo
	@echo "Done!"

.PHONY: rio3find
rio3find: rio3find/lib/ces/libbma.a \
	rio3find/lib/ces/libbus.a \
	rio3find/lib/ces/libuio.a \
	rio3find/lib/ces/libvme.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ RIO3LDPATH=mock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) RIO3_FINDCONTROLLER $(CHECK_MAP_POST)

.PHONY: rio3smem
rio3smem: rio3smem/lib/ces/libbma.a rio3smem/lib/ces/libuio.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ RIO3LDPATH=mock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) RIO3_SMEM $(CHECK_MAP_POST)

.PHONY: rio4bma
rio4bma: rio4bma/lib/ces/libbma.a \
	rio4bma/lib/ces/libbus.a \
	rio4bma/lib/ces/libuio.a \
	rio4bma/lib/ces/libvme.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ RIO4LDPATH=mock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) RIO4_BMA $(CHECK_MAP_POST)

.PHONY: xpc326
xpc326:
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ LDFLAGS=-Lmock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) XPC_3_2_6 $(CHECK_MAP_POST)

.PHONY: xpc3310
xpc3310: xpc3310/libcesXpcLib.a xpc3310/libcesDma.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ LDFLAGS=-Lmock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) XPC_3_3_10 $(CHECK_MAP_POST)

.PHONY: rio2
rio2: rio2/libsmem.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ LDFLAGS=-Lmock/$@ LIBS=-lsmem make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) RIO2 $(CHECK_MAP_POST)

.PHONY: universe
universe: universe/libvme.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ LDFLAGS=-Lmock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) UNIVERSE $(CHECK_MAP_POST)

.PHONY: rimfaxe
rimfaxe: rimfaxe/libavb.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS=-Imock/$@ LDFLAGS=-Lmock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) RIMFAXE $(CHECK_MAP_POST)

.PHONY: caen
caen: caen/libCAENVME.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS="-isystem mock/$@" LDFLAGS=-Lmock/$@ make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) CAEN_VMELIB $(CHECK_MAP_POST)

.PHONY: mvlc_config
mvlc_config: mvlc/libmesytec-mvlc.a mvlc/libmvlcc.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ MVLCC_CONFIG=mock/mvlc/mvlcc-config.sh make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) MVLC $(CHECK_MAP_POST)

.PHONY: mvlc_env
mvlc_env: mvlc/libmesytec-mvlc.a mvlc/libmvlcc.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ MVLCC_CFLAGS=-Imock/mvlc/include \
	    MVLCC_LIBS="-Lmock/mvlc -lmvlcc -lmesytec-mvlc" make -j8 -C ../ &&\
	$(CHECK_MAP_PRE) MVLC $(CHECK_MAP_POST)

.PHONY: etherbone
etherbone: etherbone/lib/libetherbone.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ make -j8 -C ../ WR_SYS=mock/$@ &&\
	$(CHECK_EBONE_PRE) ETHERBONE_bYES $(CHECK_EBONE_POST)

.PHONY: pex
pex: pex/dev/pexor
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ CPPFLAGS="-DPEX_DEV_PREFIX=mock/$@ -DMAP_LOCKED=0" make -j8 -C ../ &&\
	$(CHECK_PEX_PRE) GSI_PEX_bYES $(CHECK_PEX_POST)

.PHONY: tridi
tridi: tridi/common/lib_mock/trcomlib.a \
	tridi/hwmap/lib_mock/libhwmap.a \
	tridi/trloctrl/fw_cacad0d0_tridi/lib_mock/tridi_ctrllib.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ TRLOII_PATH=mock/$@ make -j8 -C ../ &&\
	$(CHECK_TRLOII_PRE) TRIDI_bYES $(CHECK_TRLOII_POST)

.PHONY: trlo
trlo: trlo/dtc_arch/arch_suffix_inc.mk \
	trlo/common/lib_mock/trcomlib.a \
	trlo/hwmap/lib_mock/libhwmap.a \
	trlo/trloctrl/fw_cacad0d0_trlo/lib_mock/trlo_ctrllib.a
	$(QUIET) \
	BUILD_DIR_PREFIX=$@_ TRLOII_PATH=mock/$@ make -j8 -C ../ &&\
	$(CHECK_TRLOII_PRE) VULOM4_bYES $(CHECK_TRLOII_POST)

.PHONY: drasi
drasi: caen \
	drasi/f_user_daq.o \
	drasi/lwroc_message_internal.o \
	drasi/lwroc_thread_util.o \
	drasi/lwroc_track_timestamp.o \
	drasi/lmd/lwroc_lmd_ev_sev.o
	$(QUIET) \
	BUILD_DIR_PREFIX=caen_ CPPFLAGS="-Imock/drasi/ -Imock/caen" LDFLAGS=-Lmock/caen \
	    make -j8 -C ../ DRASI_CONFIG=mock/drasi/drasi-config.sh fuser_drasi

.PHONY: drasi_tridi
drasi_tridi: tridi \
	drasi/f_user_daq.o \
	drasi/lwroc_message_internal.o \
	drasi/lwroc_thread_util.o \
	drasi/lwroc_track_timestamp.o \
	drasi/lmd/lwroc_lmd_ev_sev.o
	$(QUIET) \
	BUILD_DIR_PREFIX=tridi_ TRLOII_PATH=mock/tridi CPPFLAGS=-Imock/drasi/ \
	    make -j8 -C ../ DRASI_CONFIG=mock/drasi/drasi-config.sh fuser_drasi

.PHONY: drasi_trlo
drasi_trlo: trlo \
	drasi/f_user_daq.o \
	drasi/lwroc_message_internal.o \
	drasi/lwroc_thread_util.o \
	drasi/lwroc_track_timestamp.o \
	drasi/lmd/lwroc_lmd_ev_sev.o
	$(QUIET) \
	BUILD_DIR_PREFIX=trlo_ TRLOII_PATH=mock/trlo CPPFLAGS=-Imock/drasi/ \
	    make -j8 -C ../ DRASI_CONFIG=mock/drasi/drasi-config.sh fuser_drasi

.PHONY: mbs
mbs: caen \
	mbs/lib/lib_mbs.a \
	mbs/lib/lib_tools.a \
	mbs/obj/m_read_meb.o
	$(QUIET) \
	BUILD_DIR_PREFIX=caen_ CPPFLAGS="-Imock/mbs/ -Imock/caen" LDFLAGS="-Lmock/mbs -Lmock/caen" \
	    make -j8 -C ../ GSI_OS=os GSI_CPU_ENDIAN=endian MBSROOT=mock/mbs fuser_mbs

.PHONY: mbs_tridi
mbs_tridi: tridi \
	mbs/lib/lib_mbs.a \
	mbs/lib/lib_tools.a \
	mbs/obj/m_read_meb.o
	$(QUIET) \
	BUILD_DIR_PREFIX=tridi_ TRLOII_PATH=mock/tridi CPPFLAGS=-Imock/mbs/ LDFLAGS=-Lmock/mbs \
	    make -j8 -C ../ GSI_OS=os GSI_CPU_ENDIAN=endian MBSROOT=mock/mbs fuser_mbs

.PHONY: mbs_trlo
mbs_trlo: trlo \
	mbs/lib/lib_mbs.a \
	mbs/lib/lib_tools.a \
	mbs/obj/m_read_meb.o
	$(QUIET) \
	BUILD_DIR_PREFIX=trlo_ TRLOII_PATH=mock/trlo CPPFLAGS=-Imock/mbs/ LDFLAGS=-Lmock/mbs \
	    make -j8 -C ../ GSI_OS=os GSI_CPU_ENDIAN=endian MBSROOT=mock/mbs fuser_mbs

.PHONY: clean
clean:
	BUILD_DIR_PREFIX=rio3find_ make -C ../ clean
	BUILD_DIR_PREFIX=rio3smem_ make -C ../ clean
	BUILD_DIR_PREFIX=rio4bma_ make -C ../ clean
	BUILD_DIR_PREFIX=xpc326_ make -C ../ clean
	BUILD_DIR_PREFIX=xpc3310_ make -C ../ clean
	BUILD_DIR_PREFIX=rio2_ make -C ../ clean
	BUILD_DIR_PREFIX=universe_ make -C ../ clean
	BUILD_DIR_PREFIX=rimfaxe_ make -C ../ clean
	BUILD_DIR_PREFIX=caen_ make -C ../ clean
	BUILD_DIR_PREFIX=mvlc_ make -C ../ clean
	BUILD_DIR_PREFIX=etherbone_ make -C ../ clean
	BUILD_DIR_PREFIX=pex_ make -C ../ clean
	BUILD_DIR_PREFIX=tridi_ make -C ../ clean
	BUILD_DIR_PREFIX=trlo_ make -C ../ clean
	rm -rf */*.a */*.o */lib/ pex \
	    tridi/*/lib_mock/*.a tridi/*/lib_mock/*.o \
	    tridi/trloctrl/*/*/*.a tridi/trloctrl/*/*/*.o \
	    trlo \
	    drasi/*.o drasi/lmd/*.o

rio3find/lib/ces/libbma.a: rio3find/bma.o
rio3find/lib/ces/libbus.a: rio3find/bus.o
rio3find/lib/ces/libuio.a: rio3find/uio.o
rio3find/lib/ces/libvme.a: rio3find/vme.o

rio3smem/lib/ces/libbma.a: rio3smem/bma.o
rio3smem/lib/ces/libuio.a: rio3smem/uio.o

rio4bma/lib/ces/libbma.a: rio4bma/bma.o
rio4bma/lib/ces/libbus.a: rio4bma/bus.o
rio4bma/lib/ces/libuio.a: rio4bma/uio.o
rio4bma/lib/ces/libvme.a: rio4bma/vme.o

xpc3310/libcesXpcLib.a: xpc3310/cesXpcLib.o
xpc3310/libcesDma.a: xpc3310/cesDma.o

rio2/libsmem.a: rio2/smem.o

universe/libvme.a: universe/vme.o

rimfaxe/libavb.a: rimfaxe/avb.o

caen/libCAENVME.a: caen/caen.o

mvlc/libmesytec-mvlc.a: mvlc/mesytec-mvlc.o
mvlc/libmvlcc.a: mvlc/mvlcc.o

etherbone/lib/libetherbone.a: etherbone/lib.o

mbs/lib/lib_mbs.a: mbs/mbs.o
mbs/lib/lib_tools.a: mbs/tools.o

pex/dev/pexor: Makefile
	$(QUIET)echo PEX $@ && \
	$(MKDIR) &&\
	touch $@

tridi/common/lib_mock/trcomlib.a: tridi/common/lib_mock/com.o
tridi/hwmap/lib_mock/libhwmap.a: tridi/hwmap/lib_mock/map.o
tridi/trloctrl/fw_cacad0d0_tridi/lib_mock/tridi_ctrllib.a: \
    tridi/trloctrl/fw_cacad0d0_tridi/lib_mock/ctrl.o

# Copy and transform tridi/ to trlo/.
trlo/common/lib_mock/com.c: tridi/common/lib_mock/com.c
	$(QUIET)$(MKDIR) && cp $< $@
trlo/dtc_arch/arch_suffix_inc.mk: tridi/dtc_arch/arch_suffix_inc.mk
	$(QUIET)$(MKDIR) && cp $< $@
trlo/hwmap/hwmap_mapvme.h: tridi/hwmap/hwmap_mapvme.h
	$(QUIET)$(MKDIR) && cp $< $@
trlo/hwmap/lib_mock/map.c: \
	tridi/hwmap/lib_mock/map.c trlo/hwmap/hwmap_mapvme.h
	$(QUIET)$(MKDIR) && cp $< $@
trlo/trloctrl/fw_cacad0d0_trlo/include/trlo_access.h: \
	tridi/trloctrl/fw_cacad0d0_tridi/include/tridi_access.h
	$(QUIET)$(MKDIR) && $(SED) 's,tridi,trlo,g;s,TRIDI,TRLO,g' $< > $@
trlo/trloctrl/fw_cacad0d0_trlo/include/trlo_functions.h: \
	tridi/trloctrl/fw_cacad0d0_tridi/include/tridi_functions.h
	$(QUIET)$(MKDIR) && $(SED) 's,tridi,trlo,g;s,TRIDI,TRLO,g;s,NIM,LEMO,g' $< > $@
trlo/trloctrl/fw_cacad0d0_trlo/lib_mock/ctrl.c: \
	tridi/trloctrl/fw_cacad0d0_tridi/lib_mock/ctrl.c \
	trlo/trloctrl/fw_cacad0d0_trlo/include/trlo_access.h \
	trlo/trloctrl/fw_cacad0d0_trlo/include/trlo_functions.h
	$(QUIET)$(MKDIR) && $(SED) 's,tridi,trlo,g;s,TRIDI,TRLO,g' $< > $@

trlo/common/lib_mock/trcomlib.a: trlo/common/lib_mock/com.o
trlo/hwmap/lib_mock/libhwmap.a: trlo/hwmap/lib_mock/map.o
trlo/trloctrl/fw_cacad0d0_trlo/lib_mock/trlo_ctrllib.a: \
    trlo/trloctrl/fw_cacad0d0_trlo/lib_mock/ctrl.o

%.a:
	$(QUIET)echo A $@ && \
	$(MKDIR) && \
	ar crs $@.tmp $^ && \
	mv -f $@.tmp $@

%.o: %.c Makefile
	$(QUIET)echo O $@ && \
	gcc -c -o $@ $< -I$(@D) -I$(@D)/include
