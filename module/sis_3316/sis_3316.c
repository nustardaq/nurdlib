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

#include <module/sis_3316/sis_3316.h>
#include <stdio.h>
#include <string.h>
#include <module/map/map.h>
#include <module/sis_3316/sis_3316_i2c.h>
#include <module/sis_3316/internal.h>
#include <module/sis_3316/offsets.h>
#include <module/sis_3316/tau_table.h>
#include <nurdlib/crate.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <nurdlib/serialio.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/time.h>

/*
 * Options
 */

/*
 * Enabled: Necessary to have FIFOs in good state.
 */
#define DO_RESET_FSM 1

/*
 * Disabled: Dangerous, and not wanted
 * This should only be enabled, if modules are
 * not synchronised via the FP bus.
 */
/* #define DO_CLEAR_TIMESTAMP */

#define POLL_OK 0
#define POLL_TIMEOUT 1
#define ADC_MEM_OFFSET 0x100000

#define NAME "sis3316"
#define REQUIRED_FIRMWARE 0x3316200e /* user counter */

#define ADDRESS_THR_ADC_FLAG(adc) (1 << (25 + 2 * (adc)))
#define ADDRESS_THR_FPBUS_FLAG (1 << 21)
#define ADDRESS_THR_FLAG (1 << 19)
#define BANK2_ARMED_FLAG (1 << 17)
#define SAMPLE_LOGIC_ARMED_FLAG (1 << 16)
#define SAMPLE_LOGIC_BUSY_FLAG (1 << 18)
#define UI_AS_BANK_SWAP_FLAG (1 << 13)

/* TODO: Is the following comment relevant? */
/* Allow 1 MiB for all channels */
const char *g_clock_source[] =
{
	"internal",
	"vxs",
	"fp_bus",
	"external"
};

extern struct tau_entry tau_14bit_125MHz[];
extern struct tau_entry tau_14bit_250MHz[];
extern struct tau_entry tau_16bit_125MHz[];

/*
 * External clock divider:
 * If 250 MHz is coming in, and should be scaled to 125 MHz:
 *   ratio 1/2
 *   n1_hs = 10
 *   nc1_ls = 4
 *   n2_ls = 40
 *   n31 = 2
 *   f3 = 125 MHz
 *   bw_sel = 2 (872 kHz)
 *   clk_in from 242 MHz to 283 MHz
 *   clk_out range from 121 to 141 MHz
 *   Registers:
 *     n1_hs = 6
 *     nc1_ls = 3
 *     nc2_ls = 3
 *     n2_ls = 0x28
 *     n2_hs = 6
 *     n31 = 1
 *     n32 = 1
 */

/*
 * Since the module is broken and does not properly
 * tell, when the FIFO is ready to be read out
 * we have to wait some arbitrary minimum time.
 * This is done using dummy VME reads.
 */
/* Rimfaxe is fast, set this to 1 for sicy */
#define N_DUMMY_READS_AFTER_FSM 1

enum Sis3316FsmMode {SIS3316_FSM_READ, SIS3316_FSM_WRITE};

int g_n_channels;

MODULE_PROTOTYPES(sis_3316);
static uint32_t sis_3316_readout_shadow(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
void sis_3316_swap_banks(struct Sis3316Module*);
void sis_3316_test_clock_sync(struct Sis3316Module*);
int sis_3316_poll_addr_threshold(struct Sis3316Module*) FUNC_RETURNS;
int sis_3316_test_addr_threshold(struct Sis3316Module*) FUNC_RETURNS;
int sis_3316_poll_sample_logic_ready(struct Sis3316Module *m) FUNC_RETURNS;
int sis_3316_poll_prev_bank(struct Sis3316Module*, int) FUNC_RETURNS;
void sis_3316_start_fsm(struct Sis3316Module*, int, enum Sis3316FsmMode);
uint32_t sis_3316_test_channel(struct Sis3316Module*, int, uint32_t, uint32_t
    *, size_t, uint32_t) FUNC_RETURNS;
uint32_t sis_3316_read_channel(struct Sis3316Module*, int, uint32_t, struct
    EventBuffer *) FUNC_RETURNS;
uint32_t sis_3316_read_channel_dma(struct Sis3316Module*, int, uint32_t,
    struct EventBuffer *) FUNC_RETURNS;
void sis_3316_read_stat_counters(struct Sis3316Module *);
int sis_3316_check_channel_data(struct Sis3316Module*, int, struct
    EventConstBuffer *) FUNC_RETURNS;
uint32_t sis_3316_check_channel_padding(struct Sis3316Module*, int, struct
    EventConstBuffer *);
void sis_3316_check_hit(struct Sis3316Module *, int, int, struct
    EventConstBuffer *);
void sis_3316_dump_data(void const *, void const *);
void sis_3316_dump_stat_counters(struct Sis3316Module *, int);
uint32_t sis_3316_get_event_counter(struct Sis3316Module *) FUNC_RETURNS;
int sis_3316_post_init(struct Crate *, struct Module *);
void sis_3316_set_iob_delay_logic(struct Sis3316Module *, int);
void sis_3316_set_sample_clock_dist(struct Sis3316Module *);
void sis_3316_setup_averaging_mode(struct Sis3316Module *);
void sis_3316_setup_event_config(struct Sis3316Module *);
void sis_3316_setup_data_format(struct Sis3316Module *);
void sis_3316_set_offset(struct Sis3316Module *, int );
void sis_3316_setup_adcs(struct Sis3316Module *);
void sis_3316_set_clock_frequency(struct Sis3316Module *);
void sis_3316_clear_timestamp(struct Sis3316Module *);
void sis_3316_disarm(struct Sis3316Module *);
void sis_3316_adjust_address_threshold(struct Sis3316Module *, double);
void sis_3316_configure_external_clock_input(struct Sis3316Module *);

#define CHECK_REG_SET_MASK(reg, val, mask) do { \
		uint32_t reg_; \
		SERIALIZE_IO; \
		reg_ = MAP_READ(m->sicy_map, reg) & (mask); \
		if ((val) != reg_) { \
			log_die(LOGL, NAME" register " #reg " value does "\
			    "not match %08x != %08x & %08x",\
			    (val), reg_, (mask));\
		} \
	} while (0)

#define CHECK_REG_SET(reg, val) CHECK_REG_SET_MASK(reg, val, 0xffffffff)

uint32_t
sis_3316_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
	/* log_die(LOGL, NAME" check_empty not implemented!"); */
}

uint32_t
sis_3316_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct Sis3316Module *m;

	(void)a_crate;
	(void)a_event_buffer;
	(void)a_do_pedestals;
	LOGF(debug)(LOGL, NAME" parse_data {");
	MODULE_CAST(KW_SIS_3316, m, a_module);
	if (m->config.run_mode == RM_ASYNC) {
		++a_module->event_counter.value;
	}
	LOGF(debug)(LOGL, NAME" parse_data }");
	return 0;
}

/**
 * This is called after init of all modules is done, but before the readout,
 * so we can hijack it to perform last minute initialisation.
 */
int
sis_3316_post_init(struct Crate *a_crate, struct Module *a_module)
{
	int i;
	struct Sis3316Module *m;
	(void) a_crate;

	LOGF(verbose)(LOGL, NAME" post_init {");

	MODULE_CAST(KW_SIS_3316, m, a_module);

	/*
	 * Clear timestamp only once in the beginning, and only
	 * if this module is fp bus master.
	 */
	if (m->config.is_fpbus_master) {
		LOGF(verbose)(LOGL, "clear timestamp");
		sis_3316_clear_timestamp(m);
	}

	/* read counters to initialise previous values */
	sis_3316_read_stat_counters(m);
	COPY(m->stat_prev_dump, m->stat_prev);

	/* TODO: Is this really right? BL: Works! */
	a_module->event_counter.value = 0;

	/* Disarm and arm the first bank. */
	MAP_WRITE(m->sicy_map, disarm_and_arm_bank2, 1);
	m->current_bank = 1;
	LOGF(verbose)(LOGL, "%d bank 2 armed.", m->module.id);
	LOGF(verbose)(LOGL, "%d configuration complete.", m->module.id);

	/* ADC status. */
	/* TODO: Occurrence 1/4 of this dumping in the file! */
	for (i = 0; i < N_ADCS; ++i) {
		LOGF(verbose)(LOGL, "ADC %d status: 0x%08x", i,
		    MAP_READ(m->sicy_map, fpga_adc_status(i)));
	}

	LOGF(verbose)(LOGL, NAME" post_init(ctr=0x%08x) }",
	    a_module->event_counter.value);

	return 1;
}

struct Module *
sis_3316_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct Sis3316Module *m;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(m);
	/* TODO: Good event-size? BL: The sky is the limit. */
	m->module.event_max = 512;

	ZERO(m->config);
	ZERO(m->stat);
	ZERO(m->stat_prev);
	ZERO(m->stat_prev_dump);

	m->config.address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address = %08x", m->config.address);

	sis_3316_get_config((struct Sis3316Module *)m, a_block);

	m->current_bank = 1;

	m->dma_map = NULL;

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)m;
}

void
sis_3316_deinit(struct Module *a_module)
{
	struct Sis3316Module* m;

	LOGF(verbose)(LOGL, NAME" deinit {");
	MODULE_CAST(KW_SIS_3316, m, a_module);
	map_unmap(&m->sicy_map);
	map_unmap(&m->dma_map);
	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
sis_3316_destroy(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" destroy.");
}

struct Map *
sis_3316_get_map(struct Module *a_module)
{
	struct Sis3316Module *m;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_SIS_3316, m, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return m->sicy_map;
}

void
sis_3316_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		/* This module uses the address, not module id! Hmm... */
		0,
		BITS_MASK(16, 31),
		0x3316 << 16)
	MODULE_SIGNATURE_END(a_array, a_num)
}

void
sis_3316_reset(struct Sis3316Module* m)
{
	int i = 0;

	/* Reset FPGA logic */
	MAP_WRITE(m->sicy_map, reset_adc_fpga, 1);

	/* Clear link error latch bits */
	for (i = 0; i < 4; ++i) {
		MAP_WRITE(m->sicy_map, fpga_adc_tap_delay(i), 0x400);
	}

	/* Clear error bits */
	MAP_WRITE(m->sicy_map, data_link_status, 0xE0E0E0E0);
	LOGF(verbose)(LOGL, "Data link status: 0x%08x",
	    MAP_READ(m->sicy_map, data_link_status));

	/* Wait a short while for things to settle */
	time_sleep(30e-3);

	/* ADC status */
	/* TODO: Occurrence 2/4 of this dumping in the file! */
	for (i = 0; i < 4; ++i) {
		LOGF(verbose)(LOGL, "ADC %d status: 0x%08x", i,
		    MAP_READ(m->sicy_map, fpga_adc_status(i)));
	}

	/* Reset module */
	MAP_WRITE(m->sicy_map, register_reset, 1);

	/* Disarm */
	MAP_WRITE(m->sicy_map, disarm_sampling_logic, 1);
	
	/* get current bank */
	m->current_bank = ((
	    MAP_READ(m->sicy_map, channel_actual_sample_address(0))
	    >> 24) & 0x1);

	LOGF(debug)(LOGL, "current_bank = %d", m->current_bank);

	sis_3316_swap_banks(m);

	time_sleep(1e-3);

	LOGF(debug)(LOGL, "current_bank after initial swap (1) = %d",
	    m->current_bank);

	m->current_bank = ((
	    MAP_READ(m->sicy_map, channel_actual_sample_address(0))
	    >> 24) & 0x1);

	LOGF(debug)(LOGL, "current_bank after initial swap (2) = %d",
	    m->current_bank);

	sis_3316_clear_timestamp(m);
}

/*
 * Setup IOB delay logic according to clock frequency
 */
void
sis_3316_set_iob_delay_logic(struct Sis3316Module *m, int i)
{
	uint32_t data;

	/* 0x300 = Select all channels */
	if (m->config.clk_freq == 25) {
		if (m->config.bit_depth == BD_14BIT) {
			data = 0x300 | V2004_14BIT_TAP_DELAY_25MHZ;
		} else {
			data = 0x300 | V2004_16BIT_TAP_DELAY_25MHZ;
		}
	} else if (m->config.clk_freq == 125) {
		if (m->config.bit_depth == BD_14BIT) {
			data = 0x300 | V2004_14BIT_TAP_DELAY_125MHZ;
		} else {
			data = ADD_HALF_PERIOD |
			       0x300 | V2004_16BIT_TAP_DELAY_125MHZ;
		}
	} else {
		/* Default */
		data = ADD_HALF_PERIOD | 0x300 | V2004_14BIT_TAP_DELAY_250MHZ;
	}

	/* Add fine tune value. */
	data = (data & 0xffffff00)
	    | ((data + m->config.tap_delay_fine_tune) & 0xff);

	MAP_WRITE(m->sicy_map, fpga_adc_tap_delay(i), data);
	CHECK_REG_SET(fpga_adc_tap_delay(i), data);
	LOGF(verbose)(LOGL, "Tap delay ADC %d: 0x%08x", i,
	    MAP_READ(m->sicy_map, fpga_adc_tap_delay(i)));
}

/*
 * Set sample_clock_distribution_control register.
 */
void
sis_3316_set_sample_clock_dist(struct Sis3316Module *m)
{
	uint32_t data;

	data = 0;
	if (m->config.is_fpbus_master) {
		if (m->config.ext_clk_freq == 0 &&
		    m->config.use_rataclock == 0) {
			data = CS_INTERNAL;
		} else {
			if (m->config.use_clock_from_vxs == 1) {
				data = CS_VXS;
			} else {
				data = CS_EXTERNAL;
			}
		}
	} else {
		/* TODO: what, when use_rataclock = 1 ? */
		data = CS_FP_BUS;
	}
	LOGF(verbose)(LOGL, "sis3316[%d]: Setting %s (%d) clock.",
	    m->module.id, g_clock_source[data], data);
	MAP_WRITE(m->sicy_map, sample_clock_distribution_control, data);
	CHECK_REG_SET(sample_clock_distribution_control, data);
}

/*
 * Setup external clock multiplier
 */

void
sis_3316_configure_external_clock_input(struct Sis3316Module *m)
{
	uint32_t clk_from;
	uint32_t clk_to;
	struct Sis3316ExternalClockMultiplierConfig c;

	clk_from = m->config.ext_clk_freq;
	clk_to = m->config.clk_freq;

	/*
	 * Bypass, if the external clock is unconfigured, or if
	 * rataclock is used.
	 */
	if (clk_from == 0 || m->config.use_rataclock == 1) {
		LOGF(verbose)(LOGL,
		    "Leaving clock input multiplier unconfigured");
		sis_3316_si5325_clk_multiplier_bypass(m);
	} else {
		if (clk_to == 25) {
			log_die(LOGL,
			    "Cannot generate 25 MHz from external clock.");
		}
		c = sis_3316_make_ext_clk_mul_config(clk_from, clk_to);
		sis_3316_si5325_clk_multiplier_set(m, c);
	}
}

/*
 * Setup frequency of internal oscillator according to config
 */
void
sis_3316_set_clock_frequency(struct Sis3316Module *m)
{
	unsigned int clk_hsdiv;
	unsigned int clk_n1div;
#define CLK_N1DIV_250MHZ	4
#define CLK_N1DIV_125MHZ	8
#define CLK_N1DIV_25MHZ		40
#define CLK_HSDIV_250MHZ	5
#define CLK_HSDIV_125MHZ	5
#define CLK_HSDIV_25MHZ		5
	switch (m->config.clk_freq) {
	case 250:
		clk_n1div = CLK_N1DIV_250MHZ;
		clk_hsdiv = CLK_HSDIV_250MHZ;
		break;
	case 125:
		clk_n1div = CLK_N1DIV_125MHZ;
		clk_hsdiv = CLK_HSDIV_125MHZ;
		break;
	case 25:
		clk_n1div = CLK_N1DIV_25MHZ;
		clk_hsdiv = CLK_HSDIV_25MHZ;
		break;
	default:
		log_die(LOGL, "Frequency %d MHz not supported.",
		    m->config.clk_freq);
	}
	LOGF(verbose)(LOGL, "Setting frequency to %d MHz",
	    m->config.clk_freq);
	sis_3316_change_frequency(m, 0, clk_hsdiv, clk_n1div);

	/* Wait until clock is stable */
	time_sleep(30e-3);

	/* PLL Lock */
	MAP_WRITE(m->sicy_map, reset_adc_clock, 0x0);

	/* Wait until ADC clock / PLL is reset */
	time_sleep(30e-3);
}

/*
 * Configure offset DAC for ADC i
 */
void
sis_3316_set_offset(struct Sis3316Module *m, int i)
{
	int j;

	MAP_WRITE(m->sicy_map, fpga_adc_offset_dac_control(i), 
	    0x88f00000 + 0x1); /* internal reference */
	time_sleep(1e-3);

        for (j = 0; j < 4; ++j) {
                uint32_t data;
                data = 0x82000000 + (j << 20) +
                    ((0x8000 + m->config.dac_offset[i*4 + j]) << 4);
		MAP_WRITE(m->sicy_map, fpga_adc_offset_dac_control(i), data);
                LOGF(verbose)(LOGL, "DAC offset %d = %08x", (i*4+j), data);
                time_sleep(1e-3);
        }
        time_sleep(1e-3);
}

/*
 * Configure ADCs via SPI bus
 */
