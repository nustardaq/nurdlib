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

#include <module/mesytec_mdpp32qdc/mesytec_mdpp32qdc.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp32qdc/internal.h>
#include <module/mesytec_mdpp32qdc/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/time.h>

#define NAME "Mesytec Mdpp32 QDC"

MODULE_PROTOTYPES(mesytec_mdpp32qdc);
static int	mesytec_mdpp32qdc_post_init(struct Crate *, struct Module *)
	FUNC_RETURNS;

uint32_t
mesytec_mdpp32qdc_check_empty(struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mxdc32_check_empty(&mdpp32qdc->mdpp.mxdc32);
}

struct Module *
mesytec_mdpp32qdc_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(mdpp32qdc);
	/* 48640/64k buffer, 255 word maximum event size, p. 9 & 24. */
	mdpp32qdc->mdpp.mxdc32.module.event_max = 48640 / 255;
	mesytec_mdpp_create_(a_block, &mdpp32qdc->mdpp, KW_MESYTEC_MDPP32QDC);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)mdpp32qdc;
}

void
mesytec_mdpp32qdc_deinit(struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	mesytec_mdpp_deinit(&mdpp32qdc->mdpp);
}

void
mesytec_mdpp32qdc_destroy(struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	mesytec_mdpp_destroy(&mdpp32qdc->mdpp);
}

struct Map *
mesytec_mdpp32qdc_get_map(struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mdpp_get_map(&mdpp32qdc->mdpp);
}

void
mesytec_mdpp32qdc_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	mesytec_mdpp_get_signature(a_array, a_num);
}

int
mesytec_mdpp32qdc_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;
	double init_sleep;
	unsigned i, output_format;

	LOGF(verbose)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	mesytec_mdpp_init_fast(a_crate, &mdpp32qdc->mdpp);

	/* Quadwise settings */
	CONFIG_GET_INT_ARRAY(mdpp32qdc->config.sig_width,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_SIGNAL_WIDTH, CONFIG_UNIT_NS, 0, 1023);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.in_amp,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_AMPLITUDE, CONFIG_UNIT_MV, 0, 65535);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.jump_range,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_JUMPER_RANGE, CONFIG_UNIT_MV, 0, 65535);
	mdpp32qdc->config.qdc_jump = config_get_bitmask(
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_QDC_JUMPER, 0, 7);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.int_long,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_INTEGRATION_LONG, CONFIG_UNIT_NS, 0, 511);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.int_short,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_INTEGRATION_SHORT, CONFIG_UNIT_NS, 0, 127);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.long_gain_corr,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_CORRECTION_LONG_GAIN, CONFIG_UNIT_NONE, 0, 4);
	CONFIG_GET_DOUBLE_ARRAY(mdpp32qdc->config.short_gain_corr,
	    mdpp32qdc->mdpp.mxdc32.module.config,
	    KW_CORRECTION_SHORT_GAIN, CONFIG_UNIT_NONE, 0, 4);

	/* Channel settings. */
	init_sleep = mesytec_mxdc32_sleep_get(&mdpp32qdc->mdpp.mxdc32);
	for (i = 0; MDPP32QDC_QD_N > i; ++i) {
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, select_chan_pair,
		    i);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, signal_width,
		    mdpp32qdc->config.sig_width[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, input_amplitude,
		    mdpp32qdc->config.in_amp[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, jumper_range,
		    mdpp32qdc->config.jump_range[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, qdc_jumper,
		    (mdpp32qdc->config.qdc_jump >> i) & 1);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, integration_long,
		    mdpp32qdc->config.int_long[i]/12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, integration_short,
			  mdpp32qdc->config.int_short[i]/12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map,
		    long_gain_correction,
		    mdpp32qdc->config.long_gain_corr[i] * 1024);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map,
		    short_gain_correction,
		    mdpp32qdc->config.short_gain_corr[i] * 1024);
		time_sleep(init_sleep);
	}

	/* Resolution */
	mdpp32qdc->config.adc_resolution = config_get_int32(
	    mdpp32qdc->mdpp.mxdc32.module.config, KW_RESOLUTION,
	    CONFIG_UNIT_NONE, 0, 4);
	MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, adc_resolution,
	    mdpp32qdc->config.adc_resolution);

	MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, fifo_reset, 0);

	/* Do "proper" (safe?) BLT. */
	MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, fast_mblt, 0);

	output_format = config_get_int32(
	    mdpp32qdc->mdpp.mxdc32.module.config, KW_OUTPUT_FORMAT,
	    CONFIG_UNIT_NONE, 0, 3);
	/* Streaming mode? */
/*	if (crate_free_running_get(a_crate)) {
		output_format |= 4;
	}*/
	MAP_WRITE(mdpp32qdc->mdpp.mxdc32.sicy_map, output_format, output_format);

	LOGF(verbose)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mdpp32qdc_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	mesytec_mdpp_init_slow(a_crate, &mdpp32qdc->mdpp);

	return 1;
}

void
mesytec_mdpp32qdc_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mdpp32qdc_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mdpp_parse_data(a_crate, &mdpp32qdc->mdpp,
	    a_event_buffer, a_do_pedestals, 1);
}

int
mesytec_mdpp32qdc_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mdpp_post_init(&mdpp32qdc->mdpp);
}

uint32_t
mesytec_mdpp32qdc_readout(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mdpp_readout(a_crate, &mdpp32qdc->mdpp,
	    a_event_buffer, 1);
}

uint32_t
mesytec_mdpp32qdc_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32qdcModule *mdpp32qdc;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP32QDC, mdpp32qdc, a_module);
	return mesytec_mdpp_readout_dt(&mdpp32qdc->mdpp);
}

void
mesytec_mdpp32qdc_setup_(void)
{
	MODULE_SETUP(mesytec_mdpp32qdc, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(mesytec_mdpp32qdc, post_init);
}
