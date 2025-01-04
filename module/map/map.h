/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Michael Munch
 * Stephane Pietri
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef MODULE_MAP_H
#define MODULE_MAP_H

#include <nconf/module/map/map.h>
#include <nurdlib/keyword.h>
#include <util/funcattr.h>
#include <util/stdint.h>

/* Assume BLT return value unreliable, clear for verified good platforms. */
/* TODO: This is ugly! */
#define MAP_BLT_RETURN_BROKEN 1

#if NCONF_mMAP_bRIO3_FINDCONTROLLER
/* NCONF_LDFLAGS=-L$RIO3LDPATH/lib/ces */
/* NCONF_LIBS=-lbma -luio -lbus -lvme */
/* NCONF_NOEXEC */
#	include <ces/bmalib.h>
#	include <ces/vmelib.h>
#	if NCONFING_mMAP
intptr_t	find_controller(uintptr_t, size_t, unsigned, unsigned,
    unsigned, struct pdparam_master const *);
#		define NCONF_TEST \
	0 == bma_open() && -1 != find_controller(0, 0, 0, 0, 0, NULL)
#	endif
#	undef MAP_BLT_RETURN_BROKEN
#	define POKE_SIGBUS
#	define SICY_FINDCONTROLLER
#	define BLT_BMA
#	define BLT_BMA_D32_BASE 0x50000000
#	define BLT_BMA_D64_BASE 0x60000000
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bRIO3_SMEM
/* NCONF_LDFLAGS=-L$RIO3LDPATH/lib/ces */
/* NCONF_LIBS=-lbma -luio */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <smem.h>
#		include <ces/bmalib.h>
#		define NCONF_TEST \
	0 == bma_open() && NULL != smem_create(NULL, NULL, 0, 0)