void
sis_3316_setup_adcs(struct Sis3316Module *m)
{
	int i;
	char adc_output_mode;
	char adc_input_span;

	/* Set ADC chips via SPI */
	if (m->config.bit_depth == BD_14BIT) {
		adc_input_span = SPI_AD9634_INPUT_SPAN_1V75;
		adc_output_mode = SPI_AD9634_OUTPUT_INVERT;

	} else {
		adc_input_span = SPI_AD9268_INPUT_SPAN_1V5;
		adc_output_mode = SPI_AD9268_OUTPUT_LVDS;
	}
	if (m->config.bit_depth != BD_14BIT
	    && m->config.use_dithering == 1) {
		log_die(LOGL,
		    "Dithering only supported on 14 bit ADC.");
	}
	for (i = 0; i < N_ADCS; ++i) {
		int adc;
		for (adc = SPI_CH12; adc <= SPI_CH34; adc += SPI_CH34) {
			/* Reset ADC chip */
			MAP_WRITE(m->sicy_map, fpga_adc_spi_control(i),
			    SPI_WRITE | SPI_ENABLE | adc |
			    SPI_OP(SPI_OP_RESET) | 0x24);
			time_sleep(20e-3);
			/* Input span */
			MAP_WRITE(m->sicy_map, fpga_adc_spi_control(i),
			    SPI_WRITE | SPI_ENABLE | adc |
			    SPI_OP(SPI_OP_INPUT_SPAN) | adc_input_span);
			time_sleep(20e-3);
			/* Output mode */
			MAP_WRITE(m->sicy_map, fpga_adc_spi_control(i),
			    SPI_WRITE | SPI_ENABLE | adc |
			    SPI_OP(SPI_OP_OUTPUT_MODE) | adc_output_mode);
			time_sleep(20e-3);

			/* Dithering (only 125 MHz ADC) */
			if (m->config.bit_depth == BD_14BIT &&
			    m->config.use_dithering == 1) {
				MAP_WRITE(m->sicy_map,
				    fpga_adc_spi_control(i),
				    SPI_WRITE | SPI_ENABLE | adc
				    | SPI_OP(SPI_OP_DITHER_ENABLE)
				    | SPI_AD9268_DITHER_ENABLE);
				time_sleep(20e-3);
			}

			/* Update values atomically */
			MAP_WRITE(m->sicy_map, fpga_adc_spi_control(i),
			    SPI_WRITE | SPI_ENABLE | adc |
			    SPI_OP(SPI_OP_TRANSFER) | 0x1);
			time_sleep(20e-3);
		}
	}

	for (i = 0; i < N_ADCS; ++i) {
		/* enable ADC */
		MAP_WRITE(m->sicy_map, fpga_adc_spi_control(i), (1 << 24));
		time_sleep(1e-3);
	}
}

void
sis_3316_setup_event_config(struct Sis3316Module *m)
{
	int i;
	uint32_t event_config_data[N_ADCS] = {0,0,0,0};

	for (i = 0; i < N_CHANNELS; ++i) {
		int adc = i / N_ADCS;
		int ch = i % N_ADCS;
		int offset = ch * 8;
		if (m->config.invert_signal[i] == 1) {
			event_config_data[adc] |=
			    (1 << offset);
		}
		if (((m->config.use_external_gate >> i) & 1) == 1) {
			event_config_data[adc] |=
			    (1 << (offset + 6 /* external gate */));
		}
		if (((m->config.use_external_veto >> i) & 1) == 1) {
			event_config_data[adc] |=
			    (1 << (offset + 7 /* external veto */));
		}
		if (((m->config.use_external_trigger >> i) & 1) == 1) {
			event_config_data[adc] |=
			    (1 << (offset + 3 /* external */));
		}
		if (((m->config.use_internal_trigger >> i) & 1) == 1) {
			event_config_data[adc] |=
			    (1 << (offset + 2 /* internal */));
		}
	}
	for (i = 0; i < N_ADCS; ++i) {
		MAP_WRITE(m->sicy_map, fpga_adc_event_config(i),
		    event_config_data[i]);
		CHECK_REG_SET(fpga_adc_event_config(i), event_config_data[i]);
	}
	LOGF(verbose)(LOGL,
	    "Event = 0x%08x 0x%08x 0x%08x 0x%08x",
	    MAP_READ(m->sicy_map, fpga_adc_event_config(0)),
	    MAP_READ(m->sicy_map, fpga_adc_event_config(1)),
	    MAP_READ(m->sicy_map, fpga_adc_event_config(2)),
	    MAP_READ(m->sicy_map, fpga_adc_event_config(3)));
}

void
sis_3316_setup_averaging_mode(struct Sis3316Module *m)
{
	int i;

	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;

		data = (m->config.average_mode[i] << 28)
		     | (m->config.average_pretrigger[i] << 16)
		     | (m->config.average_length[i]);

		LOGF(verbose)(LOGL, "Average mode setup[%d] = 0x%08x", i, data);
		MAP_WRITE(m->sicy_map, fpga_adc_average_config(i), data);
		CHECK_REG_SET(fpga_adc_average_config(i), data);
	}
}


void
sis_3316_setup_data_format(struct Sis3316Module *m)
{
	int i;
	uint32_t data_format = 0;
	uint32_t header_length = 3;
	/* uint32_t header_acc2_offset = 2; */
	/* uint32_t header_maw3_offset = 2; */

	/* Data format setup */
	if (m->config.use_user_counter) {
		header_length += 1;
		/* header_acc2_offset += 1; */
		/* header_maw3_offset += 1; */
		data_format += 0x40;
	}
	if (m->config.use_accumulator6 == 1) {
		header_length += 7;
		/* header_acc2_offset += 7; */
		/* header_maw3_offset += 7; */
		data_format += 0x1;
	}
	if (m->config.use_accumulator2 == 1) {
		header_length += 2;
		/* header_maw3_offset += 2; */
		data_format += 0x2;
	}
	if (m->config.use_maw3 == 1) {
		header_length += 3;
		data_format += 0x4;
	}
	if (m->config.use_maxe == 1) {
		header_length += 2;
		data_format += 0x8;
	}
	if (m->config.write_traces_maw == 1) {
		data_format += 0x10;
	}
	if (m->config.write_traces_maw_energy == 1) {
		data_format += 0x30;
	}

	/* Apply to all channels */
	data_format += data_format << 8;
	data_format += data_format << 16;
	LOGF(verbose)(LOGL, "Data format = 0x%08x", data_format);

	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;
		/* Length of a single event */
		m->config.event_length[i] = header_length
		    + m->config.sample_length[i] / 2
		    + m->config.average_length[i] / 2
		    + m->config.sample_length_maw[i];
		LOGF(verbose)(LOGL, "Event length %d = %d (raw=%d,avg=%d,maw=%d)",
		    i, m->config.event_length[i],
		    m->config.sample_length[i] / 2,
		    m->config.average_length[i] / 2,
		    m->config.sample_length_maw[i]);

		/* additional header for averaging mode */
		if (m->config.average_mode[i] != 0) {
			m->config.event_length[i] += 1;
		}

		MAP_WRITE(m->sicy_map, fpga_adc_data_format_config(i),
		    data_format);
		CHECK_REG_SET(fpga_adc_data_format_config(i), data_format);
		data = m->config.sample_length_maw[i]
		    + (m->config.pretrigger_delay_maw[i] << 16);
		LOGF(verbose)(LOGL, "Maw buffer setup[%d] = 0x%08x", i, data);
		MAP_WRITE(m->sicy_map, fpga_adc_maw_test_config(i), data);
		CHECK_REG_SET(fpga_adc_maw_test_config(i), data);
	}
}

