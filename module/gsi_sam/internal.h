/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2021, 2023-2024
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

#ifndef MODULE_GSI_SAM_INTERNAL_H
#define MODULE_GSI_SAM_INTERNAL_H

#include <module/module.h>

#define GSI_SAM_GTB_CLIENT_CREATE(client, a_type, a_gtb_i, a_free, \
    a_parse_data, a_rewrite_data) \
	do { \
		client.type = a_type; \
		client.gtb_i = a_gtb_i; \
		client.free = a_free; \
		client.parse_data = a_parse_data; \
		client.rewrite_data = a_rewrite_data; \
	} while (0)
#define GSI_SAM_GTB_OFFSET(sam, gtb_i) \
    ((((sam)->address >> 24) - (sam)->crate->first) * 0x01000000 + \
     (gtb_i) * 0x00100000)

struct GsiSamModule;
struct GsiSideremCrate;
struct GsiTacquilaCrate;
struct PnpiCros3Crate;

struct GsiSamGtbClient {
	enum	Keyword type;
	unsigned	gtb_i;
	void	(*free)(struct GsiSamGtbClient **);
	uint32_t	(*parse_data)(struct Crate const *, struct
	    GsiSamGtbClient *, struct EventConstBuffer *) FUNC_RETURNS;
	uint32_t	(*rewrite_data)(struct GsiSamGtbClient const *, struct
	    EventBuffer *) FUNC_RETURNS;
};
struct GsiSamCrate {
	unsigned	first;
	unsigned	last;
	enum	Keyword blt_mode;
	struct	Map *sicy_map;
	struct	Map *blt_map;
};
struct GsiSamModule {
	struct	Module module;
	uint32_t	address;
	enum	Keyword blt_mode;
	struct	GsiSamCrate *crate;
	struct	GsiSamGtbClient *gtb_client[2];
};

void	gsi_sam_crate_collect(struct GsiSamCrate *, struct GsiSideremCrate *,
    struct GsiTacquilaCrate *, struct PnpiCros3Crate *, struct Module *);
void	gsi_sam_crate_create(struct GsiSamCrate *);
void	gsi_sam_crate_deinit(struct GsiSamCrate *);
int	gsi_sam_crate_init_slow(struct GsiSamCrate *) FUNC_RETURNS;
int	gsi_sam_crate_init_fast(struct GsiSamCrate *) FUNC_RETURNS;

#endif
