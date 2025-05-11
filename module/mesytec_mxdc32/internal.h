/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
 * Bastian Löher
 * Michael Munch
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

#ifndef MODULE_MESYTEC_MXDC32_INTERNAL_H
#define MODULE_MESYTEC_MXDC32_INTERNAL_H

#include <module/module.h>

#define MESYTEC_MXDC32_USE_PEDESTALS(mod) \
	do { \
		struct Pedestal const *pedestal_array; \
		size_t i_; \
		uint32_t mask; \
		pedestal_array = mod->mxdc32.module.pedestal.array; \
		mask = mod->mxdc32.channel_mask; \
		for (i_ = 0; LEN_threshold(0) > i_; ++i_) { \
			if (0 != (1 & mask)) { \
				MAP_WRITE(mod->mxdc32.sicy_map, \
				    threshold(i_), \
				    pedestal_array[i_].threshold); \
			} \
			mask >>= 1; \
		} \
	} while (0)
#define MESYTEC_MXDC32_SET_THRESHOLDS(mod, array)\
	do {\
		char str[64];\
		size_t i_, ofs_;\
		uint32_t mask;\
		LOGF(verbose)(LOGL, "Thresholds {");\
		ofs_ = 0;\
		mask = mod->mxdc32.channel_mask;\
		for (i_ = 0; i_ < LENGTH(array);) {\
			if (0 == (1 & mask)) {\
				array[i_] = 0x1fff;\
				ofs_ += snprintf_(str + ofs_, sizeof str -\
				    ofs_, " -off-");\
			} else {\
				ofs_ += snprintf_(str + ofs_, sizeof str -\
				    ofs_, " 0x%03x", array[i_]);\
			}\
			MAP_WRITE(mod->mxdc32.sicy_map, threshold(i_), \
			    array[i_]);\
			++i_;\
			if (0 == (7 & i_)) {\
				LOGF(verbose)(LOGL, "%s", str);\
				ofs_ = 0;\
			}\
			mask >>= 1;\
		}\
		LOGF(verbose)(LOGL, "Thresholds }");\
	} while (0)


#define MESYTEC_BANK_OP_CONNECTED	0x1
#define MESYTEC_BANK_OP_INDEPENDENT	0x2
#define MESYTEC_BANK_OP_TOGGLE		0x4
#define MESYTEC_BANK_OP_MASK		0x7

#define DMA_FILLER 0x32323232

struct MesytecMxdc32Module {
	struct	Module module;
	uint32_t	address;
	struct	Map *sicy_map;
	struct	Map *dma_map;
	uint32_t	channel_mask;
	enum	Keyword blt_mode;
	uint16_t data_len_format;
	unsigned	datum_bytes;
	uint32_t	parse_counter;
	unsigned	buffer_data_length;
	int	do_sleep;
};

uint32_t	mesytec_mxdc32_check_empty(struct MesytecMxdc32Module *)
	FUNC_RETURNS;
void		mesytec_mxdc32_create(struct ConfigBlock const *, struct
    MesytecMxdc32Module *);
void		mesytec_mxdc32_deinit(struct MesytecMxdc32Module *);
void		mesytec_mxdc32_destroy(struct MesytecMxdc32Module *);
struct Map	*mesytec_mxdc32_get_map(struct MesytecMxdc32Module *)
	FUNC_RETURNS;
void		mesytec_mxdc32_init_fast(struct Crate *, struct
    MesytecMxdc32Module *, int);
void		mesytec_mxdc32_init_slow(struct MesytecMxdc32Module *);
uint32_t	mesytec_mxdc32_parse_data(struct Crate *, struct
    MesytecMxdc32Module *, struct EventConstBuffer const *, int, int, int)
	FUNC_RETURNS;
int		mesytec_mxdc32_post_init(struct MesytecMxdc32Module *)
	FUNC_RETURNS;
uint32_t	mesytec_mxdc32_readout(struct Crate *, struct
    MesytecMxdc32Module *, struct EventBuffer *, int, int) FUNC_RETURNS;
uint32_t	mesytec_mxdc32_readout_dt(struct Crate *, struct
    MesytecMxdc32Module *) FUNC_RETURNS;
uint32_t	mesytec_mxdc32_readout_shadow(struct MesytecMxdc32Module *,
    struct EventBuffer *, int) FUNC_RETURNS;
double		mesytec_mxdc32_sleep_get(struct MesytecMxdc32Module *)
	FUNC_RETURNS;
void		mesytec_mxdc32_zero_suppress(struct MesytecMxdc32Module *,
    int);
struct cmvlc_stackcmdbuf;
void		mesytec_mxdc32_cmvlc_init(struct MesytecMxdc32Module *,
    struct cmvlc_stackcmdbuf *, int);
uint32_t	mesytec_mxdc32_cmvlc_fetch_dt(struct MesytecMxdc32Module *,
    const uint32_t *, uint32_t, uint32_t *);
uint32_t	mesytec_mxdc32_cmvlc_fetch(struct Crate *, struct
    MesytecMxdc32Module *, struct EventBuffer *, const uint32_t *, uint32_t,
    uint32_t *);

#endif