int
sis_3316_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3316Module *m;
	uint32_t max_bytes;
	uint32_t min_hits;
	uint32_t max_bytes_per_channel;
	uint32_t num_hits;
	unsigned multi_event_max;
	const size_t max_allowed_subevent_bytes = 0x100000;
	uint8_t clocks_per_ns = 0;
	int i;

	(void)a_crate;
	
	LOGF(verbose)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_SIS_3316, m, a_module);

	/* kill request for others and set our own request */
	MAP_WRITE(m->sicy_map, access_arbitration_control, 0x80000001);

	sis_3316_get_config(m, a_module->config);

	clocks_per_ns = 1000 / m->config.clk_freq;

	/* This can only be done after knowing the bit depth. */
        /* 
         * This calculates: 
         * config.gate parameters (width, delay) 
         * config.pileup / repileup 
         * config.sample_length (raw, maw) 
         * config.pretrigger_delay 
         * (disabled) dac offsets 
         * (disabled) peak time / gap time 
         * config.trigger_gate_window_length 
         * tau table / tau factor 
         * config.extra_filter 
         * config.histogram_divider 
         * config.threshold 
         */
	sis_3316_calculate_settings((struct Sis3316Module *)m);

	sis_3316_set_sample_clock_dist(m);

	/*
	 * FB Bus Enable
	 * 0 - Control out,
	 * 1 - Status out,
	 * 4 - Clock out,
	 * 5 - Clock MUX out (0 internal source, 1 from external)
	 */
	if (m->config.is_fpbus_master) {
		uint32_t data = 0;
		LOGF(verbose)(LOGL, "sis3316[%d]: is FPbus master (default).",
		    m->module.id);
		data |= (1 << 4); /* Enable sample clock output on fp-bus */
		if (m->config.ext_clk_freq != 0) {
			/* Enable external clock output on fp-bus */
			data |= (1 << 5);
		}
		data |= (1 << 0); /* Enable all control lines on fp-bus
				     (trigger/veto, ts clear) */
		MAP_WRITE(m->sicy_map, fpbus_control, data); /* Master only */
		CHECK_REG_SET(fpbus_control, data);
	}

	if (m->config.ext_clk_freq != 0) {
		/* Configure external clock multiplier */
		sis_3316_configure_external_clock_input(m);

		LOGF(verbose)(LOGL, "sis3316: wait for clock to stabilise");
		time_sleep(1.2);

		/* PLL Lock */
		MAP_WRITE(m->sicy_map, reset_adc_clock, 0x0);

		/* Wait until ADC clock / PLL is reset */
		time_sleep(30e-3);
	} else {
		/* Set clock frequency */
		sis_3316_set_clock_frequency(m);
	}

	/* ADC status. */
	/* TODO: Occurrence 3/4 of this dumping in the file! */
	for (i = 0; i < N_ADCS; ++i) {
		LOGF(verbose)(LOGL, "ADC %d status: 0x%08x", i,
		    MAP_READ(m->sicy_map, fpga_adc_status(i)));
	}

	/* Calibrate IOB _delay logic. */
	LOGF(verbose)(LOGL, "Calibrating IOB logic.");
	for (i = 0; i < N_ADCS; ++i) {
		/*
		 * 0xf00 = Calibration + Clear errors + Select all channels.
		 */
		MAP_WRITE(m->sicy_map, fpga_adc_tap_delay(i), 0xf00);
		CHECK_REG_SET(fpga_adc_tap_delay(i), 0xf00);
	}
	time_sleep(30e-3);

	sis_3316_setup_adcs(m);

	for (i = 0; i < N_ADCS; ++i) {
		sis_3316_set_offset(m, i);
	}

	for (i = 0; i < N_ADCS; ++i) {
		sis_3316_set_iob_delay_logic(m, i);
	}
	time_sleep(30e-3);

	/* TODO: Gain / termination. */
	LOGF(verbose)(LOGL, "Setting gain/termination.");
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;

		data = 0;
		switch (m->config.range[i]) {
		case 2:
			data |= 1;
			break;
		case 5:
			break;
		default:
			log_error(LOGL, "Unsupported V range: %d V",
			    m->config.range[i]);
			abort();
		}
		if (m->config.use_termination == 0) {
			data |= 4;
		}
		data |= (data << 8);
		data |= (data << 16);

		LOGF(verbose)(LOGL, "Gain & termination = %08x", data);
		MAP_WRITE(m->sicy_map, fpga_adc_gain_termination(i), data);
		CHECK_REG_SET(fpga_adc_gain_termination(i), data);
	}

	/* Disable FIR triggers. */
	for (i = 0; i < N_CHANNELS; ++i) {
		if (i < N_ADCS) {
			MAP_WRITE(m->sicy_map,
			    fpga_adc_trigger_threshold_sum(i), 0);
			CHECK_REG_SET(fpga_adc_trigger_threshold_sum(i), 0);
		}
		MAP_WRITE(m->sicy_map, channel_trigger_threshold(i), 0);
		CHECK_REG_SET(channel_trigger_threshold(i), 0);
	}

	/* Set high energy threshold (and both edges bit). */
	for (i = 0; i < N_CHANNELS; ++i) {
		uint32_t thr;

		if (i < N_ADCS) {
			thr = m->config.threshold_high_e[i];
			MAP_WRITE(m->sicy_map,
			    fpga_adc_trigger_thr_high_e_sum(i), thr);
			CHECK_REG_SET(
			    fpga_adc_trigger_thr_high_e_sum(i), thr);
		}
		thr = (((m->config.use_dual_threshold >> i) & 1) << 31) |
		    m->config.threshold_high_e[i];
		MAP_WRITE(m->sicy_map, channel_trigger_thr_high_e(i), thr);
		CHECK_REG_SET(channel_trigger_thr_high_e(i), thr);
	}

	/* FIR trigger setup. */
	for (i = 0; i < N_CHANNELS; ++i) {
		uint32_t data;
		if (i < N_ADCS) {
			data = m->config.peak[i]
			    + (m->config.gap[i] << 12);
			MAP_WRITE(m->sicy_map, fpga_adc_trigger_setup_sum(i),
			    data);
			LOGF(verbose)(LOGL, "T gap  %d= 0x%02x "
			    "%d= 0x%02x %d= 0x%02x %d= 0x%02x",
			    i, m->config.gap[i],
			    i+4, m->config.gap[i+4],
			    i+8, m->config.gap[i+8],
			    i+12, m->config.gap[i+12]);
			LOGF(verbose)(LOGL, "T peak %d= 0x%02x "
			    "%d= 0x%02x %d= 0x%02x %d= 0x%02x",
			    i, m->config.peak[i],
			    i+4, m->config.peak[i+4],
			    i+8, m->config.peak[i+8],
			    i+12, m->config.peak[i+12]);
			CHECK_REG_SET(fpga_adc_trigger_setup_sum(i), data);
		}
		data = ((10 << 24) + m->config.peak[i] +
		     (m->config.gap[i] << 12));
		MAP_WRITE(m->sicy_map, channel_trigger_setup(i), data);
		CHECK_REG_SET(channel_trigger_setup(i), data);
	}

	/* Trigger threshold setup. */
	for (i = 0; i < N_CHANNELS; ++i) {
		uint32_t cfd_bits = 0;
		if (m->config.use_cfd_trigger) {
			cfd_bits = 3; /* 50% mode */
		}
		MAP_WRITE(m->sicy_map, channel_trigger_threshold(i),
		      (1 << 31) /* Trigger enable */
		    | (cfd_bits << 28)
		    | m->config.threshold[i]);
		SERIALIZE_IO;
		LOGF(verbose)(LOGL, "Threshold %d = 0x%08x", i,
		    MAP_READ(m->sicy_map, channel_trigger_threshold(i)));
	}

	/* FIR energy pickup index */
	for (i = 0; i < N_CHANNELS; ++i) {
		uint32_t data;
		data = (m->config.energy_pickup[i] & 0x7ff) << 16;
		MAP_WRITE(m->sicy_map, channel_energy_pickup_config(i), data);
		CHECK_REG_SET(channel_energy_pickup_config(i), data);
		LOGF(verbose)(LOGL, "energy_pickup[%02d] = %08x", i, data);
	}

	/* FIR energy setup. */
	for (i = 0; i < N_CHANNELS; ++i) {
		uint32_t data;
		data = ((m->config.peak_e[i] & 0xfff)
		     | ((m->config.gap_e[i] & 0x3ff) << 12)
		     | ((m->config.extra_filter[i] & 0x3) << 22)
		     | ((m->config.tau_factor[i] & 0x3f) << 24)
		     | ((m->config.tau_table[i] & 0x3) << 30));
		MAP_WRITE(m->sicy_map, channel_fir_energy_setup(i), data);
		CHECK_REG_SET(channel_fir_energy_setup(i), data);
		LOGF(verbose)(LOGL, "fir_energy_setup[%02d] = %08x",i,data);
		if (i%4 == 3) {
			LOGF(verbose)(LOGL,
			    "Gap time E  %d= 0x%03x %d= 0x%03x "
			    "%d= 0x%03x %d= 0x%03x",
			    i-3, m->config.gap_e[i-3],
			    i-2, m->config.gap_e[i-2],
			    i-1, m->config.gap_e[i-1],
			    i, m->config.gap_e[i]);
			LOGF(verbose)(LOGL,
			    "Peak time E %d= 0x%03x %d= 0x%03x "
			    "%d= 0x%03x %d= 0x%03x",
			    i-3, m->config.peak_e[i-3],
			    i-2, m->config.peak_e[i-2],
			    i-1, m->config.peak_e[i-1],
			    i, m->config.peak_e[i]);
			LOGF(verbose)(LOGL,
			    "Tau config  %d= %d,%d %d= %d,%d %d= "
			    "%d,%d %d= %d,%d",
			    i-3, m->config.tau_table[i-3],
			    m->config.tau_factor[i-3],
			    i-2, m->config.tau_table[i-2],
			    m->config.tau_factor[i-2],
			    i-1, m->config.tau_table[i-1],
			    m->config.tau_factor[i-1],
			    i, m->config.tau_table[i],
			    m->config.tau_factor[i]);
		}
	}

	/* Clear error bits. */
	MAP_WRITE(m->sicy_map, data_link_status, 0xE0E0E0E0);

	/* Setup MAW test buffer delay. */
	for (i = 0; i < N_ADCS; ++i) {
		if (m->config.pretrigger_delay_maw[i] > 1022) {
			log_die(LOGL, "pretrigger_delay_maw[%d] must be <= "
			    "1022, is %d", i,
			    m->config.pretrigger_delay_maw[i]);
		}
		if (m->config.pretrigger_delay_maw_e[i] > 1022) {
			log_die(LOGL, "pretrigger_delay_maw[%d] must be <= "
			    "1022, is %d", i,
			    m->config.pretrigger_delay_maw_e[i]);
		}
		/* BL: Stupid to do this here */
		/* if (m->config.write_traces_maw == 0) {
			m->config.sample_length_maw[i] = 0;
		}
		if (m->config.write_traces_maw_energy == 0) {
			m->config.sample_length_maw_e[i] = 0;
		}
		*/
	}

	/* Set up trigger gate lengths. */
	{
		int sample_start_index = 0;

		for (i = 0; i < N_ADCS; ++i) {
			uint32_t data;
			int trigger_gate_window_length = 0;

			trigger_gate_window_length =
			    m->config.trigger_gate_window_length;

			data = trigger_gate_window_length - 2;
			MAP_WRITE(m->sicy_map,
			    fpga_adc_trigger_gate_length(i), data);
			CHECK_REG_SET(fpga_adc_trigger_gate_length(i), data);

			data = (m->config.sample_length[i] << 16) +
			    sample_start_index;
			MAP_WRITE(m->sicy_map,
			    fpga_adc_raw_data_buffer_config(i), data);
			CHECK_REG_SET(fpga_adc_raw_data_buffer_config(i),
			    data);

                        if (m->config.sample_length[i] > 65534) {
				MAP_WRITE(m->sicy_map,
				    fpga_adc_ext_raw_data_buffer(i),
                                    m->config.sample_length[i]);
				CHECK_REG_SET(fpga_adc_ext_raw_data_buffer(i),
				    m->config.sample_length[i]);
                        }

			LOGF(verbose)(LOGL, "Sample Length [%d] = %d samples",
			    i, m->config.sample_length[i]);
			LOGF(verbose)(LOGL,
			    "Trigger Gate Window Length [%d] = %d", i,
			    trigger_gate_window_length);
		}
	}

	/* Set up accumulator gates. */
	if (m->config.use_accumulator6) {
		for (i = 0; i < N_ADCS; ++i) {
			int j;
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_1(i),
			      m->config.gate[0].width +
			      m->config.gate[0].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_2(i),
			      m->config.gate[1].width +
			      m->config.gate[1].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_3(i),
			      m->config.gate[2].width +
			      m->config.gate[2].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_4(i),
			      m->config.gate[3].width +
			      m->config.gate[3].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_5(i),
			      m->config.gate[4].width +
			      m->config.gate[4].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_6(i),
			      m->config.gate[5].width +
			      m->config.gate[5].delay);
			for (j = 0; j < 6; ++j) {
				LOGF(verbose)(LOGL, "ADC[%d] Gate%d = "
				    "{width = %d, delay = %d}",
				    i, j, m->config.gate[j].width,
				    m->config.gate[j].delay);
			}
		}
	}

	if (m->config.use_accumulator2) {
		for (i = 0; i < N_ADCS; ++i) {
			int j;
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_7(i),
			      m->config.gate[6].width
			    + m->config.gate[6].delay);
			MAP_WRITE(m->sicy_map, fpga_adc_acc_gate_8(i),
			      m->config.gate[7].width
			    + m->config.gate[7].delay);
			for (j = 7; j < N_GATES; ++j) {
				LOGF(verbose)(LOGL, "ADC[%d] Gate%d = "
				    "{width = %d, delay = %d}",
				    i, j, m->config.gate[j].width,
				    m->config.gate[j].delay);
			}
		}
	}

	/* Set up internal trigger delays. */
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data = 0;
		int ch;
		for (ch = 0; ch < 4; ++ch) {
			uint8_t val;
			val = m->config.internal_trigger_delay[i * 4 + ch];
			/* value stored is in unit of 2 clock cycles */
			val /= clocks_per_ns * 2;
			data |= val;
		}
		MAP_WRITE(m->sicy_map, fpga_adc_int_trigger_delay(i), data);
		LOGF(verbose)(LOGL, "ADC[%d] internal_delay = 0x%08x", i,
		    data);
	}

	/* Set up pretrigger and pileup lengths. */
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;
		/*
		 * According to Struck, pretrigger delay should be augmented
		 * by the internal_trigger_delay length (in samples) * 2.
		 */
		data = m->config.pretrigger_delay[i]
		    + m->config.internal_trigger_delay[i] / 2 / clocks_per_ns;
		MAP_WRITE(m->sicy_map, fpga_adc_pretrigger_delay(i), data);
		CHECK_REG_SET(fpga_adc_pretrigger_delay(i), data);

		data = (m->config.re_pileup_length[i] << 16)
		    + (m->config.pileup_length[i]);
		MAP_WRITE(m->sicy_map, fpga_adc_pileup_config(i), data);
		CHECK_REG_SET(fpga_adc_pileup_config(i), data);
	}
	LOGF(verbose)(LOGL, "Pretrigger delay 0= %d 1= %d 2= %d 3= %d",
	    m->config.pretrigger_delay[0],
	    m->config.pretrigger_delay[1],
	    m->config.pretrigger_delay[2],
	    m->config.pretrigger_delay[3]);

	/* Set trigger statistic counter mode to latching. */
	for (i = 0; i < N_ADCS; ++i) {
		MAP_WRITE(m->sicy_map, fpga_adc_trigger_counter_mode(i), 1);
	}


	/*
	 * Setup of LEMO inputs and outputs.
	 */

	/*
	 * TI:
	 * TI is used as rataclock input, when rataclock is used.
	 */
	if (m->config.use_rataclock == 0) {
		uint32_t nim_in_control = 0;
		
		if (m->config.use_user_counter) {
			/*
			 * Enable TI and UI for user counter
			 */
			nim_in_control |= (1 << 15);
			LOGF(verbose)(LOGL, "Enable TI/UI as user counter.");
		}

		if (m->config.use_external_trigger != 0 ||
		    m->config.use_external_gate != 0) {

			/*
			 * Enable TI as external trigger input.
			 */
			nim_in_control |= (1 << 4);
			LOGF(verbose)(LOGL,
			    "Setting TI as trigger/gate input.");
		}

		if (m->config.ext_clk_freq != 0) {
			/*
			 * Enable CI as external clock input
			 */
			nim_in_control |= 1;
		}

		/*
		 * When using external gate input, enable TI as
		 * level sensitive.
		 */
		if (m->config.use_external_gate != 0) {
			nim_in_control |= (1 << 6);
			LOGF(verbose)(LOGL, "Setting TI as level sensitive.");
		}

		/*
		 * When using external veto input, enable UI as
		 * level sensitive.
		 */
		if (m->config.use_external_veto == 1) {
			nim_in_control |= (1 << 10);
			LOGF(verbose)(LOGL, "Setting UI as level sensitive.");
		}

		MAP_WRITE(m->sicy_map, nim_in_control, nim_in_control);
		CHECK_REG_SET_MASK(nim_in_control, nim_in_control, 0xffff);
	} else {
		LOGF(verbose)(LOGL, "Using rataclock as external clock.");
	}

	/*
	 * Setup rataclock receiver(s)
	 */
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;

		/* clock mul select (read receiver status & timestamp */
		MAP_WRITE(m->sicy_map, fpga_adc_rataser_mux_sel(i), 0x3);

		/* control register 1 */
		data = (m->config.rataclock_receive_delay << 16)
		    | (m->config.rataclock_clk_period << 6)
		    | m->config.rataclock_pulse_period_clk;
		MAP_WRITE(m->sicy_map, fpga_adc_rataser_control1(i), data);

		/* control register 2 */
		data = (m->config.rataclock_use_auto_edge << 2)
		    | m->config.rataclock_expect_edge;
		MAP_WRITE(m->sicy_map, fpga_adc_rataser_control2(i), data);
	}
	LOGF(verbose)(LOGL, "rataser control1 = 0x%08x, control2 = %08x",
		MAP_READ(m->sicy_map, fpga_adc_rataser_control1(0)),
		MAP_READ(m->sicy_map, fpga_adc_rataser_control1(1)));

	/*
	 * Enable CO as sample clock output.
	 */
	MAP_WRITE(m->sicy_map, lemo_out_co_select, 0x1);
	CHECK_REG_SET(lemo_out_co_select, 0x1);

	/*
	 * UO:
	 * In synochronous mode enable UO as sample event active.
	 * This is needed for multi-event operation (busy).
	 *
	 * For asynchronous readout, UO sends the addr threshold
	 * flag indicating a full bank. This can be used for
	 * automatic bank switching.
	 *
	 * For asynchronous readout with external gate, send address
	 * threshold.
	 */
	switch (m->config.run_mode)
	{
		case RM_SYNC:
		case RM_ASYNC:
			MAP_WRITE(m->sicy_map, lemo_out_uo_select, 0x10);
			CHECK_REG_SET(lemo_out_uo_select, 0x10);
			LOGF(verbose)(LOGL, "UO as sampling active flag.");
			break;
		case RM_ASYNC_AUTO_BANK_SWITCH:
		case RM_ASYNC_EXTERNAL_GATE:
			MAP_WRITE(m->sicy_map, lemo_out_uo_select, 0x8);
			CHECK_REG_SET(lemo_out_uo_select, 0x8);
			LOGF(verbose)(LOGL, "UO as address_threshold flag.");
			break;
		default:
			/* Nothing */
			log_die(LOGL, "m->config.run_mode illegal.");
	}

	/*
	 * Enable TO as trigger OR on selected channels.
	 */
	MAP_WRITE(m->sicy_map, lemo_out_to_select, m->config.trigger_output);
	CHECK_REG_SET(lemo_out_to_select, m->config.trigger_output);
	/* BL: why is this here?
         * LOGF(info)(LOGL, "Using rataclock! TO used for trigger output");
         */
	sis_3316_setup_event_config(m);
	sis_3316_setup_data_format(m);
	sis_3316_setup_averaging_mode(m);

	/*
	 * Set threshold according to crate multi-event limit to allow
	 * multi-event operation. In synchronous mode, we ask the crate.
	 * In asynchronous mode, it can be set to an arbitrary configured
	 * value (async_max_events).
	 */
	/*
	 * TODO: MUST fix this. Otherwise no multi-event is possible.
	 */
	multi_event_max = 1/*crate_get_event_max(a_crate)*/;
	if (m->config.run_mode == RM_SYNC) {
		num_hits = multi_event_max;
	} else {
		num_hits = m->config.async_max_events;
	}
	LOGF(verbose)(LOGL, "Configured num_hits per channel %d", num_hits);

	/*
	 * Maximum number of hits is limited by the maximum
	 * event length. Here we assume an even event length distribution
	 * over all enabled channels.
	 */
	max_bytes_per_channel = max_allowed_subevent_bytes / g_n_channels;

	/*
	 * Figure out the smallest number of hits to fit in each ADC group
	 * and also the maximum amount of bytes to be read per readout.
	 */
	min_hits = (uint32_t) -1;
	max_bytes = 0;
	for (i = 0; i < N_ADCS; ++i) {
		size_t hits;
		size_t bytes_per_hit = m->config.event_length[i] *
			sizeof(uint32_t);
		hits = max_bytes_per_channel / bytes_per_hit;
		if (bytes_per_hit > max_bytes) {
			max_bytes = bytes_per_hit;
		}
		if (hits < min_hits) {
			min_hits = hits;
		}
	}

	if (min_hits < num_hits) {
		num_hits = min_hits;
		LOGF(verbose)(LOGL, "Restricting number of hits to %d",
		    num_hits);
	} else {
	}
	m->max_num_hits = num_hits;
	if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		/* Start with a factor of 4 less than maximum */
		m->num_hits = num_hits / 4;
	} else {
		m->num_hits = num_hits;
	}
	max_bytes *= num_hits * g_n_channels;

	LOGF(verbose)(LOGL, "Maximum possible number of hits %d",
	    m->max_num_hits);
	LOGF(verbose)(LOGL, "Using at most %d bytes per readout", max_bytes);
	LOGF(verbose)(LOGL, "Channels enabled: %d", g_n_channels);

	sis_3316_adjust_address_threshold(m, 1.0);

	/* Enable external trigger & external timestamp clear. */
	{
		uint32_t data;
		if (m->config.is_fpbus_master) {
			data = (1 << 8)   /* external trigger enable. */
			    |  (1 << 10)  /* external ts clear enable. */
			;
		} else {
			/* FPbus slaves should listen to the FPbus master. */
			data = (1 << 4)   /* fp control1 as trigger */
			    |  (1 << 6)   /* fp control2 enable (ts clear) */
			    |  (1 << 10)  /* external ts clear enable */
			;
		}

		/*
		 * If both external gate and internal trigger are used,
		 * then the local veto is used as a veto. TODO ok?
		 */
		if (m->config.use_external_gate != 0 &&
		    m->config.use_internal_trigger != 0) {
			data |= (1 << 11);
		}

		/*
		 * In async mode, one may also swap banks automatically.
		 * For this to work, a cable must be connected between
		 * UO and UI/TI, and the corresponding bits have to be set.
		 *
		 * To start acquisition, the KA: 0x428 must be issued.
		 */
		if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
			LOGF(verbose)(LOGL, "%d enable UI as bank switch "
			    "input.", m->module.id);
			data |= (1 << 13); /* UI as bank swap. */
			/* data |= (1 << 12); */ /* TI as bank swap. */
		}
		MAP_WRITE(m->sicy_map, acquisition_control, data);
		CHECK_REG_SET_MASK(acquisition_control, data, 0xffff);
	}

	/*
	 * Test if rataser clock is synchronised
	 */
	sis_3316_test_clock_sync(m);

	/*
	 * In automatic switching mode, the additional swap control logic
	 * needs to be enabled.
	 */
	if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		LOGF(verbose)(LOGL, "%d enable bank switch with NIM input.",
		    m->module.id);
		MAP_WRITE(m->sicy_map,
		    enable_sample_bank_swap_control_with_nim_input, 1);
	}

	LOGF(verbose)(LOGL, "May discard data for channels: %08x",
	    m->config.discard_data);
	
	/* Note: Timestamp clear is now done in post_init function. */

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
sis_3316_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct Sis3316Module *m;
	uint32_t firmware;
	uint32_t firmware_adc[N_ADCS];
	uint32_t serial_number;
	uint32_t data;
	int i;

	(void)a_crate;

	LOGF(verbose)(LOGL, NAME" init_slow {");

	MODULE_CAST(KW_SIS_3316, m, a_module);

	m->last_dumped = 0;
	m->last_read = 0;

	m->sicy_map = map_map(m->config.address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(trigger_coinc_lut_control),
	    MAP_POKE_REG(trigger_coinc_lut_control), 0);

	if (KW_NOBLT != m->config.blt_mode) {
		m->dma_map = map_map(m->config.address,
		    ADC_MEM_OFFSET * 5, m->config.blt_mode, 0, 0,
		    MAP_POKE_REG(nim_in_control),
		    MAP_POKE_REG(nim_in_control), 0);
	}

	sis_3316_reset(m);

	serial_number = MAP_READ(m->sicy_map, serial_number);
	LOGF(info)(LOGL, "Serial number=0x%08x.", serial_number);

	firmware = MAP_READ(m->sicy_map, module_id_firmware);
	if (firmware < REQUIRED_FIRMWARE) {
		log_die(LOGL, NAME"Actual firmware=%08x < required firmware="
		    "%08x.", firmware, REQUIRED_FIRMWARE);
	}
	LOGF(info)(LOGL, "id/firmware=0x%08x.", firmware);
	for (i = 0; i < N_ADCS; ++i) {
		firmware_adc[i] = MAP_READ(m->sicy_map, fpga_adc_version(i));
		LOGF(info)(LOGL, "adc[%u] firmware=0x%08x.", i,
		    firmware_adc[i]);
		if ((firmware_adc[i] >> 16) == 0x0250) {
			m->config.bit_depth = BD_14BIT;
			LOGF(verbose)(LOGL, "adc[%u] has 14 bits.", i);
		} else {
			m->config.bit_depth = BD_16BIT;
			LOGF(verbose)(LOGL, "adc[%u] has 16 bits.", i);
		}
	}
	if (firmware == 0x33165010 || firmware == 0x3316b011
	    || firmware == 0x3316a012 || firmware == 0x3316b012) {
		m->config.has_rataclock_receiver = 1;
	}
	if (m->config.has_rataclock_receiver == 0
	    && m->config.use_rataclock == 1) {
		log_error(LOGL, NAME"Cannot use rataclock! No receiver!\n");
		abort();
	}

	/* TODO: Also set in init_fast, needed here? */
	sis_3316_set_sample_clock_dist(m);

	/*
	 * FB Bus Enable
	 * 0 - Control out,
	 * 1 - Status out,
	 * 4 - Clock out,
	 * 5 - Clock MUX out (0 internal source, 1 from external)
	 */
	if (m->config.is_fpbus_master) {
		data = 0;
		LOGF(verbose)(LOGL, "sis3316[%d]: is FPbus master (default).",
		    m->module.id);
		data |= (1 << 4); /* Enable sample clock output on fp-bus */
                if (m->config.ext_clk_freq != 0) {
                        /* Enable external clock output on fp-bus */
                        data |= (1 << 5);
                }
		data |= (1 << 0); /* Enable all control lines on fp-bus
				     (trigger/veto, ts clear) */
		MAP_WRITE(m->sicy_map, fpbus_control, data); /* Master only */
		CHECK_REG_SET(fpbus_control, data);
	}

	if (m->config.ext_clk_freq != 0 || m->config.use_rataclock == 1) {
		/* Configure external clock multiplier */
		sis_3316_configure_external_clock_input(m);

		LOGF(verbose)(LOGL, "sis3316: wait for clock to stabilise");
		time_sleep(1.2);

		/* PLL Lock */
		MAP_WRITE(m->sicy_map, reset_adc_clock, 0x0);

		/* Wait until ADC clock / PLL is reset */
		time_sleep(30e-3);
	} else {
		/* Set clock frequency */
		sis_3316_set_clock_frequency(m);
	}

	/* ADC status. */
	/* TODO: Occurrence 4/4 of this dumping in the file! */
	for (i = 0; i < N_ADCS; ++i) {
		LOGF(verbose)(LOGL, "ADC %d status: 0x%08x", i,
		    MAP_READ(m->sicy_map, fpga_adc_status(i)));
	}

	/* Calibrate IOB _delay logic. */
	LOGF(verbose)(LOGL, "Calibrating IOB logic.");
	for (i = 0; i < N_ADCS; ++i) {
		/*
		 * 0xf00 = Calibration + Clear errors + Select all channels.
		 */
		MAP_WRITE(m->sicy_map, fpga_adc_tap_delay(i), 0xf00);
		CHECK_REG_SET(fpga_adc_tap_delay(i), 0xf00);
	}
	time_sleep(30e-3);

	sis_3316_setup_adcs(m);

	/* Channel header. */
	for (i = 0; i < N_ADCS; ++i) {
		/* write module number to header */
		data = m->config.address + ((i*4) << 20);
		MAP_WRITE(m->sicy_map, fpga_adc_header_id(i), data);
		CHECK_REG_SET(fpga_adc_header_id(i), data);
		LOGF(verbose)(LOGL, "Base address #%d: 0x%08x", i,
		    m->config.address);
		LOGF(verbose)(LOGL, "Header ID #%d: 0x%08x", i,
		    MAP_READ(m->sicy_map, fpga_adc_header_id(i)));
	}

	/* Memory test. */
	sis_3316_memtest(a_module, KW_OFF);

	/*
	 * Enable LEDs to show acquisition status.
	 */
	MAP_WRITE(m->sicy_map, control_status, (7 << 4));

	/* Disarm. */
	MAP_WRITE(m->sicy_map, disarm_sampling_logic, 1);

	/* The rest of the setup is done in init_fast */

	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
