/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
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

#include <module/mesytec_mdpp32scp/mesytec_mdpp32scp.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp32scp/internal.h>
#include <module/mesytec_mdpp32scp/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/time.h>

#define NAME "Mesytec Mdpp32 SCP"

MODULE_PROTOTYPES(mesytec_mdpp32scp);
static int	mesytec_mdpp32scp_post_init(struct Crate *, struct Module *)
	FUNC_RETURNS;
static uint32_t	mesytec_mdpp32scp_readout_shadow(struct Crate *, struct Module
    *, struct EventBuffer *) FUNC_RETURNS;
static void	mesytec_mdpp32scp_use_pedestals(struct Module *);
static void	mesytec_mdpp32scp_zero_suppress(struct Module *, int);
static void	mesytec_mdpp32scp_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t mesytec_mdpp32scp_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t mesytec_mdpp32scp_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);

uint32_t
mesytec_mdpp32scp_check_empty(struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mxdc32_check_empty(&mdpp32scp->mdpp.mxdc32);
}

struct Module *
mesytec_mdpp32scp_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(mdpp32scp);
	/* 48640/64k buffer, 255 word maximum event size, p. 9 & 24. */
	mdpp32scp->mdpp.mxdc32.module.event_max = 48640 / 255;
	mesytec_mdpp_create_(a_block, &mdpp32scp->mdpp, KW_MESYTEC_MDPP32SCP);
	MODULE_CREATE_PEDESTALS(mdpp32scp, mesytec_mdpp32scp, MDPP32SCP_CH_N);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)mdpp32scp;
}

void
mesytec_mdpp32scp_deinit(struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_deinit(&mdpp32scp->mdpp);
}

void
mesytec_mdpp32scp_destroy(struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_destroy(&mdpp32scp->mdpp);
}

struct Map *
mesytec_mdpp32scp_get_map(struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_get_map(&mdpp32scp->mdpp);
}

void
mesytec_mdpp32scp_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	mesytec_mdpp_get_signature(a_array, a_num);
}

