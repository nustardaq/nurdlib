/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019-2021, 2023-2024
 * Oliver Papst
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

#include <module/mesytec_mdpp16qdc/mesytec_mdpp16qdc.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp16qdc/internal.h>
#include <module/mesytec_mdpp16qdc/offsets.h>
#include <nurdlib/config.h>

#define NAME "Mesytec Mdpp16 QDC"

MODULE_PROTOTYPES(mesytec_mdpp16qdc);
/*static void	mesytec_mdpp16qdc_use_pedestals(struct Module *);
static void	mesytec_mdpp16qdc_zero_suppress(struct Module *, int);*/

uint32_t
mesytec_mdpp16qdc_check_empty(struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	return mesytec_mxdc32_check_empty(&mdpp16qdc->mdpp.mxdc32);
}

struct Module *
mesytec_mdpp16qdc_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(mdpp16qdc);
	/* See SCP/RCP version. */
	mdpp16qdc->mdpp.mxdc32.module.event_max = 48640 / 255;
	mesytec_mdpp_create_(a_block, &mdpp16qdc->mdpp, KW_MESYTEC_MDPP16QDC);
	MODULE_CREATE_PEDESTALS(mdpp16qdc, mesytec_mdpp16qdc, MDPP16QDC_CH_N);

	LOGF(verbose)(LOGL, NAME" create }");

	return (struct Module *)mdpp16qdc;
}

void
mesytec_mdpp16qdc_deinit(struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	mesytec_mdpp_deinit(&mdpp16qdc->mdpp);
}

void
mesytec_mdpp16qdc_destroy(struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	mesytec_mdpp_destroy(&mdpp16qdc->mdpp);
}

struct Map *
mesytec_mdpp16qdc_get_map(struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *m;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, m, a_module);
	return mesytec_mdpp_get_map(&m->mdpp);
}

void
mesytec_mdpp16qdc_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	mesytec_mdpp_get_signature(a_array, a_num);
}

int
mesytec_mdpp16qdc_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;
	unsigned i;

	(void)a_crate;
	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	mesytec_mdpp_init_fast(a_crate, &mdpp16qdc->mdpp);

	/* Channel independent settings */
	for (i = 0; i < LENGTH(mdpp16qdc->mdpp.config.threshold); ++i) {
		LOGF(verbose)(LOGL, "Threshold[%u]=%u",
		    i, mdpp16qdc->mdpp.config.threshold[i]);
	}

	/* Pairwise settings */
	CONFIG_GET_INT_ARRAY(mdpp16qdc->config.signal_width,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_SIGNAL_WIDTH, CONFIG_UNIT_NS, 0, 1023);
	CONFIG_GET_INT_ARRAY(mdpp16qdc->config.amplitude,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_AMPLITUDE, CONFIG_UNIT_MV, 0, 65535);
	CONFIG_GET_DOUBLE_ARRAY(mdpp16qdc->config.jumper_range,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_JUMPER_RANGE, CONFIG_UNIT_V, 0, 65.535);
	CONFIG_GET_INT_ARRAY(mdpp16qdc->config.qdc_jumper,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_QDC_JUMPER, CONFIG_UNIT_NONE, 0, 1);
	CONFIG_GET_INT_ARRAY(mdpp16qdc->config.integration_long,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    /*
	     * The limits are 12.5*[2,506]=[25,6325], and since we divide by
	     * 12.5 and truncate later, increment limits by one.
	     */
	    KW_INTEGRATION_LONG, CONFIG_UNIT_NS, 26, 6326);
	CONFIG_GET_INT_ARRAY(mdpp16qdc->config.integration_short,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    /* Limits here are 12.5*[1,127]=[12.5,1587.5]. */
	    KW_INTEGRATION_SHORT, CONFIG_UNIT_NS, 13, 1588);
	CONFIG_GET_DOUBLE_ARRAY(mdpp16qdc->config.correction_long_gain,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_CORRECTION_LONG_GAIN, CONFIG_UNIT_NONE, 0.25, 4.);
	CONFIG_GET_DOUBLE_ARRAY(mdpp16qdc->config.correction_tf_gain,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_CORRECTION_TIMING_FILTER_GAIN, CONFIG_UNIT_NONE, 0.25, 4.);
	CONFIG_GET_DOUBLE_ARRAY(mdpp16qdc->config.correction_short_gain,
	    mdpp16qdc->mdpp.mxdc32.module.config,
	    KW_CORRECTION_SHORT_GAIN, CONFIG_UNIT_NONE, 0.25, 4.);

	/* Channel settings. */
	/* TODO: Shouldn't these have sleeps? */
	for (i = 0; MDPP16QDC_PR_N > i; ++i) {
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, select_chan_pair,
		    i);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, signal_width,
		    mdpp16qdc->config.signal_width[i]);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, input_amplitude,
		    mdpp16qdc->config.amplitude[i]);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, jumper_range,
		    mdpp16qdc->config.jumper_range[i] * 1000);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, qdc_jumper,
		    mdpp16qdc->config.qdc_jumper[i]);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, integration_long,
		    mdpp16qdc->config.integration_long[i] / 12.5);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map,
		    integration_short,
		    mdpp16qdc->config.integration_short[i] / 12.5);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map,
		    long_gain_correction,
		    mdpp16qdc->config.correction_long_gain[i] * 1024);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map,
		    tf_gain_correction,
		    mdpp16qdc->config.correction_tf_gain[i] * 1024);
		MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map,
		    short_gain_correction,
		    mdpp16qdc->config.correction_short_gain[i] * 1024);
	}

	MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, fifo_reset, 0);
	MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, readout_reset, 0);
	MAP_WRITE(mdpp16qdc->mdpp.mxdc32.sicy_map, reset_ctr_ab, 3);

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mdpp16qdc_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	mesytec_mdpp_init_slow(a_crate, &mdpp16qdc->mdpp);

	return 1;
}

void
mesytec_mdpp16qdc_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

void
mesytec_mdpp16qdc_setup_(void)
{
	MODULE_SETUP(mesytec_mdpp16qdc, 0);
	/*MODULE_CALLBACK_BIND(mesytec_mdpp16qdc, readout_shadow);
	MODULE_CALLBACK_BIND(mesytec_mdpp16qdc, use_pedestals);
	MODULE_CALLBACK_BIND(mesytec_mdpp16qdc, zero_suppress);*/
}

uint32_t
mesytec_mdpp16qdc_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	return mesytec_mdpp_parse_data(a_crate, &mdpp16qdc->mdpp,
	    a_event_buffer, a_do_pedestals, 1);
}

uint32_t
mesytec_mdpp16qdc_readout(struct Crate *a_crate, struct Module * a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	return mesytec_mdpp_readout(a_crate, &mdpp16qdc->mdpp,
	    a_event_buffer, 1);
}

uint32_t
mesytec_mdpp16qdc_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16qdcModule *mdpp16qdc;

	MODULE_CAST(KW_MESYTEC_MDPP16QDC, mdpp16qdc, a_module);
	return mesytec_mdpp_readout_dt(a_crate, &mdpp16qdc->mdpp);
}

/*
void
mesytec_mdpp16qdc_use_pedestals(struct Module *a_module)
{
	mesytec_mdpp_use_pedestals(&((struct MesytecMdpp16qdcModule
		*)a_module)->mdpp);
}

void
mesytec_mdpp16qdc_zero_suppress(struct Module *a_module, int a_yes)
{
	mesytec_mdpp_zero_suppress(&((struct MesytecMdpp16qdcModule
		*)a_module)->mdpp, a_yes);
}
*/