sis_3316_memtest(struct Module *a_module, enum Keyword a_memtest_mode)
{
#define ADC_FSM_N_BURSTS 64
#define ADC_FSM_BURST_SIZE 64 /* Fixed, see manual (2.8 Memory Handling) */
#define ADC_MEM_MAX_BLOCK_SIZE ADC_FSM_BURST_SIZE * ADC_FSM_N_BURSTS
	/* Some old compilers don't like huge (16B...) alignments. */
	static uint32_t *data = NULL;
	uint32_t *data_read;
	int adc = 0;
	int chunk = 0;
	int chunks = 0;
	uint32_t i = 0;
	struct Sis3316Module *m;
	/* TODO: HTJ: wondering if this change, moving block_size from above
	 * is really wanted...
	 */
	uint32_t block_size;
	double t_start, t_end;

	if (a_memtest_mode == KW_OFF) {
		return;
	}

	LOGF(verbose)(LOGL, NAME" memtest {");

	MODULE_CAST(KW_SIS_3316, m, a_module);

	if (a_memtest_mode == KW_QUICK) {
		chunks = 1;
	} else if (a_memtest_mode == KW_PARANOID) {
		chunks = 100;
	} else {
		chunks = 1000;
	}

	m->config.n_memtest_bursts = 64;
	block_size = m->config.n_memtest_bursts * ADC_FSM_BURST_SIZE;
	if (block_size > ADC_MEM_MAX_BLOCK_SIZE) {
		log_die(LOGL, "block_size > ADC_MEM_MAX_BLOCK_SIZE");
	}

	LOGF(verbose)(LOGL, "n_memtest_bursts = %u",
	    m->config.n_memtest_bursts);

	if (NULL == data) {
		static uint32_t data_storage[ADC_MEM_MAX_BLOCK_SIZE + 15];

		data = (uint32_t *)((uintptr_t)data_storage + (0xf & (16 -
		    (0xf & (uintptr_t)data_storage))));
	}
	{
		uint32_t data_read_storage[ADC_MEM_MAX_BLOCK_SIZE + 15];

		data_read = (uint32_t *)((uintptr_t)data_read_storage +
		    (0xf & (16 - (0xf & (uintptr_t)data_read_storage))));
	}
	ASSERT(uintptr_t, PRIp, ((uintptr_t)&data[0] & 0xf), ==, 0);
	ASSERT(uintptr_t, PRIp, ((uintptr_t)&data_read[0] & 0xf), ==, 0);

	/* Fill data with random words */
	for (i = 0; i < block_size; ++i) {
		data[i] = rand() + (rand() << 16);
	}

	for (adc = 0; adc < N_ADCS; ++adc) {
		uint32_t bytes = 0;

		LOGF(spam)(LOGL, "adc[%d] {", adc);

		/* Start FSM write */
		sis_3316_start_fsm(m, adc * 4, SIS3316_FSM_WRITE);
		time_sleep(1e-3);

		SERIALIZE_IO;
		t_start = time_getd();
		SERIALIZE_IO;

		/* Write to ADC FPGA memory */
		for (chunk = 0; chunk < chunks; ++chunk) {
			LOGF(spam)(LOGL, "chunk wr %d {", chunk);
			for (i = 0; i < block_size; ++i) {
				LOGF(spam)(LOGL, "write from data[%u]", i);
				MAP_WRITE(m->sicy_map,
				    adc_fifo_memory_fifo(adc), data[i]);
			}
			LOGF(spam)(LOGL, "chunk wr %d }", chunk);
			bytes += block_size;
		}

		SERIALIZE_IO;
		t_end = time_getd();
		SERIALIZE_IO;

		LOGF(verbose)(LOGL, "wrote %u bytes in %f us.\n", bytes,
		    (t_end - t_start) * 1e6);

		/* Start FSM read */
		sis_3316_start_fsm(m, adc * 4, SIS3316_FSM_READ);
		time_sleep(1e-3);

		/* Read from ADC FPGA memory */

		for (chunk = 0; chunk < chunks; ++chunk) {
			int ret = 0;

			LOGF(spam)(LOGL, "chunk rd %d {", chunk);

			SERIALIZE_IO;
			t_start = time_getd();
			SERIALIZE_IO;

			if (KW_NOBLT == m->config.blt_mode) {
			for (i = 0; i < block_size; ++i) {
				LOGF(spam)(LOGL, "read to data[%u]", i);
				data_read[i] = MAP_READ(m->sicy_map,
				    adc_fifo_memory_fifo(adc));
			}
			} else {
				if (0 > map_blt_read(m->dma_map,
				    ADC_MEM_OFFSET * (adc + 1), data_read,
				    block_size * sizeof(uint32_t))) {
					log_error(LOGL, "DMA read failed!");
				}
			}

			SERIALIZE_IO;
			t_end = time_getd();
			SERIALIZE_IO;
			LOGF(verbose)(LOGL, "read %u bytes in %f us.\n",
			    block_size, (t_end - t_start) * 1e6);

			LOGF(spam)(LOGL, "chunk rd %d }", chunk);

			/* Compare */
			ret = memcmp(data, data_read, block_size *
			    sizeof(uint32_t));
			if (ret != 0) {
				log_error(LOGL,
				    "chunk %d data does not match!",
				    chunk);
			}
			for (i = 0; i < block_size; ++i) {
				if (data[i] != data_read[i]) {
					log_error(LOGL,
					    "chunk %d data[%d] does not"
					    "match! (0x%08x != 0x%08x)",
					     chunk, i, data[i],
					     data_read[i]);
				}
			}
		}
		LOGF(verbose)(LOGL, "adc[%d] }", adc);
	}
	LOGF(verbose)(LOGL, "passed OK");
	LOGF(verbose)(LOGL, NAME" memtest }");
}

#define ADC_STATISTIC_MEMORY 0x80000000 + 0x30000000

/*
 * NOTE: This only checks the event counter of the first ADC channel in
 * each ADC group. Otherwise one would have to read 24 words every time.
 * Only used in SYNC mode.
 *
 * TODO: Run this check on *all* enabled channels, but not on every
 * event to save deadtime.
 */
uint32_t
sis_3316_get_event_counter(struct Sis3316Module *m)
{
	int i;
	uint32_t ctr[N_ADCS];
	int tested, same;
	uint32_t count;
	int tries = 0;
	const int tries_max = 3;

	LOGF(debug)(LOGL, "get_event_counter {");

get_event_counter_retry:

	/* Sample logic must not be busy, when using latched mode */
	if (POLL_OK != sis_3316_poll_sample_logic_ready(m)) {
		log_error(LOGL, "Sample logic still busy.");
		return 0;
	}

	for (i = 0; i < N_ADCS; ++i) {
		uint32_t internal_trigger_ctr;
		uint32_t addr;

		(void)internal_trigger_ctr; /* Only set, but not used */

		/* Start FSM */
		addr = ADC_STATISTIC_MEMORY;
		MAP_WRITE(m->sicy_map,
		    fpga_ctrl_status_data_transfer_control(i), addr);

		/* Sleep for at least 1 us (Struck) */
		time_sleep(1e-6); /* BL: Let's try 1 us */

		/* Read event counter value */
		/* i.e. second scaler value */
		SERIALIZE_IO;
		internal_trigger_ctr = MAP_READ(m->sicy_map, adc_fifo_memory_fifo(i));
		SERIALIZE_IO;
		ctr[i] = MAP_READ(m->sicy_map, adc_fifo_memory_fifo(i));
		SERIALIZE_IO;

#if DO_RESET_FSM
		/*
		 * Reset the FSM
		 * This is needed to clear out the FIFO until the next read.
		 */
		MAP_WRITE(m->sicy_map,
		    fpga_ctrl_status_data_transfer_control(i), 0x0);
		SERIALIZE_IO;
#endif
	}

	tested = 0;
	same = 1;
	count = -1;
	for (i = 0; i < N_ADCS; ++i) {
		if (((m->config.channels_to_read >> (i * 4)) & 1) == 1) {
			if (tested == 0) {
				count = ctr[i];
			} else {
				if (count == ctr[i]) {
					same++;
				}
			}
			tested++;
		}
	}

	if (tested > 0 && tested != same) {
		log_error(LOGL, "Event counter mismatch within ADCs: ");
		log_error(LOGL, "Comparing counter in the first channel "
		    "of each ADC group (ch 0, 4, 8, 12).");
		log_error(LOGL, "ctr = (0x%x, 0x%x, 0x%x, 0x%x), "
		    "tested = %d, same = %d", ctr[0], ctr[1], ctr[2], ctr[3],
		    tested, same);

		sis_3316_read_stat_counters(m);
		log_level_push(g_log_level_debug_);
		sis_3316_dump_stat_counters(m, 0);
		log_level_pop();

		if (tries < tries_max) {
			tries++;
			log_error(LOGL, "Retrying... (%d of %d)\n", tries,
			    tries_max);
			goto get_event_counter_retry;
		} else {
			log_die(LOGL, "Aborting...");
		}
	}

	/* we don't know, so expect increase of 1 */
	if (tested == 0) {
		count = m->module.event_counter.value + 1;
	}

	if (tries > 0) {
		LOGF(info)(LOGL, "event counter ok after %d tries.", tries);
		LOGF(info)(LOGL, "event counter = %u", count);
	}

	LOGF(debug)(LOGL, "event counter = %u", count);

	LOGF(debug)(LOGL, "get_event_counter }");

	return count;
}

#define STAT_WORDS_PER_ADC 24

void
sis_3316_read_stat_counters(struct Sis3316Module *m)
{
	int i;
	int n;
	uint32_t addr;

	LOGF(spam)(LOGL, NAME" read_stat_counters {");

	addr = ADC_STATISTIC_MEMORY;

	/*
	 * Note: Counters are cleared after each readout, so the increase
	 * since the last read is always the currently read number.
	 */
	COPY(m->stat_prev, m->stat);

	/*
	 * Each adc memory fifo contains 6 trigger counters for each of the
	 * four adc channels. In total 24 words.
	 */
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t *dest;

		dest = &m->stat.ch[i * 4].internal;

		/* Start FSM */
		MAP_WRITE(m->sicy_map,
		    fpga_ctrl_status_data_transfer_control(i), addr);

		/*
		 * Without this, the data words are not in correct order.
		 * Even in single cycle readout.
		 * According to Struck, one needs only 1 us before starting
		 * the readout.
		 */
		time_sleep(0.001); /* BL: Works well with 0.01 ms */

		/* BL: Don't do this with DMA reads. Memory is not aligned,
		 * and this is also only 24 words at a time. */
		if (1 /*KW_NOBLT == m->config.blt_mode*/) {
			for (n = 0; n < STAT_WORDS_PER_ADC; ++n) {
				*dest++ = MAP_READ(m->sicy_map,
				    adc_fifo_memory_fifo(i));
			}
		} else {
			uint32_t offset;
			offset = ADC_MEM_OFFSET * (i + 1);
			if (0 > map_blt_read(m->dma_map, offset, dest,
				    STAT_WORDS_PER_ADC * sizeof(uint32_t))) {
				log_error(LOGL, "DMA read failed!");
			}
		}

		/*
		 * Check, if the internal counter has reached the hard limit of
		 * 0xffffffff. Then, we fail fatally and have to re-init.
		 */
		if (m->stat.ch[i].internal == 0xffffffff) {
			log_error(LOGL, "Internal Counter reached limit!");
		}
		
		/* Reset the FSM */	
		MAP_WRITE(m->sicy_map,
		    fpga_ctrl_status_data_transfer_control(i), 0x0);
	}

	/*
	 * In automatic bank switching mode, we would like to see, if the
	 * highest hit increase equals the max_num_hits value. If there is
	 * a mismatch, this may point to the fact, that we've been missing
	 * out on data (e.g. double bank switch in between readout).
	 */
	if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		uint32_t max_hit_increase;

		max_hit_increase = 0;
		for (i = 0; i < N_CHANNELS; ++i) {
			uint32_t hit_increase;
			/* wrap */
			if (m->stat_prev.ch[i].hit > m->stat.ch[i].hit) {
				hit_increase = m->stat.ch[i].hit +
				    (0xffffffff - m->stat_prev.ch[i].hit);
			} else {
				hit_increase = m->stat.ch[i].hit
				    - m->stat_prev.ch[i].hit;
			}
			if (max_hit_increase < hit_increase) {
				max_hit_increase = hit_increase;
			}
		}
		if (max_hit_increase > m->max_num_hits) {
			LOGF(verbose)(LOGL, " max_hit_increase > max_num_hits"
			    " (%u > %u)", max_hit_increase,
			    m->max_num_hits);
		} else {
			LOGF(spam)(LOGL, " max_hit_increase = %u",
			    max_hit_increase);
		}
	}

	LOGF(spam)(LOGL, NAME" read_stat_counters }");
}

/*
 * Dump the trigger statistic counters.
 * 0: internal trigger counter
 * 1: hit trigger counter (if sampling took place)
 * 2: deadtime trigger counter (hit during busy)
 * 3: pileup trigger counter (trigger window active)
 * 4: veto trigger counter (hit while veto active)
 * 5: HE trigger counter (count HE suppressed hits)
 * Counters are cleared on each timestamp clear.
 * Note: Counters are latched on bank swap!
 */
void
sis_3316_dump_stat_counters(struct Sis3316Module *m, int a_show_rate)
{
	int i;
	struct Sis3316ChannelCounters total;

	ZERO(total);

	LOGF(debug)(LOGL, NAME"[%d] dump_stat_counters {", m->module.id);

	LOGF(verbose)(LOGL, "ch internal      hit   dtime  pileup (%s)",
	    (a_show_rate == 1) ? "rate" : "count");
	for (i = 0; i < N_CHANNELS; ++i) {
		struct Sis3316ChannelCounters rate;

		rate.internal = m->stat.ch[i].internal
			- m->stat_prev_dump.ch[i].internal,
		rate.hit = m->stat.ch[i].hit
			- m->stat_prev_dump.ch[i].hit;
		rate.deadtime = m->stat.ch[i].deadtime
			- m->stat_prev_dump.ch[i].deadtime;
		rate.pileup = m->stat.ch[i].pileup
			- m->stat_prev_dump.ch[i].pileup;

		if (a_show_rate == 1) {
			LOGF(verbose)(LOGL, "%2d %8d %8d %7d %7d", i, rate.internal,
			    rate.hit, rate.deadtime, rate.pileup);
		} else {
			LOGF(verbose)(LOGL, "%2d %8x %8x %8x %8x hex", i,
			    m->stat.ch[i].internal,
			    m->stat.ch[i].hit,
			    m->stat.ch[i].deadtime,
			    m->stat.ch[i].pileup);
		}

		total.internal += rate.internal;
		total.hit += rate.hit;
		total.deadtime += rate.deadtime;
		total.pileup += rate.pileup;
	}
	LOGF(verbose)(LOGL, "------------------------------------");
	LOGF(verbose)(LOGL, "[%d] t: %7.1fk %7.1fk %6.1fk %6.1fk",
	    m->module.id, total.internal / 1000., total.hit / 1000.,
	    total.deadtime / 1000., total.pileup / 1000.);

	COPY(m->stat_prev_dump, m->stat_prev);

	LOGF(debug)(LOGL, NAME"[%d] dump_stat_counters }", m->module.id);
}

uint32_t
sis_3316_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	uint32_t result = 0;
	struct Sis3316Module *m;

	LOGF(debug)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_SIS_3316, m, a_module);

	if (m->config.do_readout == 0) {
		a_module->event_counter.value++; 
		goto sis_3316_readout_done;
	}

#define DO_EXPECT_ONLY_TRIGGER_ONE
#ifdef DO_EXPECT_ONLY_TRIGGER_ONE
	/* BL: Only expect real data on trigger 1 */
	{
		unsigned gsi_mbs_trigger;
		gsi_mbs_trigger = crate_gsi_mbs_trigger_get(a_crate);
		if (gsi_mbs_trigger != 1) {
			LOGF(debug)(LOGL, NAME"skipping trigger = %d",
			    gsi_mbs_trigger);
			goto sis_3316_readout_done;
		}
	}
#endif

	/*
	 * Only in synchronous mode must the addr threshold be crossed
	 * In asynchronous mode we just check if any threshold was crossed,
	 * and return without any data, if not.
	 *
	 * In asynchronous mode with external gate, most modules won't have
	 * the address threshold flag set (only the one that made the readout
	 * request).
	 */
	if (m->config.run_mode == RM_SYNC) {
		if (POLL_OK != sis_3316_poll_addr_threshold(m)) {
			result |= CRATE_READOUT_FAIL_DATA_MISSING;
			log_error(LOGL, "Data poll failed -> done.");
			goto sis_3316_readout_done;
		}
	} else if (m->config.run_mode == RM_ASYNC) {
		LOGF(debug)(LOGL, NAME"not polling in ASYNC mode.");
	} else if (m->config.run_mode == RM_ASYNC_EXTERNAL_GATE) {
		LOGF(debug)(LOGL, NAME"not polling in ASYNC_EXTERNAL mode.");
	} else if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		/*
		 * In automatic bank switch mode the addr threshold flag
		 * won't be set anymore once we get here (already switched
		 * back). Instead, we have to check, whether or not we are
		 * still sampling on the current_bank. If this is true, we
		 * return, and come back later.
		 * TODO: Otherwise, we could swap banks now and read whatever
		 * is available (more reactive, but busier).
		 *
		 * We also need to test, whether there hasn't been any bank
		 * switch in a while at all and issue a warning (at least).
		 */
		uint32_t ctrl = MAP_READ(m->sicy_map, acquisition_control);
		int bank = (ctrl & BANK2_ARMED_FLAG) > 0 ? 1 : 0;
		double now = 0;

		now = time_getd();

		LOGF(spam)(LOGL, "Acquisition control = 0x%08x.", ctrl);
		if (bank == m->current_bank) {
			double no_read_interval = now - m->last_read;

			if (m->last_read != 0) {
				if (no_read_interval > 1
				    && now - m->last_dumped > 1) {
					LOGF(info)(LOGL, "[%d] No data for %d"
						" second(s). "
						"Is the module triggering?",
						m->module.id,
						(int)no_read_interval);
					sis_3316_dump_stat_counters(m, 1);
					m->last_dumped = now;
				}
				if (no_read_interval > 10) {
					result |=
						CRATE_READOUT_FAIL_DATA_MISSING;
					LOGF(info)(LOGL,
					    NAME"no_read_interval > 10");
					log_error(LOGL,"[%d] Re-initialising.",
						m->module.id);
				}
			}
			/*result |= CRATE_READOUT_FAIL_DATA_MISSING;*/
			LOGF(debug)(LOGL, "No data (same bank).");
			a_module->event_counter.value++; 
			goto sis_3316_readout_done;
		}
	} else {
		log_die(LOGL,"Unsupported run mode.");
	}

	/*
	 * Only swap banks, if we are not doing it automatically.
	 */
	if (m->config.run_mode != RM_ASYNC_AUTO_BANK_SWITCH) {
		sis_3316_swap_banks(m);
	} else {
		m->current_bank = !m->current_bank;
	}

	/*
	 * Note: event counter scalers are latched on bank swap.
	 *       So, swapping has to be done before this step.
	 */
	if (m->config.run_mode == RM_SYNC) {
		int n_tries = 0;
		uint32_t prev = a_module->event_counter.value;

#ifdef DO_CLEAR_TIMESTAMP
		uint32_t event_counter_saved = prev;
		prev = 0;
#endif
		/* BL: TODO this needs to be expanded for multi-event */
		while (1) {
			a_module->event_counter.value = sis_3316_get_event_counter(m);
			if (prev + 1 == a_module->event_counter.value) {
				break;
			} else {
				log_error(LOGL,
				    "Event counter (prev = 0x%08x, now = "
				    "0x%08x) not increased by 1. "
				    "Retry reading.",
				    prev, a_module->event_counter.value);
				n_tries++;
			}
		}
		if (n_tries > 0) {
			log_error(LOGL, "Event counter (0x%08x) OK after %d tries.",
			    a_module->event_counter.value, n_tries);
		}

	}
	else if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH
		|| m->config.run_mode == RM_ASYNC) {
		a_module->event_counter.value++; 
	}
