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

#ifndef NURDLIB_CRATE_H
#define NURDLIB_CRATE_H

#include <nurdlib/base.h>
#include <nurdlib/keyword.h>

/* TODO: Make this a user-configurable struct. */
enum {
	CRATE_READOUT_FAIL_GENERAL = (1 << 0),
	CRATE_READOUT_FAIL_DATA_CORRUPT = (1 << 1),
	CRATE_READOUT_FAIL_DATA_MISSING = (1 << 2),
	CRATE_READOUT_FAIL_DATA_TOO_MUCH = (1 << 3),
	CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH = (1 << 4),
	CRATE_READOUT_FAIL_ERROR_DRIVER = (1 << 5),
	CRATE_READOUT_FAIL_UNEXPECTED_TRIGGER = (1 << 6)
};

struct Crate;
struct CrateCounter;
struct CrateTag;
struct GsiPex;
struct Module;
struct ModuleCounter;

typedef void		(*CvtSetCallback)(struct Module *, unsigned);
typedef uint32_t	(*ScalerGetCallback)(struct Module *, void *, struct
    Counter *) FUNC_RETURNS;

unsigned		crate_acvt_get_ns(struct Crate const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			crate_acvt_grow(struct Crate *) FUNC_NONNULL(());
int			crate_acvt_has(struct Crate const *) FUNC_NONNULL(())
	FUNC_RETURNS;
void			crate_acvt_set(struct Crate *, struct Module *,
    CvtSetCallback) FUNC_NONNULL(());

struct Crate		*crate_create(void) FUNC_RETURNS;
void			crate_deinit(struct Crate *) FUNC_NONNULL(());
void			crate_free(struct Crate **) FUNC_NONNULL(());

struct CrateCounter	*crate_counter_get(struct Crate *, char const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
uint32_t		crate_counter_get_diff(struct CrateCounter const *)
	FUNC_NONNULL(()) FUNC_RETURNS;

int			crate_dt_is_on(struct Crate const *) FUNC_NONNULL(());
void			crate_dt_release_inhibit_once(struct Crate *)
	FUNC_NONNULL(());
void			crate_dt_release_set_func(struct Crate *, void
    (*)(void *), void *) FUNC_NONNULL((1,2));

int			crate_free_running_get(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			crate_free_running_set(struct Crate *, int)
	FUNC_NONNULL(());

int			crate_get_do_shadow(struct Crate const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
char const		*crate_get_name(struct Crate const *) FUNC_NONNULL(())
	FUNC_RETURNS;
struct CrateTag		*crate_get_tag_by_name(struct Crate *, char const *)
	FUNC_NONNULL((1)) FUNC_RETURNS;

size_t			crate_tag_get_num(struct Crate const *)
	FUNC_NONNULL(()) FUNC_RETURNS;

struct CrateTag		*crate_get_tag_by_index(struct Crate *, unsigned)
	FUNC_NONNULL((1)) FUNC_RETURNS;

struct PnpiCros3Crate	*crate_get_cros3_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiCTDCCrate	*crate_get_ctdc_crate(struct Crate *) FUNC_NONNULL(())
	FUNC_RETURNS;
struct GsiFebexCrate	*crate_get_febex_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiKilomCrate	*crate_get_kilom_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiMppcRobCrate	*crate_get_mppc_rob_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiSideremCrate	*crate_get_siderem_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiTacquilaCrate	*crate_get_tacquila_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct GsiTamexCrate	*crate_get_tamex_crate(struct Crate *)
	FUNC_NONNULL(()) FUNC_RETURNS;

/* GSI MBS information sharing. */
unsigned		crate_gsi_mbs_trigger_get(struct Crate const *)
	FUNC_NONNULL(());
void			crate_gsi_mbs_trigger_set(struct Crate *, unsigned)
	FUNC_NONNULL(());
struct GsiPex		*crate_gsi_pex_get(struct Crate const *)
	FUNC_NONNULL(()) FUNC_RETURNS;

void			crate_init(struct Crate *) FUNC_NONNULL(());
void			crate_memtest(struct Crate const *const, const int)
	FUNC_NONNULL(());

/* Finds the n:th (0-based) module of a specific type. */
struct Module		*crate_module_find(struct Crate const *, enum Keyword,
    unsigned) FUNC_NONNULL(()) FUNC_RETURNS;
/* Puts the last read out data range into the event-buffer. */
void			crate_module_get_event_buffer(struct EventConstBuffer
    *, struct Module const *) FUNC_NONNULL(());
/* Returns the total number of modules. */
size_t			crate_module_get_num(struct Crate const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			crate_module_remap_id(struct Crate *, unsigned,
    unsigned) FUNC_NONNULL(());

/* Crate-wide readout and verification inside dead-time. */
uint32_t		crate_readout_dt(struct Crate *) FUNC_NONNULL(())
	FUNC_RETURNS;
/* Readout that may happen outside dead-time. */
uint32_t		crate_readout(struct Crate *, struct EventBuffer *)
	FUNC_NONNULL(()) FUNC_RETURNS;
/* Modules read out, do some final touches before the next event. */
void			crate_readout_finalize(struct Crate *)
	FUNC_NONNULL(());

void			crate_scaler_add(struct Crate *, char const *, struct
    Module *, void *, ScalerGetCallback) FUNC_NONNULL(());

void			crate_setup(void);

/* Sync channels. */
int			crate_sync_get(struct Crate *, unsigned, int *)
	FUNC_NONNULL(()) FUNC_RETURNS;
void			crate_sync_push(struct Crate *, int) FUNC_NONNULL(());

void			crate_tag_counter_increase(struct Crate *, struct
    CrateTag *, unsigned) FUNC_NONNULL(());
unsigned		crate_tag_get_event_max(struct CrateTag const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
size_t			crate_tag_get_module_num(struct CrateTag const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
char const		*crate_tag_get_name(struct CrateTag const *)
	FUNC_NONNULL(()) FUNC_RETURNS;
struct Module		*crate_tag_get_module_by_index(struct CrateTag const *,
    unsigned) FUNC_NONNULL(()) FUNC_RETURNS;

/* Pedestal readout and update. */
void			crate_tag_pedestal_prepare(struct CrateTag *)
	FUNC_NONNULL(());
void			crate_tag_pedestal_update(struct CrateTag *)
	FUNC_NONNULL(());

#endif