int
mesytec_mdpp32scp_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;
	double init_sleep;
	unsigned i;
	unsigned want_samples;
	int samples_tot;

	want_samples = 0;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_init_fast(a_crate, &mdpp32scp->mdpp);

	/* Channel independent settings */
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.pz,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_POLE_ZERO, CONFIG_UNIT_NS, 0, 800e3);
	for (i = 0; i < LENGTH(mdpp32scp->config.pz); ++i) {
		int32_t *pz;

		pz = &mdpp32scp->config.pz[i];
		if (0 == *pz) {
			*pz = 65535;
		} else if (*pz < 0.8e3) {
			*pz = 64;
		} else {
			*pz /= 12.5;
		}
		LOGF(verbose)(LOGL, "Threshold[%u]=%u\tPz[%u]=%uns.",
			      i, mdpp32scp->mdpp.config.threshold[i], i,
			      (int32_t) (*pz * 12.5));
	}

	/* Pairwise settings */
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.tf,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_DIFFERENTIATION, CONFIG_UNIT_NS, 1*12.5, 125*12.5);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.gain,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_GAIN, CONFIG_UNIT_NONE, 100, 20000);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.shaping,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_SHAPING_TIME, CONFIG_UNIT_NS, 4*12.5, 1999*12.5);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.blr,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_BLR, CONFIG_UNIT_NONE, 0, 2);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.rise_time,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_SIGNAL_RISETIME, CONFIG_UNIT_NS, 0, 127*12.5);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.samples_pre,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_SAMPLES_PRE, CONFIG_UNIT_NONE, 0, 500);
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.samples_tot,
	    mdpp32scp->mdpp.mxdc32.module.config,
	    KW_SAMPLES_TOT, CONFIG_UNIT_NONE, 0, 500);
	for (i = 0; i < MDPP32SCP_PR_N; ++i) {
		LOGF(verbose)(LOGL, "Input gain[%d]=%.2f (=%d/100).", i,
		    mdpp32scp->config.gain[i] / 100.,
		    mdpp32scp->config.gain[i]);
	}

	/* Channel settings. */
	init_sleep = mesytec_mxdc32_sleep_get(&mdpp32scp->mdpp.mxdc32);
	for (i = 0; MDPP32SCP_PR_N > i; ++i) {
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, select_chan_pair,
		    i);
		/* Must sleep after setting pair! */
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, tf_int_diff,
		    mdpp32scp->config.tf[i] / 12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, gain,
		    mdpp32scp->config.gain[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, shaping_time,
		    mdpp32scp->config.shaping[i] / 12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, blr,
		    mdpp32scp->config.blr[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, signal_rise_time,
		    mdpp32scp->config.rise_time[i] / 12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, pz0,
		    mdpp32scp->config.pz[i * 4]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, pz1,
		    mdpp32scp->config.pz[i * 4 + 1]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, pz2,
		    mdpp32scp->config.pz[i * 4 + 2]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, pz3,
		    mdpp32scp->config.pz[i * 4 + 3]);
		time_sleep(init_sleep);

		if (0 != mdpp32scp->config.samples_tot[i])
			want_samples = 1;

		samples_tot = mdpp32scp->config.samples_tot[i];
		/* Round number of samples up to multiple of four
		 * (plus two).  Plus two is to avoid filler words in
		 * streaming mode.  Could be relaxed after lmd2ebye
		 * filter fix.
		 */
		if (0 != samples_tot)
			samples_tot = ((samples_tot - 2 + 3) & ~3) + 2;

		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, pre_samples,
		    mdpp32scp->config.samples_pre[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, tot_samples,
		    samples_tot);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, sample_config,
		    0x0);
		time_sleep(init_sleep);
	}

	/* Resolution */
	CONFIG_GET_INT_ARRAY(mdpp32scp->config.resolution,
	    mdpp32scp->mdpp.mxdc32.module.config, KW_RESOLUTION,
	    CONFIG_UNIT_NONE, 0, 5);
	if (4 < mdpp32scp->config.resolution[RT_ADC]) {
		log_die(LOGL, "ADC resolution in range [0..4]");
	}
	{
		int res_div, val_max;

		res_div = 1 << (10 - mdpp32scp->config.resolution[RT_TDC]);
		val_max = mdpp32scp->mdpp.config.gate_width * res_div / 25;
		if (val_max > 0xffff) {
			int gate_max;

			gate_max = 0xffff * 25 / res_div;
			log_die(LOGL, "Window width=0x%08x (%d ns) "
			    "does not work with TDC resolution=%d "
			    "(max gate width=%d ns, or change TDC resolution)!",
			    mdpp32scp->mdpp.config.gate_width,
			    mdpp32scp->mdpp.config.gate_width * 25 / 16,
			    mdpp32scp->config.resolution[RT_TDC],
			    gate_max);
		}
	}
	MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, adc_resolution,
	    mdpp32scp->config.resolution[RT_ADC]);
	MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, tdc_resolution,
	    mdpp32scp->config.resolution[RT_TDC]);

	MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, fifo_reset, 0);

	/* Do "proper" (safe?) BLT. */
	MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, fast_mblt, 0);


	/* Streaming mode? */
	if (crate_free_running_get(a_crate)) {
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, output_format,
			  8 | (want_samples ? 16 : 0));
		MAP_WRITE(mdpp32scp->mdpp.mxdc32.sicy_map, marking_type, 3);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mdpp32scp_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_init_slow(a_crate, &mdpp32scp->mdpp);

	return 1;
}

void
mesytec_mdpp32scp_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mdpp32scp_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_parse_data(a_crate, &mdpp32scp->mdpp,
	    a_event_buffer, a_do_pedestals, 1);
}

int
mesytec_mdpp32scp_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_post_init(&mdpp32scp->mdpp);
}

uint32_t
mesytec_mdpp32scp_readout(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_readout(a_crate, &mdpp32scp->mdpp,
	    a_event_buffer, 1);
}

uint32_t
mesytec_mdpp32scp_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_readout_dt(a_crate, &mdpp32scp->mdpp);
}

uint32_t
mesytec_mdpp32scp_readout_shadow(struct Crate *a_crate, struct Module
    *a_module, struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_readout_shadow(&mdpp32scp->mdpp, a_event_buffer);
}

void
mesytec_mdpp32scp_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_cmvlc_init(&mdpp32scp->mdpp, a_stack, a_dt);
}

uint32_t
mesytec_mdpp32scp_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_cmvlc_fetch_dt(&mdpp32scp->mdpp,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
mesytec_mdpp32scp_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	return mesytec_mdpp_cmvlc_fetch(a_crate, &mdpp32scp->mdpp,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}

void
mesytec_mdpp32scp_setup_(void)
{
	MODULE_SETUP(mesytec_mdpp32scp, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, post_init);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, readout_shadow);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, use_pedestals);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, zero_suppress);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, cmvlc_init);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(mesytec_mdpp32scp, cmvlc_fetch);
}

void
mesytec_mdpp32scp_use_pedestals(struct Module *a_module)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_use_pedestals(&mdpp32scp->mdpp);
}

void
mesytec_mdpp32scp_zero_suppress(struct Module *a_module, int a_yes)
{
	struct MesytecMdpp32scpModule *mdpp32scp;

	MODULE_CAST(KW_MESYTEC_MDPP32SCP, mdpp32scp, a_module);
	mesytec_mdpp_zero_suppress(&mdpp32scp->mdpp, a_yes);
}