sis_3316_readout_done:
	LOGF(debug)(LOGL, NAME" readout_dt(ctr=0x%08x) }",
	    a_module->event_counter.value);
	return result;
}

uint32_t
sis_3316_readout_shadow(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	(void)a_crate;
	(void)a_module;
	(void)a_event_buffer;
	return 0;
	/*
	 * To read out data from the module, one has to first initiate
	 * a memory transfer from the DDR memory to the output FIFO.
	 * This has to be done for each channel separately.
	 *
	 * However, for each ADC FPGA, one transfer from one of the four
	 * channels can be active at all times, because only a single
	 * memory FIFO exists per ADC.
	 *
	 * For the shadow to work, one would first need to know, if any
	 * of the enabled channels have one or more events stored, i.e.
	 * an 'actual_sample_address' larger than 'event_length'. Or,
	 * alternatively, 'address_threshold' can be set to the size of
	 * a single event (with sampling to continue after crossing
	 * the threshold).
	 *
	 * If address threshold is crossed, then 'actual_sample_address'
	 * has to be checked for each enabled channel.
	 *
	 * hits in the buffer:
	 * n = (actual_sample_address - previous_read_end) / event_size
	 *
	 * If the actual_sample_address is above 75% of the buffer size,
	 * a bank swap needs to be initiated. After this, return from
	 * the readout and continue next time. Poll the swap flag and
	 * continue, if swap is complete.
	 *
	 * Then, one would initiate the data transfers on each enabled
	 * channel starting from the previous_read_end and then read
	 * the data from the output FIFOs.
	 *
	 * Whenever the readout_shadow has the 'trigger' flag set,
	 * a bank swap is advised to continue sampling on the
	 * other bank and to be able to read out the rest of the data.
	 *
	 * So, summary:
	 *
	 * case no_data:
	 *   -> 1 VME read (address_threshold)
	 * case data_in_one_channel:
	 *   -> 1 VME read (address_threshold)
	 *   -> 1..16 VME reads (actual_sample_address)
	 *   -> 1 VME write (FSM start)
	 *   -> 1 transfers from FIFO
	 *      decide whether or not to use DMA by number of words
	 *      to read
	 * case data_in_several_channels:
	 *   -> 1 VME read (address_threshold)
	 *   -> 1..16 VME reads (actual_sample_address)
	 *   -> 1..16 VME writes (FSM start)
	 *   -> 1..16 transfers from FIFO
	 *      decide whether or not to use DMA by number of words
	 *      to read
	 *
	 */
}

uint32_t
sis_3316_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	char const read_seq[16] = {
		0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
	};
	struct Sis3316Module *m;
	uint32_t *outp, *ultrastart;
	uint32_t words_to_read;
	uint32_t total_bytes_to_read;
	unsigned int i;
	uint32_t fsm_started = 0;
	uint32_t result;
	int ok = 0;
	uint32_t event_num;
	double now = 0;

	(void)a_crate;

	LOGF(debug)(LOGL, NAME" readout {");
	result = 0;

	now = time_getd();

	MODULE_CAST(KW_SIS_3316, m, a_module);

	if (m->config.do_readout == 0) {
		goto sis_3316_readout_done;
	}

	outp = a_event_buffer->ptr;
	ultrastart = outp;

	LOGF(debug)(LOGL, "Current_bank = %d", m->current_bank);

	/* calculate number of events */
	switch (m->config.run_mode) {
	case RM_SYNC:
		/*
		 * TODO: MUST *really* fix this.
		 */
		event_num = 1/*crate_get_event_max(a_crate)*/;
		break;
	case RM_ASYNC:
		event_num = m->config.async_max_events;
		break;
	case RM_ASYNC_EXTERNAL_GATE:
		/* TODO. */
		log_die(LOGL, "Not implemented!");
		break;
	case RM_ASYNC_AUTO_BANK_SWITCH:
		event_num = m->config.async_max_events;
		break;
	default:
		log_die(LOGL, "Not implemented!");
		break;
	}

	/* Check event data will fit into buffer */
	total_bytes_to_read = 4; /* ID word */
	for (i = 0; i < N_ADCS; ++i) {
		total_bytes_to_read += m->config.event_length[i] * 4 *
		    event_num;
	}
	if (total_bytes_to_read > a_event_buffer->bytes) {
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		log_error(LOGL, NAME" fail, too much data (%d"
		    " > %" PRIz ")",
		    total_bytes_to_read, a_event_buffer->bytes);
		goto sis_3316_readout_done;
	}

	/*
	 * In async mode the module is disarmed, then all statistic counters
	 * are read and dumped. Then timestamps and counters are cleared
	 * before banks are swapped.
	 *
	 * In async mode with automatic switching, we only read the stat
	 * counters.
	 */
	if (m->config.run_mode == RM_ASYNC) {
		sis_3316_disarm(m);
		sis_3316_read_stat_counters(m);
		sis_3316_dump_stat_counters(m, 1);
		/*
		 * TODO: Check that we really don't need to clear the
		 * timestamp.
		 */
	} else if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		sis_3316_read_stat_counters(m);
		/**
		 * Dump statistic counters, if log level is verbose,
		 * on every second
		 */
		if ((int)now > (int)m->last_dumped) {
			sis_3316_dump_stat_counters(m, 1);
			m->last_dumped = now;
		}
		/*
		 * Never do a reset, so that timestamps will
		 * continue counting -> timestamp correlations possible.
		 */
	}

	/* Read all channels */
	for (i = 0; i < N_CHANNELS; ++i) {
		void const *start;
		int ch;
		int adc;

		start = outp;
		ch = read_seq[i];
		adc = ch / 4;

		words_to_read = m->config.event_length[adc] * event_num;

		/*
		 * We keep track of the readout FSMs which are already started.
		 * One FSM is started for each set of four channels.
		 * We try to start the FSMs as efficiently as possible, such
		 * that all four FSMs will run in parallel most of the time.
		 */
		if (((fsm_started >> ch) & 1) == 0) {
			int j;

			for (j = 0; j < 4; ++j) {
				int fsm_ch = read_seq[i+j];

				if (((m->config.channels_to_read >> fsm_ch) &
				    1) == 1) {
					LOGF(spam)(LOGL, "Start FSM %d.",
					    fsm_ch);
					sis_3316_start_fsm(m, fsm_ch,
					    SIS3316_FSM_READ);

				}

				fsm_started |= (1 << fsm_ch);
			}

			/*
			 * FIXME: Wait until fsm is really started.
			 * The memory check does this using time_sleep().
			 */
			for (j = 0; j < N_DUMMY_READS_AFTER_FSM; ++j) {
				int dummy;
				SERIALIZE_IO;
				dummy = MAP_READ(m->sicy_map,
				    fpga_adc_end_addr_threshold(0));
				(void)dummy;
			}
			/* time_sleep(1); */

		}

		/*
		 * Read data from each channel, only if enabled.
		 */
		if (((m->config.channels_to_read >> ch) & 1) == 1) {
			struct EventConstBuffer ebuf;
			uint32_t words_in_channel;

			LOGF(spam)(LOGL, "Ch[%d]: %"PRIz" bytes buffer left "
			    "before readout.", ch, a_event_buffer->bytes);

			words_in_channel = 0;
			result |= sis_3316_test_channel(m, ch, words_to_read,
			    &words_in_channel, a_event_buffer->bytes,
			    event_num);
			if (0 != result) {
				log_error(LOGL, NAME
				    " ch[%d] test_channel failed", ch);
				goto sis_3316_readout_done;
			}
			if (0 >= words_in_channel) {
				goto sis_3316_channel_readout_done;
			}

			if (KW_NOBLT == m->config.blt_mode) {
				result |= sis_3316_read_channel(m, ch,
				    words_in_channel, a_event_buffer);
			} else {
				result |= sis_3316_read_channel_dma(m, ch,
				    words_in_channel, a_event_buffer);
			}
			if (0 != result) {
				log_error(LOGL, NAME
				    " ch[%d] read_channel[_dma] failed", ch);
				goto sis_3316_readout_done;
			}

sis_3316_channel_readout_done:
			LOGF(spam)(LOGL, "Ch[%d]: %"PRIz" bytes buffer left "
			    "after readout", ch, a_event_buffer->bytes);

			ebuf.ptr = start;
			ebuf.bytes = (uintptr_t)outp - (uintptr_t)start;
			ok += sis_3316_check_channel_data(m, ch, &ebuf);
			/*
			 * Reset the FSM
			 * This is needed to clear out the FIFO until the next read.
			 */
			MAP_WRITE(m->sicy_map,
			    fpga_ctrl_status_data_transfer_control(adc), 0x0);
			SERIALIZE_IO;
		}
	}

	if (ok != 0) {
		log_error(LOGL, "Corrupted data!");
	}

	if (g_log_level_spam_ == log_level_get()) {
		sis_3316_dump_data(ultrastart, outp);
	}

	/*
	 * In auto bank switch mode, it may be that bank switching occurs only
	 * every now and then (at low rate). Therefore, the num_hits value is
	 * adjusted automatically based on the time that passed since the last
	 * readout.
	 * The value of num_hits is varied between 32 and max_num_hits.
	 *
	 * TODO: Make this a configuration option.
	 */
	if (m->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH) {
		if (m->last_read != 0 && now - m->last_read > 1.0
		    && m->num_hits > 32) {
			/* Decrease */
			sis_3316_adjust_address_threshold(m, 0.8);
		}
		if (m->last_read != 0 && now - m->last_read < 0.001
		    && m->num_hits < m->max_num_hits) {
			/* Increase */
			sis_3316_adjust_address_threshold(m, 2.0);
		}
		m->last_read = now;

		sis_3316_test_clock_sync(m);
	
	}

sis_3316_readout_done:
	LOGF(debug)(LOGL, NAME" readout(0x%08x) }", result);
	return result;
}

void
sis_3316_test_clock_sync(struct Sis3316Module *self)
{
	/* test, if all clocks are still synced */
	if (self->config.has_rataclock_receiver == 1) {
		int tries_left = 3;
		int ok = 1;

		while (tries_left > 0) {
			int i;

			for (i = 0; i < N_ADCS; ++i) {
				uint32_t status = MAP_READ(self->sicy_map,
				    fpga_adc_rataser_status1(i));
				int sync = status & 0x7;
				int lost = (status >> 4) & 0x7;
				int auto_edge =
				    self->config.rataclock_use_auto_edge;
				if (sync != 7) {
					log_error(LOGL, NAME
					    " ADC%d clock not synced "
					    "(sync = %d) "
					    "(tries_left = %d)", i, sync,
					    tries_left);
					ok = 0;
				}
				if (lost != 0) {
					log_error(LOGL, NAME
					    " ADC%d clock lost sync"
					    "(lost = %d)", i, lost);
					ok = 0;
					MAP_WRITE(self->sicy_map,
					    fpga_adc_rataser_control2(i),
					    (1 << 31) | (auto_edge << 2));

				}
			}
			if (ok == 0) {
				tries_left -= 1;
				time_sleep(1e-3);
			} else {
				LOGF(debug)(LOGL, "rataser clock synced.");
				break;
			}
			MAP_WRITE(self->sicy_map, reset_adc_clock, 0x0);
			time_sleep(30e-3);
		}
	}
}

void
sis_3316_setup_(void)
{
	MODULE_SETUP(sis_3316, 0);
	MODULE_CALLBACK_BIND(sis_3316, post_init);
	MODULE_CALLBACK_BIND(sis_3316, readout_shadow);
}

/*
 * Adjust the address threshold based on the current value of m->num_hits
 * and the adjustment factor a_factor.
 */
void
sis_3316_adjust_address_threshold(struct Sis3316Module *m, double a_factor)
{
	uint32_t num_hits;
	int i;

	num_hits = m->num_hits * a_factor;

	if (num_hits <= 0) {
		num_hits = 1;
	} else if (num_hits > m->max_num_hits) {
		num_hits = m->max_num_hits;
	}

	m->num_hits = num_hits;

	LOGF(verbose)(LOGL, "[%d] Adjusting num hits to: %d", m->module.id,
	    num_hits);

	/* Address threshold (given in number of words) */
	for (i = 0; i < N_ADCS; ++i) {
		uint32_t data;
		data = (m->config.event_length[i] * num_hits) - 1;
		data |= (1 << 31); /* Enable stop sampling at addr_thr */
		MAP_WRITE(m->sicy_map, fpga_adc_end_addr_threshold(i), data);
		CHECK_REG_SET(fpga_adc_end_addr_threshold(i), data);
		LOGF(debug)(LOGL, "Addr_thr[%d] = 0x%08x", i, data);
	}
}

uint32_t
sis_3316_test_channel(struct Sis3316Module *a_sis3316, int a_ch, uint32_t
    a_words_to_read, uint32_t *a_actual_words, size_t a_dst_bytes, uint32_t
    a_num_hits)
{
	int adc;
	uint32_t words_to_read;
	uint32_t words_expected;

	LOGF(spam)(LOGL, NAME" test_channel %d {", a_ch);

	adc = a_ch / 4;

	/* Test, if sampling on previous bank has stopped */
	if (POLL_OK != sis_3316_poll_prev_bank(a_sis3316, a_ch)) {
		log_error(LOGL, NAME
		    " bank of channel not ready for readout.");
		return CRATE_READOUT_FAIL_DATA_MISSING;
	}

	/* The number of words to read is equal to the sampling address */
	words_to_read = MAP_READ(a_sis3316->sicy_map,
	    channel_previous_bank_address(a_ch)) & 0xffffff;

	/* Test, if the available data fits into the provided buffer */
	if (words_to_read * 4 > a_dst_bytes) {
		log_error(LOGL, NAME " channel data doesn't fit in buffer.");
		log_error(LOGL, NAME " bytes to read = %d, a_dst_bytes = %d",
		    (int)(words_to_read * 4), (int)a_dst_bytes);
		return CRATE_READOUT_FAIL_DATA_TOO_MUCH;
	}

	if (a_sis3316->config.run_mode == RM_SYNC) {

		/*
		 * In SYNC mode the expected amount of data must always match
		 * the available amount. No excuses!
		 */
		if (words_to_read > a_words_to_read) {
			log_error(LOGL, NAME
			    " channel data greater than expected.");
			log_error(LOGL, NAME" has %d, expected %d",
			    words_to_read, a_words_to_read);
			log_error(LOGL, NAME" Is the busy output connected?.");
			return CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		}
		if (words_to_read < a_words_to_read) {
			log_error(LOGL, NAME
			    " channel data less than expected.");
			log_error(LOGL, NAME" has %d, expected %d",
			    words_to_read, a_words_to_read);
			log_error(LOGL, NAME" Is the busy output connected?.");
			return CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		}

		/*
		 * In SYNC mode the actual amount of data must match the
		 * expected event lengths.
		 */
		words_expected = a_num_hits *
		    a_sis3316->config.event_length[adc];

		if (words_to_read > words_expected) {
			log_error(LOGL, NAME" More data than expected\n "
			    "(read = %d words, expected = %d words)",
			    words_to_read, words_expected);
			log_error(LOGL, NAME" Is the busy output connected?");
			return CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		}
		if (words_to_read < words_expected) {
			log_error(LOGL, NAME" Less data than expected\n "
			    "(read = %d words, event length = %d words)",
			    words_to_read, words_expected);
			log_error(LOGL, NAME" Are all triggers received?");
			return CRATE_READOUT_FAIL_DATA_MISSING;
		}
	}

	LOGF(debug)(LOGL, "Channel has %d words of data and we expected %d "
	    "(not critical).", words_to_read, a_words_to_read);

	*a_actual_words = words_to_read;

	LOGF(spam)(LOGL, NAME" test_channel %d}", a_ch);
	return 0;
}

uint32_t
sis_3316_read_channel(struct Sis3316Module *a_sis3316, int a_ch, uint32_t
    a_words_to_read, struct EventBuffer *a_event_buffer)
{
	uint32_t* outp;
	uint32_t* start;
#ifdef TEST_FOR_ZERO_WORD
	uint32_t word_count = 0;
#endif
	int adc;

	LOGF(spam)(LOGL, NAME" read_channel %d {", a_ch);

	outp = a_event_buffer->ptr;
	start = outp;
	adc = a_ch/4;

	if (a_words_to_read * sizeof(uint32_t) > a_event_buffer->bytes) {
		log_die(LOGL, "sis_3316_read_channel words_to_read=%d too "
		    "many for buffer bytes=%"PRIz".", a_words_to_read,
		    a_event_buffer->bytes);
	}

	/* Add channel header with channel number */
	*(outp++) = 0x33160000
	    + (a_ch << 8)
	    + (a_sis3316->config.address >> 24);

	/* Add channel header with the number of hits */
	*(outp++) = a_words_to_read / a_sis3316->config.event_length[adc];

	LOGF(spam)(LOGL, "Before read: %08x %08x %08x %08x ...", *(start),
	    *(start+1), *(start+2), *(start+3));

	while (a_words_to_read--) {
		SERIALIZE_IO;
		*outp = MAP_READ(a_sis3316->sicy_map,
		    adc_fifo_memory_fifo(adc));
#ifdef TEST_FOR_ZERO_WORD
		if (word_count == 0 && *outp == 0x0) {
			LOGF(verbose)(LOGL,
			    "a_ch: %d data = 0x0, word_count = %d, "
			    "a_words_to_read = %d, words_to_read = %d", a_ch,
			    word_count, a_words_to_read, words_to_read);
		}
		++word_count;
#endif
		++outp;
	}

	LOGF(spam)(LOGL, "After read: %08x %08x %08x %08x ...", *(start),
	    *(start+1), *(start+2), *(start+3));

	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

	LOGF(spam)(LOGL, NAME" read_channel %d }", a_ch);
	return 0;
}