#	endif
#	undef MAP_BLT_RETURN_BROKEN
#	define POKE_SIGBUS
#	define SICY_SMEM
#	define SICY_SMEM_A32_BASE 0x50000000
#	define BLT_BMA
#	define BLT_BMA_D32_BASE 0x50000000
#	define BLT_BMA_D64_BASE 0x60000000
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bRIO4_BMA
/* NCONF_LDFLAGS=-L$RIO4LDPATH/lib/ces */
/* NCONF_LIBS=-lbma -luio -lbus -lvme */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <ces/bmalib.h>
#		define NCONF_TEST 0 == bma_open()
#	endif
#	undef MAP_BLT_RETURN_BROKEN
#	define POKE_SIGBUS
#	define SICY_DIRECT
#	define SICY_DIRECT_A32_BASE 0xf0000000
#	define BLT_BMA
#	define BLT_BMA_D32_BASE 0x50000000
#	define BLT_BMA_D64_BASE 0x60000000
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bRIO4_XPC_3_2_6
/* NCONF_NOLINK */
#	if NCONFING_mMAP
#		include <ces/xpc_dma.h>
#		define NCONF_TEST XPC_DMA_READ
#	endif
#	define POKE_SAFE_PIO
#	define SICY_DIRECT
#	define SICY_DIRECT_A32_BASE 0xa0000000
#	define BLT_XPC_3_2_6
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bRIO4_XPC_3_3_10
/* NCONF_CPPFLAGS=-I/usr/include/ces/cesXpcLib */
/* NCONF_CPPFLAGS=-I/usr/include/ces/cesOsApi */
/* NCONF_CPPFLAGS=-I/usr/include/ces/cesDma */
/* NCONF_LIBS=-lcesXpcLib -lcesDma */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <ces/CesXpcBridge.h>
#		define NCONF_TEST (struct CesXpcBridge *)1
#	endif
#	define POKE_SIGBUS
#	define SICY_XPC_3_3_10
#	define BLT_XPC_3_3_10
#elif NCONF_mMAP_bRIO2
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <smem.h>
#		define NCONF_TEST NULL != smem_create(NULL, NULL, 0, 0)
#	endif
#	define POKE_SIGBUS
#	define SICY_SMEM
#	define SICY_SMEM_A32_BASE 0xe0000000
#	define BLT_DUMB
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bUNIVERSE
/* NCONF_LIBS=-lvme */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <vme/vme.h>
#		include <vme/vme_api.h>
#	endif
#	define POKE_UNIVERSE
#	define SICY_UNIVERSE
#	define BLT_DUMB
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bMVLC
/* NCONF_CFLAGS=$($MVLCC_CONFIG --cflags) */
/* NCONF_LDFLAGS=$($MVLCC_CONFIG --ldflags) */
/* NCONF_LIBS=$($MVLCC_CONFIG --libs) */
/* NCONF_CFLAGS=$MVLCC_CFLAGS */
/* NCONF_LIBS=$MVLCC_LIBS */
#       if NCONFING_mMAP
#		include <mvlcc_wrap.h>
#	endif
#	define POKE_MVLC
#       define SICY_MVLC
#	define BLT_MVLC
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bCMVLC
/* NCONF_CFLAGS=$($CMVLC_CONFIG --cflags) */
/* NCONF_LDFLAGS=$($CMVLC_CONFIG --ldflags) */
/* NCONF_LIBS=$($CMVLC_CONFIG --libs) */
/* NCONF_CFLAGS=$CMVLC_CFLAGS */
/* NCONF_LIBS=$CMVLC_LIBS */
#       if NCONFING_mMAP
#		include <cmvlc.h>
#	endif
#	define POKE_CMVLC
#       define SICY_CMVLC
#	define BLT_DUMB
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bRIMFAXE
/* NCONF_LIBS=-lavb */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <rimfaxe/avb.h>
#	endif
#	undef MAP_BLT_RETURN_BROKEN
#	define POKE_RIMFAXE
#	define SICY_RIMFAXE
#	define BLT_RIMFAXE_FF
#       define BLT_DST_DUMB
#       define BLT_HW_MBLT_SWAP
#elif NCONF_mMAP_bCAEN_VMELIB
/* NCONF_LIBS=-lCAENVME */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <CAENVMElib.h>
#		define NCONF_TEST CAENVME_End(0)
#	endif
#	define POKE_CAEN
#	define SICY_CAEN
#	define BLT_CAEN
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bCAEN_VMELIB_LOCAL
/*
 * Note: To build with CAENVMElib without having to install it:
 *  make \
 *      CPPFLAGS="-isystem <CAENVMElib-path>/include" \
 *      LIBS=<CAENVMElib-path>/lib/<platform>/libCAENVME.so...
 */
/* NCONF_NOEXEC */
#	if NCONFING_mMAP
#		include <CAENVMElib.h>
#		define NCONF_TEST CAENVME_End(0)
#	endif
#	define POKE_CAEN
#	define SICY_CAEN
#	define BLT_CAEN
#       define BLT_DST_DUMB
#elif NCONF_mMAP_bDUMB
#	define POKE_DUMB
#	define SICY_DUMB
#	define BLT_DUMB
#       define BLT_DST_DUMB
#endif

