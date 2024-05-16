/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2019, 2021, 2024
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

#ifndef MODULE_GSI_SIDEREM_INTERNAL_H
#define MODULE_GSI_SIDEREM_INTERNAL_H

#include <module/gsi_sam/internal.h>

TAILQ_HEAD(GsiSideremList, GsiSiderem);
struct GsiSiderem {
	struct	GsiSamGtbClient gtb_client;
	struct	GsiSamModule *sam;
	struct	GsiSideremCrate *crate;
	unsigned	mod_num;
	uint32_t	mod_mask;
	unsigned	lec;
	int	is_compressed;
	TAILQ_ENTRY(GsiSiderem)	next;
};
struct GsiSideremCrate {
	char	*data_path;
	char	*util_path;
	struct	GsiSideremList list;
};

void			gsi_siderem_crate_add(struct GsiSideremCrate *, struct
    GsiSamGtbClient *);
void			gsi_siderem_crate_configure(struct GsiSideremCrate *,
    struct ConfigBlock *);
void			gsi_siderem_crate_create(struct GsiSideremCrate *);
void			gsi_siderem_crate_destroy(struct GsiSideremCrate *);
int			gsi_siderem_crate_init_slow(struct GsiSideremCrate *)
	FUNC_RETURNS;
struct GsiSamGtbClient	*gsi_siderem_create(struct GsiSamModule *, struct
    ConfigBlock *) FUNC_RETURNS;

#endif