uint32_t
sis_3316_read_channel_dma(struct Sis3316Module* a_sis3316, int a_ch, uint32_t
    a_words_to_read, struct EventBuffer *a_event_buffer)
{
	uint32_t* outp;
	uint32_t* header;
	uint32_t filler;
	uint32_t n_padding;
	uint32_t offset;
	uint32_t bytes_to_read;
	int adc;

	LOGF(spam)(LOGL, NAME" read_channel %d with DMA {", a_ch);

	outp = a_event_buffer->ptr;
	header = outp++;

	adc = a_ch / 4;

	/* Add channel header with the number of hits */
	*outp++ = a_words_to_read / a_sis3316->config.event_length[adc];

	if (a_words_to_read * sizeof(uint32_t) > a_event_buffer->bytes) {
		log_die(LOGL, "sis_3316_read_channel_dma words_to_read=%d "
		    "too many for buffer bytes=%"PRIz".", a_words_to_read,
		    a_event_buffer->bytes);
	}

	/* Add appropriate padding */
	filler = 0x33160000
	    + (a_ch << 8)
	    + (a_sis3316->config.address >> 24);

	bytes_to_read = a_words_to_read * sizeof(uint32_t);

	n_padding = 0;
	if (a_sis3316->config.blt_mode == KW_MBLT) {
		if (((intptr_t)outp & 0x7) != 0) {
			*(outp++) = filler;
			++n_padding;
		}
		bytes_to_read =
		    ((a_words_to_read * sizeof(uint32_t)) + 1) & ~0x7;
	} else if (a_sis3316->config.blt_mode == KW_BLT_2ESST ||
	           a_sis3316->config.blt_mode == KW_BLT_2EVME) {
		while (((intptr_t) outp & 0xf) != 0) {
			*(outp++) = filler;
			++n_padding;
		}
		bytes_to_read =
		    ((a_words_to_read * sizeof(uint32_t)) + 1) & ~0xf;
	}

	/* Add padding value to channel header */
	filler |= (n_padding << 12);

	/* Add channel header */
	*header = filler;

	/* Skip this part, if this should not be discarded */
	if (((a_sis3316->config.discard_data >> a_ch) & 1) == 1)
	{
		uint32_t baseline;
		uint32_t energy;
		uint32_t *header_end_ptr;
		uint32_t *avg_samples_ptr;
		uint32_t header_end;
		char status_flag;
		volatile uint32_t *adc_mem;

		LOGF(spam)(LOGL, "Peeking into event header [%d] {", a_ch);
		LOGF(spam)(LOGL, "a_words_to_read = %d", a_words_to_read);
		LOGF(spam)(LOGL, "use_maw3 = %d", a_sis3316->config.use_maw3);

		/* Have to read a multiple of 4 words in 2eSST mode to stay 
		 * properly aligned. Settle for 8, we'll have the status flag
		 * in there in any case. */
		assert(a_words_to_read >= 8);
		/* Assume that the maw3 values are not read out */
		assert(a_sis3316->config.use_maw3 == 0);

		adc_mem = a_sis3316->arr->adc_fifo_memory_fifo[adc];

		*outp++ = *adc_mem++; /* timestamp 1 */
		*outp++ = *adc_mem++; /* timestamp 2 */

		/* baseline */
		baseline = *adc_mem++;
		*outp++ = baseline;
		LOGF(spam)(LOGL, "baseline = 0x%08x", baseline);

		/* energy */
		energy = *adc_mem++;
		*outp++ = energy;
		LOGF(spam)(LOGL, "energy = 0x%08x", energy);

		/* header_end */
		header_end_ptr = outp; /* save this memory location for later */
		header_end = *adc_mem++;
		*outp++ = header_end;
		LOGF(spam)(LOGL, "header_end = 0x%08x", header_end);
		assert((header_end & 0xf0000000) == 0xa0000000);

		avg_samples_ptr = outp;
		*outp++ = *adc_mem++; /* average samples */
		*outp++ = *adc_mem++; /* adc data */
		*outp++ = *adc_mem++; /* adc data */

		a_words_to_read -= 8;
		bytes_to_read -= 8 * 4;

		LOGF(spam)(LOGL, "Peeking into event header }");

		/* check the condition, if should stop reading this channel */
		status_flag = (header_end >> 26) & 0x1;

		if (a_sis3316->config.discard_threshold == 0) {
			if (status_flag == 1) {
				LOGF(spam)(LOGL, "Status flag is set!");
			} else {
				LOGF(spam)(LOGL, "Status flag is not set! SKIP!");

				/* 
				 * rewrite *header_end_ptr and *avg_samples_ptr to not confuse 
				 * the unpacker.
				 */
				LOGF(spam)(LOGL, "Rewriting header_end_ptr:  %08x",
				    *header_end_ptr);
				LOGF(spam)(LOGL, "Rewriting avg_samples_ptr: %08x",
				    *avg_samples_ptr);

				*header_end_ptr =  0xa0000000; /* status flag always 0 here */
				*avg_samples_ptr = 0xe0000002; /* two data words following */

				goto end_readout_channel_dma;
			}
		} else {
			log_error(LOGL, "Decision based on energy threshold not implemented yet!");
			abort();
		}
	} else {
		LOGF(spam)(LOGL, "[%d] This channel data is never discarded", a_ch);
	}

	offset = ADC_MEM_OFFSET * (adc + 1);

	LOGF(spam)(LOGL, "DMA: target = %p, "
	    "a_words_to_read = 0x%08x, bytes_to_read = 0x%08x, mode = %d",
	    (void const *)outp,
	    a_words_to_read, bytes_to_read, a_sis3316->config.blt_mode);

	if (0 > map_blt_read(a_sis3316->dma_map, offset, outp,
	    bytes_to_read)) {
		log_error(LOGL, "DMA read failed!");
	}

	outp += a_words_to_read;

end_readout_channel_dma:
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);

	LOGF(spam)(LOGL, "After DMA: target = %p.", a_event_buffer->ptr);

	LOGF(spam)(LOGL, NAME" read_channel %d with DMA }", a_ch);
	return 0;
}

/*
 * Note: This starts the transfer FSM. After starting for one channel,
 * one needs to wait ~1 us until starting the read from the FIFO to
 * ensure that data is present.
 * For normal data readout this is not necessary, since first all
 * four ADC FSMs are started before the readout is done.
 * However, for the statistic counter, one needs to pay attention and
 * do a little sleep.
 */
void
sis_3316_start_fsm(struct Sis3316Module* a_module, int a_fsm_ch, enum
    Sis3316FsmMode a_mode)
{
	uint32_t addr;
	int adc;

	adc = a_fsm_ch >> 2;

	if (a_mode == SIS3316_FSM_READ) {
		addr = 0x80000000;
	} else {
		addr = 0xc0000000;
	}

	/*
	 * Note: We are swapping banks BEFORE doing the fsm
	 * this means, current_bank is actually already the OTHER
	 * bank, which we don't want to read.
	 * This is also true for the automatic switching. The
	 * banks should have been swapped already.
	 */
	if (a_module->current_bank != 1) {
		addr += 0x01000000;
	}
	if ((a_fsm_ch & 1) != 0) {
		addr += 0x02000000;
	}
	if ((a_fsm_ch & 2) != 0) {
		addr += 0x10000000;
	}

	LOGF(spam)(LOGL, "start transfer: adc %d, addr = 0x%08x", adc, addr);

	MAP_WRITE(a_module->sicy_map,
	    fpga_ctrl_status_data_transfer_control(adc), addr);
}

/*
 * This is not called in RM_ASYNC_AUTO_BANK_SWITCH mode.
 * This means, that in RM_ASYNC_AUTO_BANK_SWITCH mode,
 * the current_bank needs to be flipped somewhere else.
 */
void
sis_3316_swap_banks(struct Sis3316Module* m)
{


#if 0
	int prev_bank, cur_bank;
	cur_bank = (MAP_(m->sicy_map, channel_actual_sample_address[0])
	    & 0x01000000) >> 24;
	LOGF(debug)(LOGL, "cur_bank = %d", cur_bank);
	prev_bank = (MAP_(m->sicy_map, channel_previous_bank_address[0])
	    & 0x01000000) >> 24;
	LOGF(debug)(LOGL, "prev_bank = %d", prev_bank);

	m->current_bank = cur_bank;
#endif
	LOGF(debug)(LOGL, "swap_banks (current_bank=%d) {", m->current_bank);

	if (m->current_bank == 0) {
		LOGF(debug)(LOGL, NAME" arming bank 1");
		MAP_WRITE(m->sicy_map, disarm_and_arm_bank2, 1);
		m->current_bank = 1;
	} else {
		LOGF(debug)(LOGL, NAME" arming bank 0");
		MAP_WRITE(m->sicy_map, disarm_and_arm_bank1, 1);
		m->current_bank = 0;
	}
	LOGF(debug)(LOGL, "swap_banks }");
}

void
sis_3316_clear_timestamp(struct Sis3316Module *m)
{
	/*
	 * the timestamp should never be cleared when running with
	 * rataclock. there is no sense in that.
	 */
	if (m->config.use_rataclock == 0
		&& m->config.use_rataclock_timestamp == 0) {
		MAP_WRITE(m->sicy_map, timestamp_clear, 1);
	}
}

void
sis_3316_disarm(struct Sis3316Module *m)
{
	MAP_WRITE(m->sicy_map, disarm_sampling_logic, 1);
}

int
sis_3316_test_addr_threshold(struct Sis3316Module *m)
{
	return MAP_READ(m->sicy_map, acquisition_control) & ADDRESS_THR_FLAG;
}

/*
 * TODO: One can also poll the previous_bank_sample_address
 * and then compare with the expected address, i.e.
 * (num_hits * event_length).
 * This may be useful in case the address threshold gets
 * set to a high value, but num_hits is limited via the
 * config.
 */
int
sis_3316_poll_sample_logic_ready(struct Sis3316Module *m)
{
	uint32_t status;
	uint32_t poll_counter = 0;
	uint32_t poll_timeout = 1000000;

	do {
		status = MAP_READ(m->sicy_map, acquisition_control) &
		    SAMPLE_LOGIC_BUSY_FLAG;
		SERIALIZE_IO;
		++poll_counter;
	} while (status == 0 && poll_counter < poll_timeout);

	if (poll_counter == poll_timeout) {
		log_error(LOGL, "[%d]: timeout while waiting for "
		    "sample logic to become ready.", m->module.id);
		log_error(LOGL, "[%d]: current sample addr = 0x%x",
		    m->module.id, MAP_READ(m->sicy_map,
		    channel_actual_sample_address(0)));
		return POLL_TIMEOUT;
	} else {
		return POLL_OK;
	}
}

/*
 * TODO: One can also poll the previous_bank_sample_address
 * and then compare with the expected address, i.e.
 * (num_hits * event_length).
 * This may be useful in case the address threshold gets
 * set to a high value, but num_hits is limited via the
 * config.
 */
int
sis_3316_poll_addr_threshold(struct Sis3316Module* m)
{
	uint32_t status;
	uint32_t poll_counter = 0;
	uint32_t poll_timeout = 1000000;

	do {
		status = MAP_READ(m->sicy_map, acquisition_control) &
		    ADDRESS_THR_FLAG;
		SERIALIZE_IO;
		++poll_counter;
	} while (status == 0 && poll_counter < poll_timeout);

	if (poll_counter == poll_timeout) {
		log_error(LOGL, "[%d]: timeout while polling for "
		    "addr threshold (thr = 0x%x)", m->module.id,
		    MAP_READ(m->sicy_map, fpga_adc_end_addr_threshold(0)));
		log_error(LOGL, "[%d]: current sample addr = 0x%x",
		    m->module.id,
		    MAP_READ(m->sicy_map, channel_actual_sample_address(0)));
		return POLL_TIMEOUT;
	} else {
		return POLL_OK;
	}
}

int
sis_3316_poll_prev_bank(struct Sis3316Module* m, int i)
{
	int status;
	uint32_t poll_counter = 0;
	uint32_t poll_timeout = 10000000;

	do {
		status = (
		    MAP_READ(m->sicy_map, channel_previous_bank_address(i)) &
		    0x01000000) >> 24;
		SERIALIZE_IO;
		++poll_counter;
	} while (status == m->current_bank && poll_counter < poll_timeout);

	if (poll_counter == poll_timeout) {
		log_error(LOGL, "[%d]: timeout while polling for "
		    "previous bank addr (bank %d != %d) on ch %d",
		    m->module.id, status, m->current_bank, i);
		return POLL_TIMEOUT;
	} else {
		return POLL_OK;
	}
}

void
sis_3316_dump_data(void const *start, void const *end)
{
	uint32_t const *p = start;
	int cnt = 0;

	printf("Data dump:\n");
	printf("------------------------\n");
	while(p != end) {
		if (cnt % 8 == 0) printf("\n%05x: ", cnt);
		printf("%08x ", *p++);
		++cnt;
	}
	printf("\n");
}

int
sis_3316_check_channel_data(struct Sis3316Module *a_sis3316, int a_ch,
    struct EventConstBuffer *a_event_buffer)
{
	uint32_t hit;
	uint32_t num_hits = 0;

	LOGF(spam)(LOGL, NAME" check_channel_data[%d] {", a_ch);

	if (a_sis3316->config.check_level == CL_OFF) {
		LOGF(spam)(LOGL, "Skipped.");
		goto sis_3316_check_channel_data_done;
	}
	if (0 == a_event_buffer->bytes) {
		LOGF(spam)(LOGL, "No data.");
		goto sis_3316_check_channel_data_done;
	}

	num_hits = sis_3316_check_channel_padding(a_sis3316, a_ch,
	    a_event_buffer);

	hit = 0;
	for (;;) {
		if (0 == a_event_buffer->bytes) {
			LOGF(spam)(LOGL, "Found %d hits.", hit);
			goto sis_3316_check_channel_data_done;
		}
		sis_3316_check_hit(a_sis3316, a_ch, hit, a_event_buffer);
		++hit;
	}

	if (hit != num_hits) {
		log_die(LOGL, "hit counter mismatch: expect %d, got %d",
		    num_hits, hit);
	}

sis_3316_check_channel_data_done:
	LOGF(spam)(LOGL, NAME" check_channel_data }");
	return 0;
}

uint32_t
sis_3316_check_channel_padding(struct Sis3316Module *a_sis3316, int a_ch,
    struct EventConstBuffer *a_event_buffer)
{
	uint32_t const *p;
	uint32_t filler;
	int expect_padding = 0;
	uint32_t num_hits = 0;

	LOGF(spam)(LOGL, NAME" check_channel_padding {");

	p = a_event_buffer->ptr;
	filler = 0x33160000
	    + (a_ch << 8)
	    + (a_sis3316->config.address >> 24);

	/* Check channel header. */
	if (a_sis3316->config.check_level >= CL_SLOPPY) {
		if ((*p & 0xffff0fff) != filler) {
			log_error(LOGL, NAME" Missing channel "
			    "header/padding.");
			goto err;
		}
	}

	/*
	 * Number of words used as padding is written in bits 12-15 of
	 * channel header.
	 */
	expect_padding = (*p >> 12) & 0xf;
	++p;

	/*
	 * Number of hits in this channel written in the second word
	 */
	num_hits = (*p & 0x3ff);
	++p;

	/* Check padding */
	if (a_sis3316->config.check_level >= CL_PARANOID) {
		int padding_counter = expect_padding;
		while (padding_counter--) {
			if (*p != filler) {
				log_error(LOGL,
				    "Data padding is incorrect.");
				log_error(LOGL,
				    "Expected %d words, missing %d.",
				    expect_padding, padding_counter + 1);
				log_error(LOGL,
				    "start = 0x%p, p = 0x%p",
				    a_event_buffer->ptr, (void const *)p);
				goto err;
			}
			++p;
		}
	} else {
		p += expect_padding;
	}

	EVENT_BUFFER_ADVANCE(*a_event_buffer, p);

	LOGF(spam)(LOGL, NAME" check_channel_padding }");
	return num_hits;

err:
	time_sleep(1);
	/* TODO: Return error-code so crate dumps and re-inits. */
	sis_3316_dump_data(a_event_buffer,
	    (uint8_t const *)a_event_buffer->ptr + a_event_buffer->bytes);
	log_die(LOGL, "Corrupt data.");
}

void
sis_3316_check_hit(struct Sis3316Module *a_sis3316, int a_ch, int a_hit,
    struct EventConstBuffer *a_event_buffer)
{
	uint32_t const *p;
	uint32_t raw_buffer_words;
	uint32_t maw_buffer_words;
	int adc;

	LOGF(spam)(LOGL, NAME" check_channel_hit[%d] {", a_hit);

	p = a_event_buffer->ptr;

	adc = a_ch/4;

	raw_buffer_words = a_sis3316->config.sample_length[adc]/2;
	maw_buffer_words = a_sis3316->config.sample_length_maw[adc];

	/* Check hit data end. */
	if (a_sis3316->config.check_level >= CL_SLOPPY) {
		int header_length;
		header_length = a_sis3316->config.event_length[adc]
			- raw_buffer_words - maw_buffer_words;
		p += (header_length - 1);
		if (((*p >> 28) & 0xf) != 0xe) {
			log_error(LOGL,
			    " header end not found, instead 0x%08x", *p);
			log_error(LOGL, " header length = %d", header_length);
			goto err;
		}
		++p;
	}

	/* Skip raw sample buffer. */
	if (a_sis3316->config.check_level >= CL_PARANOID) {
		if (!MEMORY_CHECK(*a_event_buffer, &p[raw_buffer_words - 1]))
		{
			log_error(LOGL,
			    "Expected raw data buffer, but reached end "
			    "early.");
			goto err;
		}
	}
	p += raw_buffer_words;

	/* Skip maw sample buffer. */
	if (a_sis3316->config.check_level >= CL_PARANOID) {
		if (!MEMORY_CHECK(*a_event_buffer, &p[maw_buffer_words - 1]))
		{
			log_error(LOGL,
			    "Expected maw data buffer, but reached end "
			    "early.");
			goto err;
		}
	}
	p += maw_buffer_words;

	EVENT_BUFFER_ADVANCE(*a_event_buffer, p);
	LOGF(spam)(LOGL, NAME" check_channel_hit }");
	return;
err:
	time_sleep(1);
	sis_3316_dump_data(a_event_buffer->ptr,
	    (uint8_t const *)a_event_buffer->ptr + a_event_buffer->bytes);
	log_die(LOGL, "Corrupt data.");
}

