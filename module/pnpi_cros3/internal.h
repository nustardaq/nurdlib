/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2018, 2022, 2024
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

#ifndef MODULE_PNPI_CROS3_INTERNAL_H
#define MODULE_PNPI_CROS3_INTERNAL_H

#include <module/gsi_sam/internal.h>

TAILQ_HEAD(PnpiCros3List, PnpiCros3);
struct PnpiCros3 {
	struct	GsiSamGtbClient gtb_client;
	struct	GsiSamModule *sam;
	struct	PnpiCros3Crate *crate;
	unsigned	ccb_id;
	unsigned	card_num;
	struct {
		uint8_t	ddc;
		uint16_t	drc;
		uint16_t	thc;
		uint8_t	thl;
		uint8_t	thu;
		uint8_t	tpe;
		uint8_t	tpo;
		uint16_t	trc;
		uint16_t	wim;
	} regs;
	TAILQ_ENTRY(PnpiCros3)	next;
};
struct PnpiCros3Crate {
	char	*data_path;
	struct	PnpiCros3List list;
};

struct GsiSamGtbClient	*pnpi_cros3_create(struct GsiSamModule *, struct
    ConfigBlock *) FUNC_RETURNS;
void			pnpi_cros3_crate_add(struct PnpiCros3Crate *, struct
    GsiSamGtbClient *);
void			pnpi_cros3_crate_configure(struct PnpiCros3Crate *,
    struct ConfigBlock *);
void			pnpi_cros3_crate_create(struct PnpiCros3Crate *);
void			pnpi_cros3_crate_destroy(struct PnpiCros3Crate *);
int			pnpi_cros3_crate_init_slow(struct PnpiCros3Crate *)
	FUNC_RETURNS;

#endif
