/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2019, 2024
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

#ifndef MODULE_GSI_FEBEX_INTERNAL_H
#define MODULE_GSI_FEBEX_INTERNAL_H

#include <module/module.h>

struct GsiFebexCrate {
	struct	GsiFebexModule *sfp[4];
};
struct GsiFebexModuleCard {
	struct	ConfigBlock *config;
	int	do_filter_trace;
};
struct GsiFebexModule {
	struct	Module module;
	size_t	sfp_i;
	size_t	card_num;
	unsigned	trace_length;
	struct	GsiFebexModuleCard *card_array;
};

void	gsi_febex_crate_add(struct GsiFebexCrate *, struct Module *);
void	gsi_febex_crate_create(struct GsiFebexCrate *);
int	gsi_febex_crate_init_slow(struct GsiFebexCrate *);

#endif
