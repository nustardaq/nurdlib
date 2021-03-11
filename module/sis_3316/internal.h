/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
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

#ifndef MODULE_SIS_3316_INTERNAL_H
#define MODULE_SIS_3316_INTERNAL_H

#include <module/module.h>

#define N_ADCS 4
#define N_CHANNELS 16
#define N_GATES 8
#define N_CH_PER_ADC (N_CHANNELS / N_ADCS)

#define ADD_HALF_PERIOD 0x1000
#define V2003_14BIT_TAP_DELAY_250MHZ 0x48
#define V2003_14BIT_TAP_DELAY_125MHZ 0x50
#define V2003_14BIT_TAP_DELAY_25MHZ  0x20
#define V2004_14BIT_TAP_DELAY_250MHZ 0x02
#define V2004_14BIT_TAP_DELAY_125MHZ 0x50
#define V2004_14BIT_TAP_DELAY_25MHZ  0x20
#define V2004_16BIT_TAP_DELAY_125MHZ 0x20
#define V2004_16BIT_TAP_DELAY_25MHZ  0x30

#define SPI_WRITE	0x80000000
#define SPI_ENABLE	(1 << 24)
#define SPI_CH12	0
#define SPI_CH34	(1 << 22)
#define SPI_OP(addr)	(addr << 8)
#define SPI_OP_OUTPUT_MODE	0x14
#define SPI_OP_INPUT_SPAN	0x18
#define SPI_OP_DITHER_ENABLE	0x30
#define SPI_OP_TRANSFER		0xff
#define SPI_OP_RESET		0x00
#define SPI_AD9634_INPUT_SPAN_1V5	0x15
#define SPI_AD9634_INPUT_SPAN_1V75	0x0
#define SPI_AD9634_INPUT_SPAN_2V	0xB
#define SPI_AD9634_OUTPUT_INVERT	0x4
#define SPI_AD9268_INPUT_SPAN_1V5	0x40
#define SPI_AD9268_INPUT_SPAN_1V75	0x80
#define SPI_AD9268_INPUT_SPAN_2V	0xC0
#define SPI_AD9268_OUTPUT_INVERT	0x4
#define SPI_AD9268_DITHER_ENABLE	0x10
#define SPI_AD9268_OUTPUT_LVDS		0x40

enum BitDepth	{BD_14BIT, BD_16BIT};
enum CheckLevel {CL_OFF, CL_SLOPPY, CL_PARANOID};

struct Gate {
	uint32_t delay;
	uint32_t width;
};

/*
 * Run mode: Either synchronous, i.e. all channels are sampling simultaneously,
 * or asynchronous: Each channel triggers and samples by itself.
 * or asynchronous with automatic bank switching: This requires a cable from UO to UI.
 */
enum RunMode {
	RM_SYNC,
	RM_ASYNC,
	RM_ASYNC_EXTERNAL_GATE,
	RM_ASYNC_AUTO_BANK_SWITCH
};

enum ClockSource {
	CS_INTERNAL,
	CS_VXS,			/* Backplane */
	CS_FP_BUS,		/* LVDS input */
	CS_EXTERNAL
};

