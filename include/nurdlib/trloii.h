/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
 * Michael Munch
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

#ifndef NURDLIB_TRLOII_H
#define NURDLIB_TRLOII_H

#include <nconf/include/nurdlib/trloii.h>
#include <nurdlib/base.h>
#include <util/funcattr.h>
#include <util/stdint.h>

/* Ripped from trloii header to avoid vararg macros. */
void trcom_hwmap_error_internal(int, const char *, int, const char *, ...);
typedef void (*hwmap_error_internal_func)(int, const char *, int, const char
    *, ...);

#if NCONF_mTRIDI_bYES
/* NCONF_CPPFLAGS=-I$TRLOII_PATH */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/dtc_arch */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/hwmap */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${TRIDI_FW}_tridi */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${TRIDI_FW}_tridi/gen_$TRLOII_ARCH_SUFFIX */
/* NCONF_LDFLAGS=-L$TRLOII_PATH/hwmap/lib_$TRLOII_ARCH_SUFFIX */
/* NCONF_LIBS=$TRLOII_PATH/trloctrl/fw_${TRIDI_FW}_tridi/lib_$TRLOII_ARCH_SUFFIX/tridi_ctrllib.a */
/* NCONF_LIBS=-lhwmap $TRLOII_PATH/common/lib_$TRLOII_ARCH_SUFFIX/trcomlib.a */
/* NCONF_NOEXEC */
#	include <include/tridi_functions.h>
#	if NCONFING_mTRIDI
#		define NCONF_TEST tridi_setup_map_hardware(0, NULL)
#	endif
#elif NCONF_mTRIDI_bNO
/* NCONF_NOLINK */
#endif

#if NCONF_mVULOM4_bYES
/* NCONF_CPPFLAGS=-I$TRLOII_PATH */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/dtc_arch */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/hwmap */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${VULOM4_FW}_trlo */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${VULOM4_FW}_trlo/gen_$TRLOII_ARCH_SUFFIX */
/* NCONF_LDFLAGS=-L$TRLOII_PATH/hwmap/lib_$TRLOII_ARCH_SUFFIX */
/* NCONF_LIBS=$TRLOII_PATH/trloctrl/fw_${VULOM4_FW}_trlo/lib_$TRLOII_ARCH_SUFFIX/trlo_ctrllib.a */
/* NCONF_LIBS=-lhwmap $TRLOII_PATH/common/lib_$TRLOII_ARCH_SUFFIX/trcomlib.a */
/* NCONF_NOEXEC */
#	include <include/trlo_functions.h>
#	if NCONFING_mVULOM4
#		define NCONF_TEST trlo_setup_map_hardware(0, NULL)
#	endif
#elif NCONF_mVULOM4_bNO
/* NCONF_NOLINK */
#endif

#if NCONF_mRFX1_bYES
/* NCONF_CPPFLAGS=-I$TRLOII_PATH */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/dtc_arch */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/hwmap */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${RFX1_FW}_rfx1 */
/* NCONF_CPPFLAGS=-I$TRLOII_PATH/trloctrl/fw_${RFX1_FW}_rfx1/gen_$TRLOII_ARCH_SUFFIX */
/* NCONF_LDFLAGS=-L$TRLOII_PATH/hwmap/lib_$TRLOII_ARCH_SUFFIX */
/* NCONF_LIBS=$TRLOII_PATH/trloctrl/fw_${RFX1_FW}_rfx1/lib_$TRLOII_ARCH_SUFFIX/rfx1_ctrllib.a */
/* NCONF_LIBS=-lhwmap $TRLOII_PATH/common/lib_$TRLOII_ARCH_SUFFIX/trcomlib.a */
/* NCONF_NOEXEC */
#	define __MYSTDINT_H__
#	include <include/rfx1_functions.h>
#	if NCONFING_mRFX1
/* TODO: Why is this different from tridi/trlo? */
#		define NCONF_TEST hwmap_map_vme(0, 0, NULL, NULL)
#	endif
#elif NCONF_mRFX1_bNO
/* NCONF_NOLINK */
#endif

#if defined(NCONF_mTRIDI_bYES) || \
    defined(NCONF_mVULOM4_bYES) || \
    defined(NCONF_mRFX1_bYES)
#	define NURDLIB_TRLOII 1
#	define TRLOII_HWMAP_ERROR_INTERNAL_IMPL \
    hwmap_error_internal_func _hwmap_error_internal = \
	trcom_hwmap_error_internal;
#	if defined(NCONFING_mTRIDI) || \
	    defined(NCONFING_mVULOM4) || \
	    defined(NCONFING_mRFX1)
#		include <hwmap/hwmap_mapvme.h>
TRLOII_HWMAP_ERROR_INTERNAL_IMPL
#	endif
#else
#	define TRLOII_HWMAP_ERROR_INTERNAL_IMPL
#endif

#ifndef NCONF_mTRIDI_bYES
typedef struct tridi_opaque_t {
	int	dummy;
} tridi_opaque;
#endif
#ifndef NCONF_mVULOM4_bYES
typedef struct trlo_opaque_t {
	int	dummy;
} trlo_opaque;
#endif
#ifndef NCONF_mRFX1_bYES
typedef struct rfx1_opaque_t {
	int	dummy;
} rfx1_opaque;
#endif

struct Module;

char const			*trloii_get_multi_event_tag_name(struct Module
    *) FUNC_NONNULL(()) FUNC_RETURNS;
struct rfx1_readout_control	*trloii_get_rfx1_ctrl(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
rfx1_opaque volatile		*trloii_get_rfx1_map(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct tridi_readout_control	*trloii_get_tridi_ctrl(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
tridi_opaque volatile		*trloii_get_tridi_map(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct trlo_readout_control	*trloii_get_trlo_ctrl(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
trlo_opaque volatile		*trloii_get_trlo_map(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
int				trloii_has_master_start(struct Module const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
int				trloii_is_timestamper(struct Module *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void				trloii_multi_event_set_limit(struct Module *,
    unsigned) FUNC_NONNULL(());

#endif
