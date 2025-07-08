/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#ifndef MODULE_MESYTEC_MDPP_INTERNAL_H
#define MODULE_MESYTEC_MDPP_INTERNAL_H

#include <module/mesytec_mxdc32/internal.h>

struct MesytecMdppConfig {
	unsigned	ch_num;
	uint16_t	threshold[32];
	enum	Keyword nim[5];
	enum	Keyword ecl[4];
	enum	Keyword ecl_term[4];
	int	use_ext_clk;
	int	do_first_hit;
	int	monitor_channel;
	int	monitor_wave;
	int	monitor_on;
	uint16_t	gate_offset;
	uint16_t	gate_width;
	int	do_auto_pedestals;
	int	pulser_enabled;
	int	pulser_amplitude;
	int	trigger_output;
	int	trigger_input;
	/* This is only available on mdpp16* so 16 bits is enough. */
	uint16_t	trigger_mask;
};

struct MesytecMdppModule {
	struct	MesytecMxdc32Module mxdc32;
	struct	MesytecMdppConfig config;
};

void		mesytec_mdpp_create_(struct ConfigBlock *, struct
    MesytecMdppModule *, enum Keyword);
void		mesytec_mdpp_deinit(struct MesytecMdppModule *);
void		mesytec_mdpp_destroy(struct MesytecMdppModule *);
struct Map	*mesytec_mdpp_get_map(struct MesytecMdppModule *)
	FUNC_RETURNS;
void		mesytec_mdpp_get_signature(struct ModuleSignature const **,
    size_t *);
void		mesytec_mdpp_init_fast(struct Crate *, struct
    MesytecMdppModule *);
void		mesytec_mdpp_init_slow(struct Crate const *, struct
    MesytecMdppModule *);
void        mesytec_mdpp_memtest(struct Module *, enum Keyword);
uint32_t	mesytec_mdpp_parse_data(struct Crate *, struct
    MesytecMdppModule *, struct EventConstBuffer const *, int, int)
	FUNC_RETURNS;
int		mesytec_mdpp_post_init(struct MesytecMdppModule *)
	FUNC_RETURNS;
uint32_t	mesytec_mdpp_readout(struct Crate *, struct MesytecMdppModule
    *, struct EventBuffer *, int) FUNC_RETURNS;
uint32_t	mesytec_mdpp_readout_dt(struct Crate *, struct
    MesytecMdppModule *) FUNC_RETURNS;
uint32_t	mesytec_mdpp_readout_shadow(struct MesytecMdppModule *, struct
    EventBuffer *) FUNC_RETURNS;
void		mesytec_mdpp_use_pedestals(struct MesytecMdppModule *);
void		mesytec_mdpp_zero_suppress(struct MesytecMdppModule *, int);

#endif
