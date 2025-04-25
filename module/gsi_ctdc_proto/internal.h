/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2025
 * Manuel Xarepe
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

#ifndef MODULE_GSI_CTDC_PROTO_INTERNAL_H
#define MODULE_GSI_CTDC_PROTO_INTERNAL_H

#include <module/module.h>

#define REG_PADI_SPI_CTRL          0x200040
#define REG_PADI_SPI_DATA          0x200044

struct GsiPex;

typedef int (*GsiCTDCSetThreshold)(struct ConfigBlock *, struct GsiPex *,
    unsigned, unsigned, int32_t, uint16_t const *) FUNC_RETURNS;

struct GsiCTDCProtoCard {
	struct	ConfigBlock *config;
	enum	Keyword frontend;
	unsigned	frequency;
};
struct GsiCTDCProtoModule {
	struct	Module module;
	size_t	sfp_i;
	size_t	card_num;
	struct	GsiCTDCProtoCard *card_array;
	struct	ConfigBlock const *config;
	GsiCTDCSetThreshold     threshold_set;
};

void			gsi_ctdc_proto_create(struct ConfigBlock *, struct
    GsiCTDCProtoModule *, enum Keyword);
void			gsi_ctdc_proto_destroy(struct GsiCTDCProtoModule *);
struct ConfigBlock	*gsi_ctdc_proto_get_submodule_config(struct
    GsiCTDCProtoModule *, unsigned) FUNC_RETURNS;
int			gsi_ctdc_proto_init_fast(struct Crate *, struct
    GsiCTDCProtoModule *, unsigned const *) FUNC_RETURNS;
void			gsi_ctdc_proto_init_slow(struct Crate *, struct
    GsiCTDCProtoModule *);
uint32_t		gsi_ctdc_proto_parse_data(struct Crate const *, struct
    GsiCTDCProtoModule *, struct EventConstBuffer const *) FUNC_RETURNS;
uint32_t		gsi_ctdc_proto_readout(struct Crate *, struct
    GsiCTDCProtoModule *, struct EventBuffer *) FUNC_RETURNS;
int			gsi_ctdc_proto_sub_module_pack(struct
    GsiCTDCProtoModule *, struct PackerList *) FUNC_RETURNS;

#endif
