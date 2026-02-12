/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2020, 2023-2024
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

#ifndef MODULE_CAEN_V7NN_INTERNAL_H
#define MODULE_CAEN_V7NN_INTERNAL_H

#include <module/module.h>

struct CaenV7nnModule {
	struct	Module module;
	uint32_t	address;
	/* A bit lazy, but most supported Caens have pedestals. */
	int	do_auto_pedestals;
	int	do_berr;
	enum	Keyword blt_mode;
	uint32_t        channel_enable;
	struct	Map *sicy_map;
	struct	Map *dma_map;
	enum	Keyword child_type;
	int	geo;
	uint16_t	number_of_channels;
	uint32_t	counter_prev;
	uint32_t	counter_parse;
};

uint32_t	caen_v7nn_check_empty(struct CaenV7nnModule *) FUNC_RETURNS;
void		caen_v7nn_create(struct ConfigBlock *, struct CaenV7nnModule
    *, enum Keyword);
void		caen_v7nn_deinit(struct CaenV7nnModule *);
void		caen_v7nn_destroy(struct CaenV7nnModule *);
struct Map	*caen_v7nn_get_map(struct CaenV7nnModule *) FUNC_RETURNS;
void		caen_v7nn_get_signature(struct ModuleSignature const **,
    size_t *);
void		caen_v7nn_init_fast(struct Crate *, struct CaenV7nnModule *);
void		caen_v7nn_init_slow(struct Crate *, struct CaenV7nnModule *);
uint32_t	caen_v7nn_parse_data(struct CaenV7nnModule *, struct
    EventConstBuffer const *, int) FUNC_RETURNS;
uint32_t	caen_v7nn_readout(struct Crate *, struct CaenV7nnModule *,
    struct EventBuffer *) FUNC_RETURNS;
void		caen_v7nn_readout_dt(struct CaenV7nnModule *);
uint32_t	caen_v7nn_readout_shadow(struct CaenV7nnModule *, struct
    EventBuffer *) FUNC_RETURNS;
void		caen_v7nn_use_pedestals(struct CaenV7nnModule *);
void		caen_v7nn_zero_suppress(struct CaenV7nnModule *, int);
struct cmvlc_stackcmdbuf;
void		caen_v7nn_cmvlc_init(struct CaenV7nnModule *,
    struct cmvlc_stackcmdbuf *, int);
uint32_t	caen_v7nn_cmvlc_fetch_dt(struct CaenV7nnModule *,
    const uint32_t *, uint32_t, uint32_t *);
uint32_t	caen_v7nn_cmvlc_fetch(struct Crate *, struct
    CaenV7nnModule *, struct EventBuffer *, const uint32_t *, uint32_t,
    uint32_t *);

#endif
