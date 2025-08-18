/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2025
 * Håkan T Johansson
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

#ifndef CMVLC_H
#define CMVLC_H

#include <stdint.h>
#include <stdio.h>

#define CMVLC_CONNECT_FLAGS_DATA 1

enum cmvlc_vme_addr_mode {
	vme_user_A16,
	vme_user_A24,
	vme_user_A32,
	vme_user_CR,
	vme_user_BLT_A24,
	vme_user_BLT_A32,
	vme_user_MBLT_A32
};
enum cmvlc_vme_data_width {
	vme_D16,
	vme_D32
};

enum cmvlc_vme_rw_op {
	vme_rw_read,
	vme_rw_read_swap,
	vme_rw_read_mem,
	vme_rw_read_swap_mem,
	vme_rw_read_to_accu,
	vme_rw_write
};

struct cmvlc_client {
	int	dummy;
};

struct cmvlc_stackcmdbuf;

struct cmvlc_event_info
{
	uint8_t  _errors;
	uint8_t  _stacknum;
	uint8_t  _ctrlid;
};

int			cmvlc_close(struct cmvlc_client *);
struct cmvlc_client	*cmvlc_connect(const char *, int, const char **, FILE
    *, FILE *);
const char		*cmvlc_last_error(struct cmvlc_client *);

int			cmvlc_single_vme_read(struct cmvlc_client *, uint32_t,
    uint32_t *, enum cmvlc_vme_addr_mode, enum cmvlc_vme_data_width);
int			cmvlc_single_vme_write(struct cmvlc_client *,
    uint32_t, uint32_t, enum cmvlc_vme_addr_mode, enum cmvlc_vme_data_width);
int			cmvlc_set_daq_mode(struct cmvlc_client *, int, int,
    uint8_t (*)[2], int, uint8_t);
int			cmvlc_block_get(struct cmvlc_client *,
    const uint32_t *, size_t,  size_t *, uint32_t *, size_t, size_t *);

int			cmvlc_mvlc_write(struct cmvlc_client *,
    uint16_t, uint32_t);

void			cmvlc_reset_stacks(struct cmvlc_client *);
int			cmvlc_setup_stack(struct cmvlc_client *,
    struct cmvlc_stackcmdbuf *, int, uint8_t);

int			cmvlc_readout_attach(struct cmvlc_client *);
int			cmvlc_readout_reset(struct cmvlc_client *);

int			cmvlc_readout_get_event(struct cmvlc_client *,
    uint32_t *, size_t, size_t *, struct cmvlc_event_info *);

#endif
