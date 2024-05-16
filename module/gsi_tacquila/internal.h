/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2022, 2024
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

#ifndef MODULE_GSI_TACQUILA_INTERNAL_H
#define MODULE_GSI_TACQUILA_INTERNAL_H

#include <module/gsi_sam/internal.h>

TAILQ_HEAD(GsiTacquilaList, GsiTacquila);
struct GsiTacquilaCrate {
	char	*data_path;
	char	*util_path;
	struct	GsiTacquilaList list;
};
struct GsiTacquila {
	struct	GsiSamGtbClient gtb_client;
	struct	GsiSamModule *sam;
	struct	GsiTacquilaCrate *crate;
	unsigned	card_num;
	unsigned	lec;
	TAILQ_ENTRY(GsiTacquila)	next;
};

void			gsi_tacquila_crate_configure(struct GsiTacquilaCrate
    *, struct ConfigBlock *);
void			gsi_tacquila_crate_add(struct GsiTacquilaCrate *,
    struct GsiSamGtbClient *);
void			gsi_tacquila_crate_create(struct GsiTacquilaCrate *);
void			gsi_tacquila_crate_destroy(struct GsiTacquilaCrate *);
int			gsi_tacquila_crate_init_slow(struct GsiTacquilaCrate
    *) FUNC_RETURNS;
struct GsiSamGtbClient	*gsi_tacquila_create(struct GsiSamModule *, struct
    ConfigBlock *) FUNC_RETURNS;

#endif