#define MAP_SIZE_MAX(s) MAX(sizeof *(s).read, sizeof *(s).write)
#define MAP_POKE_REG(reg) MOD_##reg, OFS_##reg, BITS_##reg
#define MAP_READ_OFS(map, reg, ofs) \
	map_sicy_read(map, MOD_##reg, BITS_##reg, OFS_##reg + (ofs))
#define MAP_WRITE_OFS(map, reg, ofs, val) \
	map_sicy_write(map, MOD_##reg, BITS_##reg, OFS_##reg + (ofs), val)
#define MAP_READ(map, reg) MAP_READ_OFS(map, reg, 0)
#define MAP_WRITE(map, reg, val) MAP_WRITE_OFS(map, reg, 0, val)

struct Map;
struct MapBltDst;

/*
 * Aligns pointer and size in bytes according to BLT keyword i.e. some
 * multiple of 32 bits, and pads with the given filler word.
 */
void			*map_align(void *, size_t *, enum Keyword, uint32_t)
	FUNC_RETURNS;

/*
 * Single-cycle access.
 */
uint32_t		map_sicy_read(struct Map *, unsigned, unsigned,
    size_t) FUNC_RETURNS;
void			map_sicy_write(struct Map *, unsigned, unsigned,
    size_t, uint32_t);

/*
 * Destination memory for BLT, currently for shadow mode where the DAQ backend
 * typically does not provide such aux storage. Can be used by user-code
 * between map_{setup,shutdown}.
 */
struct MapBltDst	*map_blt_dst_alloc(size_t) FUNC_RETURNS;
void			map_blt_dst_free(struct MapBltDst **);
void			*map_blt_dst_get(struct MapBltDst *) FUNC_RETURNS;

/*
 * Performs BLT read at an offset.
 *  arg0 = map from map_map.
 *  arg1 = offset in bytes.
 *  arg2 = destination buffer, from DAQ backend or map_blt_dst_*.
 *  arg3 = number of bytes to transfer.
 *  return = number of bytes transferred, or < 0 on error.
 *           NOTE: Unreliable if MAP_BLT_RETURN_BROKEN set!
 */
int			map_blt_read(struct Map *, size_t, void *, size_t)
	FUNC_RETURNS;
/* Performs BLT read which may end with a bus error. */
int			map_blt_read_berr(struct Map *, size_t, void *,
    size_t) FUNC_RETURNS;

/* Cleans up global resources allocated by 'map_map'. */
void			map_deinit(void);
struct Map		*map_map(uint32_t, size_t, enum Keyword, int, int,
    unsigned, uintptr_t, unsigned,
    unsigned, uintptr_t, unsigned, uint32_t) FUNC_RETURNS;
/* Called at the start of a process. */
void			map_setup(void);
/* Called at the end of a process. */
void			map_shutdown(void);
void			map_unmap(struct Map **);

/*
 * Memory regions can be added by the user and will override other mapping if
 * the region is large enough, e.g.:
 *
 *  map = map_map(0x01000004, 0x1000, KW_NOBLT);
 * 'map' points to "any" address as mapped by the OS.
 *
 *  uint8_t user_mem[0x1004];
 *  map_user_add(0x01000000, user_mem, sizeof user_mem);
 *  map = map_map(0x01000004, 0x1000, KW_NOBLT);
 * 'map' points to user_mem + 4.
 *
 *  uint8_t user_mem[0x1003];
 *  map_user_add(0x01000000, user_mem, sizeof user_mem);
 *  map = map_map(0x01000004, 0x1000, KW_NOBLT);
 * 'map' is not overridden with the user region since it's too small for the
 * requested mapping.
 *
 * In case user regions overlap, the first added region will be chosen. The
 * given memory is the user's responsibility and will not be freed.
 */
void		map_user_add(uint32_t, void *, size_t);
void		map_user_clear(void);

#ifdef SICY_CAEN
/* Checks if the given controller type is supported. */
int		map_caen_type_exists(char const *);
/*
 * Returns available controller types.
 * Start with arg=0 and increment, the first non-existent entry returns NULL.
 */
char const	*map_caen_type_get(size_t) FUNC_RETURNS;
/*
 * Overrides CAEN controller config.
 *  arg0: type, NULL or empty will not override.
 *  arg1: ip, NULL or empty will not override.
 *  arg2: link, -1 will not override.
 *  arg3: conet, -1 will not override.
 */
void		map_caen_config_override(char const *, char const *, int,
    int);
#endif

#if defined(SICY_MVLC) || defined (SICY_CMVLC)
/*
 * Overrides MVLC config.
 *  arg0: ip, NULL or empty will not override.
 */
void	map_mvlc_config_override(char const *);
#endif

#endif
