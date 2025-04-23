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

#ifndef MOCK_TRIDI_FUNCTIONS_H
#define MOCK_TRIDI_FUNCTIONS_H

#define TRIDI_MUX_DEST_NIM_OUT(x) x

#define TRIDI_MUX_SRC_ACCEPT_TRIG(x) x
#define TRIDI_MUX_SRC_ALL_OR(x) x
#define TRIDI_MUX_SRC_COINCIDENCE(x) x
#define TRIDI_MUX_SRC_GATE_DELAY(x) x
#define TRIDI_MUX_SRC_INPUT_COINC(x) x
#define TRIDI_MUX_SRC_PRNG_POISSON(x) x
#define TRIDI_MUX_SRC_PULSER(x) x
#define TRIDI_MUX_SRC_ECL_IN(x) x
#define TRIDI_MUX_SRC_NIM_IN(x) x

#define TRIDI_READ(a, b) ((tridi_opaque volatile *)a)->b
#define TRIDI_WRITE(a, b, c) ((tridi_opaque volatile *)a)->b = c

#define TRIDI_ACC_ALL_OR_CLEAR(a, b)
#define TRIDI_ACC_ALL_OR_SET(a, b, c)

enum {
	TRIDI_ACC_ALL_OR_CLEAR,

	TRIDI_MUX_DEST_NIM_OUT,
	TRIDI_MUX_DEST_SERIAL_TSTAMP_IN,

	TRIDI_MUX_SRC_ACCEPT_PULSE,
	TRIDI_MUX_SRC_MASTER_START,
	TRIDI_MUX_SRC_SERIAL_TSTAMP_OUT,

	TRIDI_NUM_MUX_SRC_ACCEPT_TRIG,
	TRIDI_NUM_MUX_SRC_ALL_OR,
	TRIDI_NUM_MUX_SRC_COINCIDENCE,
	TRIDI_NUM_MUX_SRC_ECL_IN,
	TRIDI_NUM_MUX_SRC_INPUT_COINC,
	TRIDI_NUM_MUX_SRC_NIM_IN,
	TRIDI_NUM_MUX_SRC_PRNG_POISSON,
	TRIDI_NUM_MUX_SRC_PULSER,
	TRIDI_MUX_SRC_WIRED_ZERO,

	TRIDI_PULSE_MULTI_TRIG_BUF_CLEAR,
	TRIDI_PULSE_MUX_DESTS,
	TRIDI_PULSE_MUX_SRC_SCALER_LATCH,
	TRIDI_PULSE_SERIAL_TSTAMP_BUF_CLEAR,
	TRIDI_PULSE_SERIAL_TSTAMP_FAIL_CLEAR,

	TRIDI_SLEW_COUNTER_ADD_OFFSET_HI,
	TRIDI_SLEW_COUNTER_ADD_OFFSET_LO
};

typedef struct tridi_opaque_t {
	struct {
		uint32_t	serial_timestamp_status;
		uint32_t	slew_counter_cur_hi;
		uint32_t	slew_counter_cur_lo;
	} out;
	struct {
		uint32_t	pulse;
		uint32_t	slew_counter;
	} pulse;
	struct {
		uint32_t	mux_src[100];
	} scaler;
	uint32_t serial_tstamp[100];
	struct {
		uint32_t	max_multi_trig;
		uint32_t	mux[100];
		uint32_t	serial_timestamp_speed;
		uint32_t	slew_counter_add;
		uint32_t	slew_counter_offset;
		uint32_t	stretch[100];
	} setup;
	struct {
		uint32_t	ctime;
		uint32_t	fcatime;
	} trimi;
} tridi_opaque;
struct tridi_readout_control {
	int	dummy;
};

volatile tridi_opaque *tridi_setup_map_hardware(void *, int, void **);
void tridi_unmap_hardware(void *);

void tridi_setup_check_version(volatile tridi_opaque *);
void tridi_setup_readout_control(volatile tridi_opaque *, struct
    tridi_readout_control *, uint32_t, uint32_t, uint32_t, uint32_t);

#endif
