/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2021, 2023-2024
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

#ifndef MODULE_GSI_ETHERBONE_INTERNAL_H
#define MODULE_GSI_ETHERBONE_INTERNAL_H

#include <module/gsi_etherbone/nconf.h>
#include <module/module.h>

struct GsiEtherboneModule {
	struct	Module module;
#if !NCONF_mGSI_ETHERBONE_bNO
	unsigned	fifo_id;
	enum	Keyword mode;
	int	is_direct;
	struct {
		uint32_t	address;
		struct  Map *vetar_sicy_map;
		uint32_t	*pexaria_mmap;
	} direct;
	struct {
		uintptr_t	tlu_address;
	} etherbone;
	eb_data_t	fifo_num;
#endif
};

uint32_t	gsi_etherbone_check_empty(struct GsiEtherboneModule *)
	FUNC_RETURNS;
void		gsi_etherbone_create(struct ConfigBlock *, struct
    GsiEtherboneModule *, enum Keyword);
void		gsi_etherbone_deinit(struct GsiEtherboneModule *);
int		gsi_etherbone_init_slow(struct GsiEtherboneModule *)
	FUNC_RETURNS;
uint32_t	gsi_etherbone_readout(struct GsiEtherboneModule *, struct
    EventBuffer *) FUNC_RETURNS;
uint32_t	gsi_etherbone_readout_dt(struct GsiEtherboneModule *)
	FUNC_RETURNS;

#endif
