/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
 * Bastian Löher
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

#ifndef MODULE_CAEN_V1N90_INTERNAL_H
#define MODULE_CAEN_V1N90_INTERNAL_H

#include <module/module.h>

enum ParseExpect {
	EXPECT_DMA_HEADER,
	EXPECT_TDC_HEADER,
	EXPECT_TDC_PAYLOAD,
	EXPECT_TIMETAG_OR_TRAILER_OR_TDC_HEADER,
	EXPECT_TRAILER
};

struct ModuleList;

struct CaenV1n90Module {
	struct	Module module;

	uint32_t	address;
	uint16_t	edge_code;
	uint16_t	resolution_code;
	uint16_t	deadtime_code;
	uint16_t	gate_offset;
	uint16_t	gate_width;
	enum	Keyword blt_mode;

	struct	Map *sicy_map;
	struct	Map *dma_map;

	uint32_t	header_counter;
	int	was_full;

	struct {
		enum	ParseExpect expect;
	} parse;

	TAILQ_ENTRY(CaenV1n90Module)	next;
};

uint32_t	caen_v1n90_check_empty(struct CaenV1n90Module *) FUNC_RETURNS;
void		caen_v1n90_create(struct ConfigBlock const *, struct
    CaenV1n90Module *);
void		caen_v1n90_deinit(struct CaenV1n90Module *);
void		caen_v1n90_destroy(struct CaenV1n90Module *);
struct Map	*caen_v1n90_get_map(struct CaenV1n90Module *) FUNC_RETURNS;
void		caen_v1n90_get_signature(struct ModuleSignature const **,
    size_t *);
void		caen_v1n90_init_fast(struct CaenV1n90Module *, enum Keyword);
void		caen_v1n90_init_slow(struct CaenV1n90Module *);
int		caen_v1n90_micro_init_fast(struct ModuleList const *);
int		caen_v1n90_micro_init_slow(struct ModuleList const *);
uint32_t	caen_v1n90_parse_data(struct CaenV1n90Module *, struct
    EventConstBuffer const *, int) FUNC_RETURNS;
uint32_t	caen_v1n90_readout(struct Crate *, struct CaenV1n90Module *,
    struct EventBuffer *) FUNC_RETURNS;
uint32_t	caen_v1n90_readout_dt(struct CaenV1n90Module *) FUNC_RETURNS;
void		caen_v1n90_register_list_pack(struct CaenV1n90Module *, struct
    PackerList *, unsigned);
void		caen_v1n90_zero_suppress(struct CaenV1n90Module *, int);

#endif