struct Sis3316Config {
	uint32_t	address;
	uint32_t	baseline_average;
	uint32_t	baseline_delay;
	uint32_t	header_id[N_ADCS];
	uint8_t		is_fpbus_master;
	uint32_t	pretrigger_delay[N_ADCS];	/* 0,2..16378 */
	uint32_t	pretrigger_delay_maw[N_ADCS];	/* 2,4..1022 */
	uint32_t	pretrigger_delay_maw_e[N_ADCS]; /* 2,4..1022 */
	uint32_t	sample_length[N_ADCS];
	uint32_t	sample_length_maw[N_ADCS];	/* 0,2..2048 */
	uint32_t	sample_length_maw_e[N_ADCS];	/* 0,2..2048 */
	uint32_t	async_max_events;
	uint32_t	event_length[N_ADCS]; /* Gets set on module init() */
	uint32_t	signal_risetime[N_CHANNELS];
	uint32_t	signal_decaytime[N_CHANNELS];
	uint32_t	range[N_ADCS];
	uint8_t		do_readout;
	uint8_t		use_accumulator6;
	uint8_t		use_accumulator2;
	uint8_t		use_maw3;
	uint8_t		use_maxe;
	uint8_t		use_termination;
	uint8_t		use_cfd_trigger;
	uint8_t		use_peak_charge;		/* 0 or 1 */
	uint8_t		use_auto_bank_switch;		/* 0 or 1 */
	uint8_t		use_user_counter;		/* 0 or 1 */
	uint8_t		use_rataclock;			/* 0 or 1 */
	uint8_t		use_rataclock_timestamp;	/* 0 or 1 */
	uint8_t		use_clock_from_vxs;		/* 0 or 1 */
	uint32_t	rataclock_receive_delay;
	uint32_t	rataclock_clk_period;
	uint32_t	rataclock_pulse_period_clk;
	uint32_t	rataclock_use_auto_edge;
	uint32_t	rataclock_expect_edge;
	int		dac_offset[N_CHANNELS];		/* -0x8000..0x8000 */
	uint16_t	pileup_length[N_ADCS];		/* 0,2..65534 */
	uint16_t	re_pileup_length[N_ADCS];	/* 0,2..65534 */
	uint8_t		invert_signal[N_CHANNELS];
	uint32_t	threshold_mV[N_CHANNELS];
	uint32_t	threshold_high_e_mV[N_CHANNELS];
	uint8_t		internal_trigger_delay[N_CHANNELS]; /* 0,2,..2044 */
	uint8_t		peak[N_CHANNELS];		/* 2,4..510 */
	uint8_t		gap[N_CHANNELS];		/* 2,4..510 */
	uint16_t	peak_e[N_CHANNELS];		/* 2,4..2044 */
	uint16_t	gap_e[N_CHANNELS];		/* 2,4..510 */
	uint16_t	energy_pickup[N_CHANNELS];	/* 2,4..2048 */
	uint16_t	histogram_divider[N_CHANNELS];
	uint8_t		extra_filter[N_CHANNELS];	/* 0..3 */
	uint8_t		tau_factor[N_CHANNELS];		/* 0..63 */
	uint8_t		tau_table[N_CHANNELS];		/* 0..3 */
	uint32_t	clk_freq;			/* In MHz */
	uint32_t	ext_clk_freq;			/* In MHz */
	uint32_t	trigger_output;			/* A bitmask */
	uint32_t	use_external_trigger;		/* A bitmask */
	uint32_t	use_external_veto;		/* A bitmask */
	uint32_t	use_internal_trigger;		/* A bitmask */
	uint32_t	use_dual_threshold;		/* A bitmask */
	uint32_t	use_tau_correction;		/* A bitmask */
	uint32_t	channels_to_read;		/* A bitmask */
	uint32_t	use_external_gate;		/* A bitmask */
	uint16_t	discard_data;			/* A bitmask */
	int		discard_threshold;		/* full range */
	struct Gate	gate[N_GATES];
	enum BitDepth	bit_depth;
	enum Keyword	blt_mode;
	enum CheckLevel	check_level;
	enum RunMode	run_mode;
	uint8_t		write_traces_raw;
	uint8_t		write_traces_maw;
	uint8_t		write_traces_maw_energy;
	uint8_t		write_histograms;
	uint8_t		use_dithering;			/* ADC dithering */
	uint32_t	trigger_gate_window_length;	/* calculated */
	uint32_t	threshold[N_CHANNELS];		/* calculated */
	uint32_t	threshold_high_e[N_CHANNELS];	/* calculated */
	uint8_t		has_rataclock_receiver;	/* boolean */
	uint32_t	average_mode[N_ADCS];		/* 0, 4, 8, ... 256 samples */
	uint32_t	average_pretrigger[N_ADCS];	/* 0..4094 samples */
	uint32_t	average_length[N_ADCS];		/* 0, 2, 4, ... 65534 samples */
	int		tap_delay_fine_tune;		/* [-32 .. 32] * 40 ps */
	uint32_t	n_memtest_bursts;	/* number of memtest bursts */
};

struct Sis3316ChannelCounters {
	uint32_t internal;
	uint32_t hit;
	uint32_t deadtime;
	uint32_t pileup;
	uint32_t veto;
	uint32_t high_energy;
};

struct Sis3316Statistic {
	struct Sis3316ChannelCounters ch[N_CHANNELS];
};

struct Sis3316Module {
	struct		Module module;
	struct		Sis3316Config config;
	struct		Map *sicy_map;
	struct		Map *dma_map;
	int		current_bank;
	struct		Sis3316Statistic stat;
	struct		Sis3316Statistic stat_prev;
	struct		Sis3316Statistic stat_prev_dump;
	uint32_t	max_num_hits;
	uint32_t	num_hits;
	double		last_dumped;
	double		last_read;
};

void	sis_3316_calculate_settings(struct Sis3316Module *);
void    sis_3316_calculate_dac_offsets(struct Sis3316Module *);
void	sis_3316_get_config(struct Sis3316Module *, struct ConfigBlock *);
void	sis_3316_reset(struct Sis3316Module *);

#endif
