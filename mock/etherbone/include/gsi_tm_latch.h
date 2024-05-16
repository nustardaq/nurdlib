/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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

#ifndef MOCK_GSI_TM_LATCH_H
#define MOCK_GSI_TM_LATCH_H

enum {
	GSI_TM_LATCH_FIFO_CLEAR,
	GSI_TM_LATCH_FIFO_CNT,
	GSI_TM_LATCH_FIFO_FTSHI,
	GSI_TM_LATCH_FIFO_FTSLO,
	GSI_TM_LATCH_FIFO_FTSSUB,
	GSI_TM_LATCH_FIFO_READY,
	GSI_TM_LATCH_FIFO_POP,
	GSI_TM_LATCH_CH_SELECT,
	GSI_TM_LATCH_CHNS_FIFOSIZE,
	GSI_TM_LATCH_PRODUCT,
	GSI_TM_LATCH_TRIG_ARMSET,
	GSI_TM_LATCH_VENDOR
};

#endif
