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

#include <cmvlc.h>
#include <cmvlc_stackcmd.h>
#include <cmvlc_supercmd.h>

int
cmvlc_close(struct cmvlc_client *a)
{
	(void)a;
	return 0;
}

struct cmvlc_client *
cmvlc_connect(const char *a_a, int a_b, const char **a_c, FILE *a_d,
    FILE *a_e)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	return NULL;
}

const char *
cmvlc_last_error(struct cmvlc_client *a)
{
	(void)a;
	return NULL;
}

int
cmvlc_single_vme_read(struct cmvlc_client *a_a, uint32_t a_b, uint32_t *a_c,
    enum cmvlc_vme_addr_mode a_d, enum cmvlc_vme_data_width a_e)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	return 0;
}

int
cmvlc_single_vme_write(struct cmvlc_client *a_a, uint32_t a_b, uint32_t a_c,
    enum cmvlc_vme_addr_mode a_d, enum cmvlc_vme_data_width a_e)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	return 0;
}

int
cmvlc_set_daq_mode(struct cmvlc_client *a_a, int a_b, int a_c,
    uint8_t (*a_d)[2], int a_e, uint8_t a_f)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	(void)a_f;
	return 0;
}

int cmvlc_block_get(struct cmvlc_client *a_a, const uint32_t *a_b,
    size_t a_c,  size_t *a_d, uint32_t *a_e, size_t a_f, size_t *a_g)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	(void)a_f;
	(void)a_g;
	return 0;
}

/* cmvlc_stackcmd_...() */

void cmvlc_stackcmd_init(struct cmvlc_stackcmdbuf *a_a)
{
	(void)a_a;
	return 0;
}

void cmvlc_stackcmd_start(struct cmvlc_stackcmdbuf *a_a,
			  enum cmvlc_stack_out_pipe a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

void cmvlc_stackcmd_end(struct cmvlc_stackcmdbuf *a_a)
{
	(void)a_a;
	return 0;
}

void cmvlc_stackcmd_vme_rw(struct cmvlc_stackcmdbuf *a_a,
			   uint32_t a_b, uint32_t a_c,
			   enum cmvlc_vme_rw_op a_d,
			   enum cmvlc_vme_addr_mode a_e,
			   enum cmvlc_vme_data_width a_f)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	(void)a_f;
	return 0;
}

void cmvlc_stackcmd_vme_block(struct cmvlc_stackcmdbuf *a_a,
			      uint32_t a_b,
			      enum cmvlc_vme_rw_op a_c,
			      enum cmvlc_vme_addr_mode a_d,
			      uint16_t a_e)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	(void)a_e;
	return 0;
}

void cmvlc_stackcmd_write_special(struct cmvlc_stackcmdbuf *a_a,
				  enum cmvlc_write_special a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

void cmvlc_stackcmd_marker(struct cmvlc_stackcmdbuf *a_a,
			   uint32_t a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

void cmvlc_stackcmd_wait(struct cmvlc_stackcmdbuf *a_a,
			 uint32_t a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

void cmvlc_stackcmd_wait_ns(struct cmvlc_stackcmdbuf *a_a,
			    uint32_t a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

void cmvlc_stackcmd_mask_rotate_accu(struct cmvlc_stackcmdbuf *a_a,
				     uint32_t a_b,
				     uint8_t a_c)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	return 0;
}

void cmvlc_stackcmd_signal_accu(struct cmvlc_stackcmdbuf *a_a)
{
	(void)a_a;
	return 0;
}

void cmvlc_stackcmd_set_accu(struct cmvlc_stackcmdbuf *a_a,
			     uint32_t a_b)
{
	(void)a_a;
	(void)a_b;
	return 0;
}

int cmvlc_mvlc_write(struct cmvlc_client *a_a,
    uint16_t a_b, uint32_t a_c)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	return 0;
}

void cmvlc_reset_stacks(struct cmvlc_client *a_a)
{
	(void)a_a;
}

int cmvlc_setup_stack(struct cmvlc_client *a_a,
    struct cmvlc_stackcmdbuf *a_b, int a_c, uint8_t a_d)
{
	(void)a_a;
	(void)a_b;
	(void)a_c;
	(void)a_d;
	return 0;
}

int cmvlc_readout_attach(struct cmvlc_client *a_a)
{
	(void)a_a;
	return 0;
}

int cmvlc_readout_reset(struct cmvlc_client *a_a)
{
	(void)a_a;
	return 0;
}
