/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2021, 2023-2024
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

#ifndef MODULE_GSI_TAMEX_INTERNAL_H
#define MODULE_GSI_TAMEX_INTERNAL_H

#include <module/module.h>

struct GsiTamexCrate {
	struct	GsiTamexModule *sfp[4];
};
struct GsiTamexModuleCard {
	struct	ConfigBlock *config;
	uint32_t	tdc_num;
	/* Channel (0..15) extracted as a sync reference, -1 = don't. */
	int	sync_ch;
};
struct GsiTamexModule {
	struct	Module module;
	size_t	sfp_i;
	enum	Keyword model;
	size_t	card_num;
	struct	GsiTamexModuleCard *card_array;
	unsigned	pex_buf_idx;
	unsigned	gsi_mbs_trigger;
};

void	gsi_tamex_crate_add(struct GsiTamexCrate *, struct Module *);
void	gsi_tamex_crate_create(struct GsiTamexCrate *);
int	gsi_tamex_crate_init_slow(struct GsiTamexCrate *) FUNC_RETURNS;

#endif
