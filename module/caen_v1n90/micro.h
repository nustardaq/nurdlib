/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019, 2024
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

#ifndef MODULE_CAEN_V1N90_MICRO_H
#define MODULE_CAEN_V1N90_MICRO_H

#include <module/caen_v1n90/internal.h>

struct CaenV1n90Module;

uint16_t	micro_read(struct CaenV1n90Module const *, int *)
	FUNC_RETURNS;
void		micro_write(struct ModuleList const *, uint16_t, int *);
void		micro_write_member(struct ModuleList const *, uintptr_t, int
    *);

enum {
	/* Acquisition mode */
	MICRO_TRIG_MATCH         = 0x0000,
	MICRO_CONT_STOR          = 0x0100,
	MICRO_READ_ACQ_MOD       = 0x0200, /* Read 1 words */
	MICRO_SET_KEEP_TOKEN     = 0x0300,
	MICRO_CLEAR_KEEP_TOKEN   = 0x0400,
	MICRO_LOAD_DEF_CONFIG    = 0x0500,
	MICRO_SAVE_USER_CONFIG   = 0x0600,
	MICRO_LOAD_USER_CONFIG   = 0x0700,
	MICRO_AUTOLOAD_USER_CONF = 0x0800,
	MICRO_AUTOLOAD_DEF_CONF  = 0x0900,


	/* Trigger */
	MICRO_SET_WIN_WIDTH      = 0x1000, /* Write 12 bit */
	MICRO_SET_WIN_OFFSET     = 0x1100, /* Write 12 bit */
	MICRO_SET_SW_MARGIN      = 0x1200, /* Write 12 bit */
	MICRO_SET_REJ_MARGIN     = 0x1300, /* Write 12 bit */
	MICRO_EN_SUB_TRG         = 0x1400,
	MICRO_DIS_SUB_TRG        = 0x1500,
	MICRO_READ_TRG_CONF      = 0x1600, /* Read 5 words */

	/* TDC EDGE DETECTION & RESOLUTION */
	MICRO_SET_DETECTION      = 0x2200, /* Write 2 bits */
	MICRO_READ_DETECTION     = 0x2300, /* Read 1 word */
	MICRO_SET_TR_LEAD_LSB    = 0x2400, /* Write 2 bits */
	MICRO_SET_PAIR_RES       = 0x2500, /* Write 16 bits */
	MICRO_READ_RES           = 0x2600, /* Read 1 word */
	MICRO_SET_DEAD_TIME      = 0x2800, /* Write 2 bits*/
	MICRO_READ_DEAD_TIME     = 0x2900, /* Read 1 word*/


	/* TDC READOUT */
	MICRO_EN_HEAD_TRAIL      = 0x3000,
	MICRO_DIS_HEAD_TRAIL     = 0x3100,
	MICRO_READ_HEAD_TRAIL    = 0x3200, /* Read 1 word */
	MICRO_SET_EVENT_SIZE     = 0x3300, /* Write 1 word*/
	MICRO_READ_EVENT_SIZE    = 0x3400, /* Read 1 word */
	MICRO_EN_ERROR_MARK      = 0x3500,
	MICRO_DIS_ERROR_MARK     = 0x3600,
	MICRO_EN_ERROR_BYPASS    = 0x3700,
	MICRO_DIS_ERROR_BYPASS   = 0x3800,
	MICRO_SET_ERROR_TYPES    = 0x3900,
	MICRO_READ_ERROR_TYPES   = 0x3A00,
	MICRO_SET_FIFO_SIZE      = 0x3B00,
	MICRO_READ_FIFO_SIZE     = 0x3C00,

	/* CHANNEL ENABLE */
	MICRO_EN_CHANNEL         = 0x4000, /* Channel encoded in low 16 bits */
	MICRO_DIS_CHANNEL        = 0x4100, /* Channel encoded in low 16 bits */
	MICRO_EN_ALL_CH          = 0x4200,
	MICRO_DIS_ALL_CH         = 0x4300,
	MICRO_READ_EN_CHANNEL    = 0x4500,

	/* DEBUG AND TEST */
	MICRO_READ_FIRMWARE      = 0x6100,
	MICRO_WRITE_SPARE        = 0xC300,
	MICRO_READ_SPARE         = 0xC400 /* Read 16 bits */
};


/* Maximum hits per event (Section 5.5.4) */
enum {
	MICRO_EVENT_SIZE_0   = 0x0,
	MICRO_EVENT_SIZE_1   = 0x1,
	MICRO_EVENT_SIZE_2   = 0x2,
	MICRO_EVENT_SIZE_4   = 0x3,
	MICRO_EVENT_SIZE_8   = 0x4,
	MICRO_EVENT_SIZE_16  = 0x5,
	MICRO_EVENT_SIZE_32  = 0x6,
	MICRO_EVENT_SIZE_64  = 0x7,
	MICRO_EVENT_SIZE_128 = 0x8,
	MICRO_EVENT_SIZE_INF = 0x9
	/* Higher than 9 is meaningless */
};

/* Readout FIFO size (Section 5.5.12) */
enum {
	MICRO_FIFO_SIZE_2   = 0x0,
	MICRO_FIFO_SIZE_4   = 0x1,
	MICRO_FIFO_SIZE_8   = 0x2,
	MICRO_FIFO_SIZE_16  = 0x3,
	MICRO_FIFO_SIZE_32  = 0x4,
	MICRO_FIFO_SIZE_64  = 0x5,
	MICRO_FIFO_SIZE_128 = 0x6,
	MICRO_FIFO_SIZE_256 = 0x7
	/* Higher than 8 is meaningless */
};

/* Acq mode (Section 5.2.3) */
enum {
	MICRO_ACQ_MODE_TRIG = 0x1,
	MICRO_ACQ_MODE_CONT = 0x0
};

#endif