void
sis_3316_get_config(struct Sis3316Module *a_module, struct ConfigBlock
    *a_block)
{
	struct ConfigBlock *g_block, *t_block;
	enum Keyword const true_false[] = {KW_TRUE, KW_FALSE};
	enum Keyword const c_blt_mode[] = {
		KW_BLT_2ESST,
		KW_BLT_2EVME,
		KW_BLT,
		KW_MBLT,
		KW_NOBLT
	};
	enum Keyword const check_level[] = {KW_OFF, KW_SLOPPY, KW_PARANOID};
	int signal_polarity[N_CHANNELS];
	enum Keyword kw_check_level;
	size_t i;

	LOGF(verbose)(LOGL, NAME" get_config {");

	a_module->config.address = config_get_block_param_int32(a_block, 0);

	/* Default value for the maximum number of events in async mode */
	a_module->config.async_max_events = 4000;

	/* channels to read */
	a_module->config.channels_to_read = config_get_bitmask(a_block,
	    KW_CHANNELS_TO_READ, 0, 15);
	LOGF(verbose)(LOGL, "channels_to_read = mask 0x%08x",
	    a_module->config.channels_to_read);
	g_n_channels = 0;
	for (i = 0; i < N_CHANNELS; ++i) {
		if (((a_module->config.channels_to_read >> i) & 1) == 1) {
			++g_n_channels;
		}
	}

	/* use tau correction */
	a_module->config.use_tau_correction = config_get_bitmask(a_block,
	    KW_USE_TAU_CORRECTION, 0, 15);
	LOGF(verbose)(LOGL, "use_tau_correction = mask 0x%08x",
	    a_module->config.use_tau_correction);

	/* use external trigger */
	a_module->config.use_external_trigger = config_get_bitmask(a_block,
	    KW_USE_EXTERNAL_TRIGGER, 0, 15);
	LOGF(verbose)(LOGL, "use_external_trigger = mask 0x%08x",
	    a_module->config.use_external_trigger);

	/* use internal trigger */
	a_module->config.use_internal_trigger = config_get_bitmask(a_block,
	    KW_USE_INTERNAL_TRIGGER, 0, 15);
	LOGF(verbose)(LOGL, "use_internal_trigger = mask 0x%08x",
	    a_module->config.use_internal_trigger);

	/* which channels contribute to TO signal (OR-ed) */
	a_module->config.trigger_output = config_get_bitmask(a_block,
	    KW_TRIGGER_OUTPUT, 0, 15);
	LOGF(verbose)(LOGL, "trigger_output = mask 0x%08x",
	    a_module->config.trigger_output);

	/* use dual threshold trigger */
	a_module->config.use_dual_threshold = config_get_bitmask(a_block,
	    KW_USE_DUAL_THRESHOLD, 0, 15);
	LOGF(verbose)(LOGL, "use_dual_threshold = mask 0x%08x",
	    a_module->config.use_dual_threshold);

	/* TODO: internal gate */

	/* external gate (region of interest) */
	a_module->config.use_external_gate = config_get_bitmask(a_block,
	    KW_USE_EXTERNAL_GATE, 0, 15);
	LOGF(verbose)(LOGL, "use_external_gate = mask 0x%08x.",
	    a_module->config.use_external_gate);

	/* external veto (deny accepting triggers) */
	a_module->config.use_external_veto = config_get_bitmask(a_block,
	    KW_USE_EXTERNAL_VETO, 0, 15);
	LOGF(verbose)(LOGL, "use_external_veto = mask 0x%08x.",
	    a_module->config.use_external_veto);

	/* use discard data */
	a_module->config.discard_data = config_get_bitmask(a_block,
	    KW_DISCARD_DATA, 0, 15);
	LOGF(verbose)(LOGL, "discard_data = mask 0x%08x",
	    a_module->config.discard_data);

	/* discard threshold */
	a_module->config.discard_threshold = config_get_int32(a_block, KW_DISCARD_THRESHOLD,
	    CONFIG_UNIT_NONE, INT_MIN, INT_MAX);
	LOGF(verbose)(LOGL, "discard_threshold = %d.",
	    a_module->config.discard_threshold);

	/* DAC offset */
	CONFIG_GET_INT_ARRAY(a_module->config.dac_offset, a_block,
	    KW_OFFSET, CONFIG_UNIT_NONE, -0x8000, 0x8000);
	for (i = 0; i < LENGTH(a_module->config.dac_offset); ++i) {
		LOGF(verbose)(LOGL, "dac_offset[%d] = %d.", (int)i,
		    a_module->config.dac_offset[i]);
	}

	/* energy pickup */
	CONFIG_GET_INT_ARRAY(a_module->config.energy_pickup, a_block,
	    KW_ENERGY_PICKUP, CONFIG_UNIT_NONE, 0, 2048);
	for (i = 0; i < LENGTH(a_module->config.energy_pickup); ++i) {
		LOGF(verbose)(LOGL, "energy_pickup[%d] = %d.", (int)i,
		    a_module->config.energy_pickup[i]);
	}

	/* internal trigger delay (use in conjunction with external gate) */
	CONFIG_GET_INT_ARRAY(a_module->config.internal_trigger_delay, a_block,
	    KW_INTERNAL_TRIGGER_DELAY, CONFIG_UNIT_NS, 0, 2044);
	for (i = 0; i < LENGTH(a_module->config.internal_trigger_delay); ++i) {
		LOGF(verbose)(LOGL, "internal_trigger_delay[%d] = %d ns.",
		    (int)i, a_module->config.internal_trigger_delay[i]);
	}

	/* Readout enable/disable */
	a_module->config.do_readout = config_get_boolean(a_block,
	    KW_READOUT);
	LOGF(verbose)(LOGL, "do readout = %s.",
	    a_module->config.do_readout ? "yes" : "no");

	/* Enable user counter */
	a_module->config.use_user_counter = config_get_boolean(a_block,
	    KW_USE_USER_COUNTER);
	LOGF(verbose)(LOGL, "use user counter = %s.",
	    a_module->config.use_user_counter ? "yes" : "no");

	/* Enable ADC dithering (improve linearity) */
	a_module->config.use_dithering = config_get_boolean(a_block,
	    KW_USE_DITHERING);
	LOGF(verbose)(LOGL, "use ADC dithering = %s.",
	    a_module->config.use_dithering ? "yes" : "no");

	/* Automatic switching */
	a_module->config.use_auto_bank_switch = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_AUTO_BANK_SWITCH, true_false);
	LOGF(verbose)(LOGL, "use auto bank switch = %s.",
	    a_module->config.use_auto_bank_switch ? "yes" : "no");

	/* use rataclock */
	a_module->config.use_rataclock = config_get_boolean(a_block,
	    KW_USE_RATACLOCK);
	LOGF(verbose)(LOGL, "use_rataclock = %s.",
	    a_module->config.use_rataclock ? "yes" : "no");

	/* use rataclock timestamp */
	a_module->config.use_rataclock_timestamp = config_get_boolean(a_block,
	    KW_USE_RATACLOCK_TIMESTAMP);
	LOGF(verbose)(LOGL, "use_rataclock_timestamp = %s.",
	    a_module->config.use_rataclock_timestamp ? "yes" : "no");

	/* use clock from VXS connector */
	a_module->config.use_clock_from_vxs = config_get_boolean(a_block,
	    KW_USE_CLOCK_FROM_VXS);
	LOGF(verbose)(LOGL, "use_clock_from_vxs = %s.",
	    a_module->config.use_clock_from_vxs ? "yes" : "no");

	/* rataclock receive delay */
	a_module->config.rataclock_receive_delay = config_get_int32(a_block,
	    KW_RATACLOCK_RECEIVE_DELAY, CONFIG_UNIT_NS, 0, 65550);
	LOGF(verbose)(LOGL, "rataclock recv delay [common] = %d.",
	    a_module->config.rataclock_receive_delay);

	/* rataclock clock period */
	a_module->config.rataclock_clk_period = config_get_int32(a_block,
	    KW_RATACLOCK_CLK_PERIOD, CONFIG_UNIT_NS, 0, 1023);
	LOGF(verbose)(LOGL, "rataclock clk_period [common] = %d.",
	    a_module->config.rataclock_clk_period);

	/* rataclock clock period */
	a_module->config.rataclock_pulse_period_clk = config_get_int32(a_block,
	    KW_RATACLOCK_PULSE_PERIOD_CLK, CONFIG_UNIT_NONE, 0, 63);
	LOGF(verbose)(LOGL, "rataclock pulse_period_clk [common] = %d.",
	    a_module->config.rataclock_pulse_period_clk);

	/* rataclock use auto edge */
	a_module->config.rataclock_use_auto_edge = config_get_boolean(a_block,
	    KW_RATACLOCK_USE_AUTO_EDGE);
	LOGF(verbose)(LOGL, "rataclock use auto_edge = %s.",
	    a_module->config.rataclock_use_auto_edge ? "yes" : "no");

	/* rataclock expect edge */
	a_module->config.rataclock_expect_edge = config_get_int32(a_block,
	    KW_RATACLOCK_EXPECT_EDGE, CONFIG_UNIT_NONE, 0, 3);
	LOGF(verbose)(LOGL, "rataclock expect_edge [common] = %d.",
	    a_module->config.rataclock_expect_edge);

	/* Derive run mode from internal trigger setting */
	if (a_module->config.use_internal_trigger != 0) {
		a_module->config.run_mode = RM_ASYNC;
	} else {
		a_module->config.run_mode = RM_SYNC;
	}
	if (a_module->config.run_mode == RM_ASYNC
	    && a_module->config.use_external_gate != 0) {
		a_module->config.run_mode = RM_ASYNC_EXTERNAL_GATE;
	}
	if (a_module->config.run_mode == RM_ASYNC
	    && a_module->config.use_auto_bank_switch) {
		a_module->config.run_mode = RM_ASYNC_AUTO_BANK_SWITCH;
	}
	LOGF(verbose)(LOGL, "Run mode = %s %s", (a_module->config.run_mode ==
	    RM_SYNC) ?  "SYNC" : "ASYNC",
		(a_module->config.run_mode == RM_ASYNC_AUTO_BANK_SWITCH)
		? "AUTO_BANK_SWITCH" :
		(a_module->config.run_mode == RM_ASYNC_EXTERNAL_GATE)
		? "EXTERNAL_GATE" : "(simple)");

	/* Termination */
	a_module->config.use_termination = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_TERMINATION, true_false);
	LOGF(verbose)(LOGL, "use termination = %s.",
	    a_module->config.use_termination ? "yes" : "no");

	/* average mode settings */
	CONFIG_GET_INT_ARRAY(a_module->config.average_mode, a_block,
	    KW_AVERAGE_MODE, CONFIG_UNIT_NONE, 0, 256);
	CONFIG_GET_INT_ARRAY(a_module->config.average_pretrigger, a_block,
	    KW_AVERAGE_PRETRIGGER, CONFIG_UNIT_NONE, 0,4094);
	CONFIG_GET_INT_ARRAY(a_module->config.average_length, a_block,
	    KW_AVERAGE_LENGTH, CONFIG_UNIT_NONE, 0, 65534);

	for (i = 0; i < LENGTH(a_module->config.average_mode); ++i) {
		int mode_bits = 0;
		switch (a_module->config.average_mode[i]) {
			case 0:
				break;
			case 4:
				mode_bits = 1; break;
			case 8:
				mode_bits = 2; break;
			case 16:
				mode_bits = 3; break;
			case 32:
				mode_bits = 4; break;
			case 64:
				mode_bits = 5; break;
			case 128:
				mode_bits = 6; break;
			case 256:
				mode_bits = 7; break;
			default:
				log_error(LOGL,
				    NAME"Unsupported averaging mode (%d).",
				    a_module->config.average_mode[i]);
				abort();
		}
		LOGF(verbose)(LOGL, "average mode[%" PRIz "]       = %d samples (0x%2x)",
		    i, a_module->config.average_mode[i], mode_bits);
		a_module->config.average_mode[i] = mode_bits;
		LOGF(verbose)(LOGL, "average pretrigger[%" PRIz "] = %d samples",
		    i, a_module->config.average_pretrigger[i]);
		LOGF(verbose)(LOGL, "average length[%" PRIz "]     = %d samples",
		    i, a_module->config.average_length[i]);
		a_module->config.average_length[i] -=
		    a_module->config.average_length[i] % 2;
	}

	/* check level */
	kw_check_level = CONFIG_GET_KEYWORD(a_block,
		        KW_CHECK_LEVEL, check_level);
	if (kw_check_level == KW_SLOPPY) {
		a_module->config.check_level = CL_SLOPPY;
	} else if (kw_check_level == KW_PARANOID) {
		a_module->config.check_level = CL_PARANOID;
	} else {
		a_module->config.check_level = CL_OFF;
	}
        LOGF(verbose)(LOGL, "use check_level = %s.",
	    (a_module->config.check_level == CL_PARANOID) ?
	    "paranoid" : "sloppy");

	/* blt_mode */
	a_module->config.blt_mode = CONFIG_GET_KEYWORD(a_block, KW_BLT_MODE,
	    c_blt_mode);
	LOGF(verbose)(LOGL, "Using BLT mode = %d.", a_module->config.blt_mode);

	/* Frequency of internal sampling clock */
	a_module->config.clk_freq = config_get_int32(a_block, KW_CLK_FREQ,
	    CONFIG_UNIT_MHZ, 25, 250);
	LOGF(verbose)(LOGL, "Sampling clock frequency = %d MHz.",
	    a_module->config.clk_freq);

	switch (a_module->config.clk_freq)
	{
	case 25:
	case 125:
	case 250:
		break;
	default:
		log_die(LOGL, "Unsupported sampling clock frequency set.");
	}

	/* Frequency of external clock provided on input CI */
	a_module->config.ext_clk_freq = config_get_int32(a_block,
	    KW_EXT_CLK_FREQ, CONFIG_UNIT_MHZ, 0, 250);
	LOGF(verbose)(LOGL, "External clock frequency = %d MHz.",
	    a_module->config.ext_clk_freq);

	switch (a_module->config.ext_clk_freq)
	{
	case 0:
	case 10:
	case 12:
	case 20:
	case 50:
	case 250:
		break;
	default:
		log_die(LOGL, "Unsupported external clock frequency set.");
	}

	/* tap delay fine tune */
	a_module->config.tap_delay_fine_tune = config_get_int32(a_block,
	    KW_TAP_DELAY_FINE_TUNE, CONFIG_UNIT_NONE, -32, 32);

	/* Accumulator 2 */
	a_module->config.use_accumulator2 = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_ACCUMULATOR2, true_false);
	LOGF(verbose)(LOGL, "use accumulator 2 = %s.",
	    a_module->config.use_accumulator2 ? "yes" : "no");

	/* Accumulator 6 */
	a_module->config.use_accumulator6 = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_ACCUMULATOR6, true_false);
	LOGF(verbose)(LOGL, "use accumulator 6 = %s.",
	    a_module->config.use_accumulator6 ? "yes" : "no");

	/* Read 3 MAW words for better timing */
	a_module->config.use_maw3 = KW_TRUE == CONFIG_GET_KEYWORD(a_block,
	    KW_USE_MAW3, true_false);
	LOGF(verbose)(LOGL, "Use MAW data = %s.",
	    a_module->config.use_maw3 ? "yes" : "no");

	/* Max E */
	a_module->config.use_maxe = KW_TRUE == CONFIG_GET_KEYWORD(a_block,
	    KW_USE_MAXE, true_false);
	LOGF(verbose)(LOGL, "Use max energy = %s.",
	    a_module->config.use_maxe ? "yes" : "no");

	/* Peak / Charge */
	a_module->config.use_peak_charge = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_PEAK_CHARGE, true_false);
	LOGF(verbose)(LOGL, "use_peak_charge = %s.",
	    a_module->config.use_peak_charge ? "yes" : "no");

	/* Write traces raw */
	a_module->config.write_traces_raw = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_WRITE_TRACES_RAW, true_false);
	LOGF(verbose)(LOGL, "write traces raw = %s.",
	    a_module->config.write_traces_raw ? "yes" : "no");

	/* Write traces MAW */
	a_module->config.write_traces_maw = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_WRITE_TRACES_MAW, true_false);
	LOGF(verbose)(LOGL, "write traces maw = %s.",
	    a_module->config.write_traces_maw ? "yes" : "no");

	/* Write traces MAW energy */
	a_module->config.write_traces_maw_energy = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_WRITE_TRACES_MAW_ENERGY, true_false);
	LOGF(verbose)(LOGL, "write traces maw energy = %s.",
	    a_module->config.write_traces_maw_energy ? "yes" : "no");

	/* Write histograms */
	a_module->config.write_histograms = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_WRITE_HISTOGRAMS, true_false);
	LOGF(verbose)(LOGL, "write histograms = %s.",
	    a_module->config.write_histograms ? "yes" : "no");

	/* FP bus master */
	a_module->config.is_fpbus_master = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_IS_FPBUS_MASTER, true_false);
	LOGF(verbose)(LOGL, "is FP bus master = %s.",
	    a_module->config.is_fpbus_master ? "yes" : "no");

	/* Baseline Average */
	a_module->config.baseline_average = config_get_int32(a_block,
	    KW_BASELINE_AVERAGE, CONFIG_UNIT_NONE, 32, 256);
	switch (a_module->config.baseline_average) {
	case 32:
	case 64:
	case 128:
	case 256:
		break;
	default:
		log_die(LOGL, NAME" Unsupported baseline average %d",
		    a_module->config.baseline_average);
	}
	LOGF(verbose)(LOGL, "Baseline average = %d ns.",
	    a_module->config.baseline_average);

	/* Baseline delay */
	a_module->config.baseline_delay = config_get_int32(a_block,
	    KW_BASELINE_DELAY, CONFIG_UNIT_NONE, 32, 256);
	LOGF(verbose)(LOGL, "Baseline delay = %d ns.",
	    a_module->config.baseline_delay);

	/* range */
	CONFIG_GET_INT_ARRAY(a_module->config.range, a_block, KW_RANGE,
	    CONFIG_UNIT_V, 1, 10);
        for (i = 0; i < LENGTH(a_module->config.range); ++i) {
            LOGF(verbose)(LOGL, "Range[%d] = %d V.",
                    (int)i, a_module->config.range[i]);
        }

	/* Decaytime */
	CONFIG_GET_INT_ARRAY(a_module->config.signal_decaytime, a_block,
	    KW_SIGNAL_DECAYTIME, CONFIG_UNIT_NS, 0, 2000000);
	for (i = 0; i < LENGTH(a_module->config.signal_decaytime); ++i) {
		LOGF(verbose)(LOGL, "Decaytime[%d] = %d.",
		    (int)i, a_module->config.signal_decaytime[i]);
	}

	/* Signal Polarity */
	CONFIG_GET_INT_ARRAY(signal_polarity, a_block, KW_SIGNAL_POLARITY,
	    CONFIG_UNIT_NONE, -1, 1);
	for (i = 0; i < N_CHANNELS; ++i) {
		a_module->config.invert_signal[i] = (signal_polarity[i] ==
		    -1) ? 1 : 0;
	}
	LOGF(verbose)(LOGL, "Invert signal = %s.",
	    (a_module->config.invert_signal[0] == 1) ? "yes" : "no");

	/* Risetime */
	CONFIG_GET_INT_ARRAY(a_module->config.signal_risetime, a_block,
	    KW_SIGNAL_RISETIME, CONFIG_UNIT_NS, 0, 10000);
	for (i = 0; i < LENGTH(a_module->config.signal_risetime); ++i) {
		LOGF(verbose)(LOGL, "Signal risetime = %d ns.",
		    a_module->config.signal_risetime[i]);
	}

	/* CFD trigger */
	a_module->config.use_cfd_trigger = KW_TRUE ==
	    CONFIG_GET_KEYWORD(a_block, KW_USE_CFD_TRIGGER, true_false);
	LOGF(verbose)(LOGL, "use_cfd_trigger = %s.",
	    a_module->config.use_cfd_trigger ? "yes" : "no");

	/* Thresholds */
	CONFIG_GET_INT_ARRAY(a_module->config.threshold_mV, a_block,
	    KW_THRESHOLD, CONFIG_UNIT_MV, 0, 5000);
	for (i = 0; i < LENGTH(a_module->config.threshold_mV); ++i) {
		LOGF(verbose)(LOGL, "threshold_mV[%d] = %d / 100.",
		    (int)i, a_module->config.threshold_mV[i]);
	}

	/* High energy threshold (upper threshold) */
	CONFIG_GET_INT_ARRAY(a_module->config.threshold_high_e_mV, a_block,
	    KW_THRESHOLD_HIGH_E, CONFIG_UNIT_MV, 0, 200000);
	for (i = 0; i < LENGTH(a_module->config.threshold_high_e_mV); ++i) {
		LOGF(verbose)(LOGL, "threshold_high_e_mV[%d] = %d / 100.",
		    (int)i, a_module->config.threshold_high_e_mV[i]);
	}

	/* Peak time for trigger filter */
	CONFIG_GET_INT_ARRAY(a_module->config.peak, a_block,
	    KW_PEAK, CONFIG_UNIT_NONE, 2, 510);
	for (i = 0; i < LENGTH(a_module->config.peak); ++i) {
		LOGF(verbose)(LOGL, "peak[%d] = %d.",
		    (int)i, a_module->config.peak[i]);
	}

	/* Gap time for trigger filter */
	CONFIG_GET_INT_ARRAY(a_module->config.gap, a_block,
	    KW_GAP, CONFIG_UNIT_NONE, 0, 510);
	for (i = 0; i < LENGTH(a_module->config.gap); ++i) {
		LOGF(verbose)(LOGL, "gap[%d] = %d.",
		    (int)i, a_module->config.gap[i]);
	}

	/* Peak time for energy filter */
	CONFIG_GET_INT_ARRAY(a_module->config.peak_e, a_block,
	    KW_PEAK_E, CONFIG_UNIT_NONE, 0, 2044);
	for (i = 0; i < LENGTH(a_module->config.peak_e); ++i) {
		LOGF(verbose)(LOGL, "peak_e[%d] = %d.",
		    (int)i, a_module->config.peak_e[i]);
	}

	/* Gap time for energy filter */
	CONFIG_GET_INT_ARRAY(a_module->config.gap_e, a_block,
	    KW_GAP_E, CONFIG_UNIT_NONE, 0, 2044);
	for (i = 0; i < LENGTH(a_module->config.gap_e); ++i) {
		LOGF(verbose)(LOGL, "gap_e[%d] = %d.",
		    (int)i, a_module->config.gap_e[i]);
	}

	/* Get TRACES settings */
	t_block = config_get_block_by_param_keyword(a_block,
	    KW_TRACES, KW_RAW);
	if (t_block == 0) {
		log_die(LOGL, "Missing TRACES(raw) block");
	}

	/* Sample length RAW */
	CONFIG_GET_INT_ARRAY(a_module->config.sample_length, t_block,
	    KW_SAMPLE_LENGTH, CONFIG_UNIT_NS, 0,
	    (1000 / a_module->config.clk_freq) * 33554430);
	for (i = 0; i < LENGTH(a_module->config.sample_length); ++i) {
		/*
		 * Raw sample length is given in NS so recompute
		 * taking into account the sampling frequency
		 */
		LOGF(verbose)(LOGL, "sample_length[%d] = %d ns.", (int)i,
		    a_module->config.sample_length[i]);
                LOGF(verbose)(LOGL, "                 = %d ms.",
                    a_module->config.sample_length[i] / 1000 / 1000);
		a_module->config.sample_length[i] =
		    a_module->config.sample_length[i] *
		    a_module->config.clk_freq / 1000;
		LOGF(verbose)(LOGL, "sample_length[%d] = %d samples.", (int)i,
		    a_module->config.sample_length[i]);
                if (a_module->config.sample_length[i] > 65534) {
                        LOGF(verbose)(LOGL, "Using extended raw buffer.");
                        if (a_module->config.run_mode ==
                            RM_ASYNC_AUTO_BANK_SWITCH) {
                                log_die(LOGL, "Long traces not supported in "
                                    "auto bank switch mode.");
                        }
                }
	}

	/* Get TRACES settings */
	t_block = config_get_block_by_param_keyword(a_block,
	    KW_TRACES, KW_MAW);
	if (t_block == 0) {
		log_die(LOGL, "Missing TRACES(maw) block");
	}

	/* Sample length MAW */
	CONFIG_GET_INT_ARRAY(a_module->config.sample_length_maw,
	    t_block, KW_SAMPLE_LENGTH, CONFIG_UNIT_NONE, 0, 2048);
	for (i = 0; i < LENGTH(a_module->config.sample_length_maw); ++i) {
		LOGF(verbose)(LOGL,
		    "sample_length_maw[%d] = %d.",
		    (int)i, a_module->config.sample_length_maw[i]);
	}

	/* Pretrigger MAW */
	CONFIG_GET_INT_ARRAY(a_module->config.pretrigger_delay_maw,
	    t_block, KW_PRETRIGGER_DELAY, CONFIG_UNIT_NONE, 0, 1022);
	for (i = 0; i < LENGTH(a_module->config.pretrigger_delay_maw); ++i) {
		LOGF(verbose)(LOGL,
		    "pretrigger_delay_maw[%d] = %d.",
		    (int)i, a_module->config.pretrigger_delay_maw[i]);
	}

	/* Get TRACES settings */
	t_block = config_get_block_by_param_keyword(a_block,
	    KW_TRACES, KW_MAW_ENERGY);
	if (t_block == 0) {
		log_die(LOGL, "Missing TRACES(maw_energy) block");
	}

	/* Sample length MAW */
	CONFIG_GET_INT_ARRAY(a_module->config.sample_length_maw_e,
	    t_block, KW_SAMPLE_LENGTH, CONFIG_UNIT_NONE, 0, 2048);
	for (i = 0; i < LENGTH(a_module->config.sample_length_maw_e); ++i) {
		LOGF(verbose)(LOGL,
		    "sample_length_maw_e[%d] = %d.", (int)i,
		    a_module->config.sample_length_maw_e[i]);
	}

	/* Pretrigger MAW */
	CONFIG_GET_INT_ARRAY(a_module->config.pretrigger_delay_maw_e,
	    t_block, KW_PRETRIGGER_DELAY, CONFIG_UNIT_NONE, 0, 1022);
	for (i = 0; i < LENGTH(a_module->config.pretrigger_delay_maw_e); ++i) {
		LOGF(verbose)(LOGL,
		    "pretrigger_delay_maw_e[%d] = %d.", (int)i,
		    a_module->config.pretrigger_delay_maw_e[i]);
	}

	/* Gate blocks */
	g_block = config_get_block(a_block, KW_GATE);
	for (i = 0; i < N_GATES; ++i) {
		if (NULL == g_block) {
			log_error(LOGL,
			    NAME" Missing GATE(%d) block.", (int)i);
			abort();
		}
		/* Time after trigger */
		a_module->config.gate[i].delay =
		    config_get_int32(g_block, KW_TIME_AFTER_TRIGGER,
			CONFIG_UNIT_NS, 0, 1024);
		LOGF(verbose)(LOGL,
		    "gate[%d].delay = %d ns.", (int)i,
		    a_module->config.gate[i].delay);
		/* Gate width */
		a_module->config.gate[i].width =
		    config_get_int32(g_block, KW_WIDTH,
			CONFIG_UNIT_NS, 0, 1024);
		LOGF(verbose)(LOGL,
		    "gate[%d].width = %d ns.", (int)i,
		    a_module->config.gate[i].width);

		g_block = config_get_block_next(g_block, KW_GATE);
	}

	LOGF(verbose)(LOGL, NAME" get_config }");
}

