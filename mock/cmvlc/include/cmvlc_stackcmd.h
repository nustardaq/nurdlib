/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2025
 * Håkan T. Johansson
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

#ifndef CMVLC_STACKCMD_H
#define CMVLC_STACKCMD_H

#include <cmvlc.h>

enum cmvlc_stack_out_pipe {
	stack_out_pipe_command,
	stack_out_pipe_data
};

enum cmvlc_write_special {
	wrspec_timestamp,
	wrspec_accu
};

struct cmvlc_stackcmdbuf {
	int dummy;
};

void cmvlc_stackcmd_init(struct cmvlc_stackcmdbuf *);
void cmvlc_stackcmd_start(struct cmvlc_stackcmdbuf *,
			  enum cmvlc_stack_out_pipe);
void cmvlc_stackcmd_end(struct cmvlc_stackcmdbuf *);
void cmvlc_stackcmd_vme_rw(struct cmvlc_stackcmdbuf *,
			   uint32_t, uint32_t,
			   enum cmvlc_vme_rw_op,
			   enum cmvlc_vme_addr_mode,
			   enum cmvlc_vme_data_width);
void cmvlc_stackcmd_vme_block(struct cmvlc_stackcmdbuf *,
			      uint32_t,
			      enum cmvlc_vme_rw_op,
			      enum cmvlc_vme_addr_mode,
			      uint16_t);
void cmvlc_stackcmd_write_special(struct cmvlc_stackcmdbuf *,
				  enum cmvlc_write_special);
void cmvlc_stackcmd_marker(struct cmvlc_stackcmdbuf *,
			   uint32_t);
void cmvlc_stackcmd_wait(struct cmvlc_stackcmdbuf *,
			 uint32_t);
void cmvlc_stackcmd_wait_ns(struct cmvlc_stackcmdbuf *,
			    uint32_t);
void cmvlc_stackcmd_mask_rotate_accu(struct cmvlc_stackcmdbuf *,
				     uint32_t,
				     uint8_t);
void cmvlc_stackcmd_signal_accu(struct cmvlc_stackcmdbuf *);
void cmvlc_stackcmd_set_accu(struct cmvlc_stackcmdbuf *,
			     uint32_t);

#endif