/*
 * DAC offset:
 * Set the offset, such that the baseline of the signal
 * is at ~20% of the full scale range
 */
void
sis_3316_calculate_dac_offsets(struct Sis3316Module *a_module)
{
	int i;

	for (i = 0; i < N_CHANNELS; ++i) {
		double percent;
		int max, min, diff;
		int adc = i >> 2;

		/* From manual 6.8 */
		if (a_module->config.range[adc] == 5) {
			max = 65536;
			min = 0;
		} else {
			max = 52000;
			min = 13000;
		}
		diff = max - min;

		/*
		 * For negative signals, offset is shifted up.
		 * For positive signals, offset is shifted down.
		 */
		if (a_module->config.invert_signal[i] == 1) {
			percent = 0.8;
		} else {
			percent = 0.2;
		}

		a_module->config.dac_offset[i] =
		    (percent * (double)diff) + min;

		LOGF(verbose)(LOGL, "dac_offset[%d] = %d", (int)i,
		    a_module->config.dac_offset[i]);
	}
}

void
sis_3316_calculate_settings(struct Sis3316Module *a_module)
{
	int i;
	int risetime_s[N_CHANNELS];
	int decaytime_s[N_CHANNELS];
	int signal_length_s[N_CHANNELS];
	int samples_per_ns;
	int full_range;
	unsigned int min_trigger_window_len;

	LOGF(verbose)(LOGL, NAME" calculate_settings {");

	/* Values in samples via clock frequency */
	samples_per_ns = 1000 / a_module->config.clk_freq;
	LOGF(verbose)(LOGL, "samples_per_ns = %d",
	    samples_per_ns);

	/*
	 * Convert signal decay/rise times from ns to samples
	 * and compute total signal length
	 */
	for (i = 0; i < N_CHANNELS; ++i) {
		decaytime_s[i] =
		    a_module->config.signal_decaytime[i] / samples_per_ns;
		risetime_s[i] =
		    a_module->config.signal_risetime[i] / samples_per_ns;

		/* Signal should be decayed to ~0 within 5 decay times */
		signal_length_s[i] = risetime_s[i] + decaytime_s[i] * 5;

		LOGF(verbose)(LOGL,
		    "risetime_s[%d] = %d, decaytime_s[%d] = %d",
		    i, risetime_s[i], i, decaytime_s[i]);
		LOGF(verbose)(LOGL,
		    "signal_length_s[%d] = %d", i, signal_length_s[i]);
	}

	/* Convert gate settings from ns to samples */
	for (i = 0; i < N_GATES; ++i) {
		a_module->config.gate[i].delay /= samples_per_ns;
		a_module->config.gate[i].width /= samples_per_ns;
	}

	/* Pileup length is the full signal length */
	for (i = 0; i < N_ADCS; ++i) {
		a_module->config.pileup_length[i] = signal_length_s[i*4];
		a_module->config.re_pileup_length[i] = signal_length_s[i*4];
		LOGF(verbose)(LOGL, "adc %d, pileup_len = %d, "
		    "repileup_len = %d",
		    i, a_module->config.pileup_length[i],
		    a_module->config.re_pileup_length[i]);
	}

	/* Set RAW sample length to 0, if write_traces_raw == 0 */
	if (a_module->config.write_traces_raw == 0) {
		for (i = 0; i < N_ADCS; ++i) {
			a_module->config.sample_length[i] = 0;
		}
	}

	/* Use only sample_length_maw and pretrigger_delay_maw from now on */
	if (a_module->config.write_traces_maw == 0) {
		for (i = 0; i < N_ADCS; ++i) {
			a_module->config.sample_length_maw[i] = 0;
		}
	}
	if (a_module->config.write_traces_maw_energy == 1) {
		for (i = 0; i < N_ADCS; ++i) {
			a_module->config.sample_length_maw[i] =
			    a_module->config.sample_length_maw_e[i];
			a_module->config.pretrigger_delay_maw[i] =
			    a_module->config.pretrigger_delay_maw_e[i];
		}
	} else {
		for (i = 0; i < N_ADCS; ++i) {
			a_module->config.sample_length_maw_e[i] = 0;
		}
	}

	/*
	 * Pretrigger length is the rise time + baseline region
	 * and a constant value
	 */
	for (i = 0; i < N_ADCS; ++i) {
		a_module->config.pretrigger_delay[i] =
		    risetime_s[i*4] + a_module->config.baseline_average + 10;
	}

	/*
	 * TODO: This does the automatic calculation, but one can now also
	 * set the offsets in the config. Disable this for now.
	 */
	/*
	sis_3316_calculate_dac_offsets(a_module);
	*/

	/*
	 * Trigger gate window length
	 * Add a constant 500
	 */
	min_trigger_window_len = a_module->config.peak_e[0]
		+ a_module->config.gap_e[0]
		+ a_module->config.pretrigger_delay[0]
		+ a_module->config.internal_trigger_delay[0]
		+ 50;
	if (a_module->config.sample_length[0]
	    + a_module->config.sample_length_maw[0] < min_trigger_window_len) {
		a_module->config.trigger_gate_window_length =
		    min_trigger_window_len;
	} else {
		a_module->config.trigger_gate_window_length =
		    a_module->config.sample_length[0]
		    + a_module->config.sample_length_maw[0]
		    + 50;
	}

	LOGF(verbose)(LOGL, "trigger_gate_window_length = %d",
	    a_module->config.trigger_gate_window_length);

	/*
	 * tau_factor, tau_table
	 * these are taken from the table
	 */
	for (i = 0; i < N_CHANNELS; ++i) {
		size_t j;
		struct tau_entry* table = NULL;

		/* LaBr detectors don't need tau correction */
		if ((a_module->config.use_tau_correction & (1 << i)) == 0) {
			a_module->config.tau_table[i] = 0;
			a_module->config.tau_factor[i] = 0;
			LOGF(verbose)(LOGL, "Setting tau table to 0.");
			continue;
		}

		if (a_module->config.bit_depth == BD_16BIT) {
			if (a_module->config.clk_freq == 125) {
				table = tau_16bit_125MHz;
			}
		} else if (a_module->config.bit_depth == BD_14BIT) {
			if (a_module->config.clk_freq == 250) {
				table = tau_14bit_250MHz;
			} else if (a_module->config.clk_freq == 125) {
				table = tau_14bit_125MHz;
			}
		}
		if (table == NULL) {
			log_die(LOGL, "No matching tau table for "
			    "combination of bit depth and clock frequency.");
		}

		/* tau_entries are sorted with ascending .value */
		a_module->config.tau_table[i] = 0xff;
		a_module->config.tau_factor[i] = 0xff;
		for (j = 0; j < TAU_TABLE_SIZE; ++j) {
			if (table[j].value*1000 >
			    a_module->config.signal_decaytime[i]) {
				a_module->config.tau_table[i] = table[j].table;
				a_module->config.tau_factor[i] = table[j].index;
				LOGF(verbose)(LOGL, "ch %d, "
				    "table %d, index %d, value %f",
				    i, table[j].table, table[j].index,
				    table[j].value);
				break;
			}
		}
		if (a_module->config.tau_factor[i] == 0xff) {
			log_die(LOGL, "ch %d, didn't find appropriate"
			    " tau factor", (int)i);
		}
	}

	/*
	 * extra filter
	 * We just put this to the maximum for now.
	 */
	for (i = 0; i < N_CHANNELS; ++i) {
		a_module->config.extra_filter[i] = 3;
	}

	/*
	 * Full range depends on bit resolution
	 * Let's assume that we will only make proper use of
	 * 75% of the dynamic range in 16 bit mode
	 */
	if (a_module->config.bit_depth == BD_14BIT) {
		full_range = (2 << 14);
	} else {
		full_range = 0.75 * (2 << 16);
	}

	/*
	 * histogram_divider
	 * should be set to ensure that the full range fits
	 * into the 64 k bins of the histogram
	 */
	for (i = 0; i < N_CHANNELS; ++i) {
		a_module->config.histogram_divider[i] =
		    (full_range * a_module->config.peak[i]) / 64000;
		if (a_module->config.histogram_divider[i] < 1)
			a_module->config.histogram_divider[i] = 1;
		LOGF(verbose)(LOGL, "histogram_divider[%d] = %d",
		    i, a_module->config.histogram_divider[i]);
	}

	/*
	 * threshold from mV
	 * takes into account the peaking time
	 */
	for (i = 0; i < N_CHANNELS; ++i) {
		unsigned adc_i;
		int counts_per_V;

		adc_i = i / N_CH_PER_ADC;
		counts_per_V = (1 << 16) / a_module->config.range[adc_i];
                LOGF(verbose)(LOGL, "%u: range = %d -> counts_per_V = %d",
                        adc_i, a_module->config.range[adc_i], counts_per_V);
		a_module->config.threshold[i] = 0x08000000 +
		    (counts_per_V
		        * a_module->config.threshold_mV[i]
		        * a_module->config.peak[i])
		    / 100000;
		a_module->config.threshold_high_e[i] = 0x08000000 +
		    (counts_per_V
		        * a_module->config.threshold_high_e_mV[i]
		        * a_module->config.peak[i])
		    / 100000;
		LOGF(info)(LOGL, "threshold[%d] = %d mV -> 0x%08x",
		    i, a_module->config.threshold_mV[i],
		    a_module->config.threshold[i]);
	}

	LOGF(verbose)(LOGL, NAME" calculate_settings }");
}
