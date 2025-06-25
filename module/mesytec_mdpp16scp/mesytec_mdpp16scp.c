/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2021, 2023-2025
 * Bastian Löher
 * Michael Munch
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

#include <module/mesytec_mdpp16scp/mesytec_mdpp16scp.h>
#include <math.h>
#include <module/map/map.h>
#include <module/mesytec_mdpp16scp/internal.h>
#include <module/mesytec_mdpp16scp/offsets.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/time.h>

#define NAME "Mesytec Mdpp16 SCP"

MODULE_PROTOTYPES(mesytec_mdpp16scp);
static int	mesytec_mdpp16scp_post_init(struct Crate *, struct Module *)
	FUNC_RETURNS;
static uint32_t	mesytec_mdpp16scp_readout_shadow(struct Crate *, struct Module
    *, struct EventBuffer *) FUNC_RETURNS;
static void	mesytec_mdpp16scp_use_pedestals(struct Module *);
static void	mesytec_mdpp16scp_zero_suppress(struct Module *, int);
#if NCONF_mMAP_bCMVLC
static void	mesytec_mdpp16scp_cmvlc_init(struct Module *,
    struct cmvlc_stackcmdbuf *, int);
static uint32_t mesytec_mdpp16scp_cmvlc_fetch_dt(struct Module *,
    const uint32_t *, uint32_t, uint32_t *);
static uint32_t mesytec_mdpp16scp_cmvlc_fetch(struct Crate *, struct
    Module *, struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *);
#endif

uint32_t
mesytec_mdpp16scp_check_empty(struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mxdc32_check_empty(&mdpp16scp->mdpp.mxdc32);
}

struct Module *
mesytec_mdpp16scp_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	MODULE_CREATE(mdpp16scp);
	/* 48640/64k buffer, 255 word maximum event size, p. 9 & 24. */
	mdpp16scp->mdpp.mxdc32.module.event_max = 48640 / 255;
	mesytec_mdpp_create_(a_block, &mdpp16scp->mdpp, KW_MESYTEC_MDPP16SCP);
	MODULE_CREATE_PEDESTALS(mdpp16scp, mesytec_mdpp16scp, MDPP16SCP_CH_N);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)mdpp16scp;
}

void
mesytec_mdpp16scp_deinit(struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_deinit(&mdpp16scp->mdpp);
}

void
mesytec_mdpp16scp_destroy(struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_destroy(&mdpp16scp->mdpp);
}

struct Map *
mesytec_mdpp16scp_get_map(struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_get_map(&mdpp16scp->mdpp);
}

void
mesytec_mdpp16scp_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	mesytec_mdpp_get_signature(a_array, a_num);
}

int
mesytec_mdpp16scp_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;
	double init_sleep;
	unsigned i;

	LOGF(info)(LOGL, NAME" init_fast {");

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_init_fast(a_crate, &mdpp16scp->mdpp);

	/* Channel independent settings */
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.pz,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_POLE_ZERO, CONFIG_UNIT_NS, 0, 800e3);
	for (i = 0; i < LENGTH(mdpp16scp->config.pz); ++i) {
		int32_t *pz;

		pz = &mdpp16scp->config.pz[i];
		if (0 == *pz) {
			*pz = 65535;
		} else if (*pz < 0.8e3) {
			*pz = 64;
		} else {
			*pz /= 12.5;
		}
		LOGF(verbose)(LOGL, "Threshold[%u] = %5u Pz[%u] = %u ns.",
		    i, mdpp16scp->mdpp.config.threshold[i], i,
		    (int32_t) (*pz * 12.5));
	}

	/* Pairwise settings */
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.tf,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_DIFFERENTIATION, CONFIG_UNIT_NS, 25, 1587);
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.gain,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_GAIN, CONFIG_UNIT_NONE, 100, 20000);
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.shaping,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_SHAPING_TIME, CONFIG_UNIT_NS, 100, 25000);
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.blr,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_BLR, CONFIG_UNIT_NONE, 0, 2);
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.rise_time,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_SIGNAL_RISETIME, CONFIG_UNIT_NS, 0, 1580);
	for (i = 0; i < MDPP16SCP_PR_N; ++i) {
		LOGF(verbose)(LOGL, "Input gain[%d] = %.2f (= %d/100).", i,
		    mdpp16scp->config.gain[i] / 100.,
		    mdpp16scp->config.gain[i]);
	}

	/* Channel settings. */
	init_sleep = mesytec_mxdc32_sleep_get(&mdpp16scp->mdpp.mxdc32);
	for (i = 0; MDPP16SCP_PR_N > i; ++i) {
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, select_chan_pair,
		    i);
		/* Must sleep after setting pair! */
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, tf_int_diff,
		    mdpp16scp->config.tf[i] / 12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, gain,
		    mdpp16scp->config.gain[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, shaping_time,
		    mdpp16scp->config.shaping[i] / 12.5);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, blr,
		    mdpp16scp->config.blr[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, signal_rise_time,
		    mdpp16scp->config.rise_time[i]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, pz0,
		    mdpp16scp->config.pz[i * 2]);
		time_sleep(init_sleep);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, pz1,
		    mdpp16scp->config.pz[i * 2 + 1]);
		time_sleep(init_sleep);
	}

	/* Resolution */
	CONFIG_GET_INT_ARRAY(mdpp16scp->config.resolution,
	    mdpp16scp->mdpp.mxdc32.module.config,
	    KW_RESOLUTION, CONFIG_UNIT_NONE, 0, 5);
	if (4 < mdpp16scp->config.resolution[RT_ADC]) {
		log_die(LOGL, "ADC resolution in range [0..4]");
	}
	{
		int res_div, val_max;

		res_div = 1 << (10 - mdpp16scp->config.resolution[RT_TDC]);
		val_max = mdpp16scp->mdpp.config.gate_width * res_div / 25;
		if (val_max > 0xffff) {
			int gate_max;

			gate_max = 0xffff * 25 / res_div;
			log_die(LOGL, "Window width=0x%08x (%d ns) "
			    "does not work with TDC resolution=%d "
			    "(max gate width=%d ns, "
			    "or change TDC resolution)!",
			    mdpp16scp->mdpp.config.gate_width,
			    mdpp16scp->mdpp.config.gate_width * 25 / 16,
			    mdpp16scp->config.resolution[RT_TDC],
			    gate_max);
		}
	}
	MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, adc_resolution,
	    mdpp16scp->config.resolution[RT_ADC]);
	MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, tdc_resolution,
	    mdpp16scp->config.resolution[RT_TDC]);

	MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, fifo_reset, 0);

	/* Do "proper" (safe?) BLT. */
	MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, fast_mblt, 0);

	/* Streaming mode? */
	if (crate_free_running_get(a_crate)) {
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, output_format, 8);
		MAP_WRITE(mdpp16scp->mdpp.mxdc32.sicy_map, marking_type, 3);
	}

	LOGF(info)(LOGL, NAME" init_fast }");
	return 1;
}

int
mesytec_mdpp16scp_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_init_slow(a_crate, &mdpp16scp->mdpp);

	return 1;
}

void
mesytec_mdpp16scp_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
mesytec_mdpp16scp_parse_data(struct Crate *a_crate, struct Module *a_module,
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_parse_data(a_crate, &mdpp16scp->mdpp,
	    a_event_buffer, a_do_pedestals, 1);
}

int
mesytec_mdpp16scp_post_init(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_post_init(&mdpp16scp->mdpp);
}

uint32_t
mesytec_mdpp16scp_readout(struct Crate *a_crate, struct Module *a_module,
    struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_readout(a_crate, &mdpp16scp->mdpp,
	    a_event_buffer, 1);
}

uint32_t
mesytec_mdpp16scp_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_readout_dt(a_crate, &mdpp16scp->mdpp);
}

uint32_t
mesytec_mdpp16scp_readout_shadow(struct Crate *a_crate, struct Module
    *a_module, struct EventBuffer *a_event_buffer)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	(void)a_crate;
	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_readout_shadow(&mdpp16scp->mdpp,
	    a_event_buffer);
}

#if NCONF_mMAP_bCMVLC
void
mesytec_mdpp16scp_cmvlc_init(struct Module *a_module,
    struct cmvlc_stackcmdbuf *a_stack, int a_dt)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_cmvlc_init(&mdpp16scp->mdpp, a_stack, a_dt);
}

uint32_t
mesytec_mdpp16scp_cmvlc_fetch_dt(struct Module *a_module,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_cmvlc_fetch_dt(&mdpp16scp->mdpp,
	    a_in_buffer, a_in_remain, a_in_used);
}

uint32_t
mesytec_mdpp16scp_cmvlc_fetch(struct Crate *a_crate,
    struct Module *a_module, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	return mesytec_mdpp_cmvlc_fetch(a_crate, &mdpp16scp->mdpp,
	    a_event_buffer, a_in_buffer, a_in_remain, a_in_used);
}
#endif

void
mesytec_mdpp16scp_setup_(void)
{
	MODULE_SETUP(mesytec_mdpp16scp, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, post_init);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, readout_shadow);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, use_pedestals);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, zero_suppress);
#if NCONF_mMAP_bCMVLC
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, cmvlc_init);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, cmvlc_fetch_dt);
	MODULE_CALLBACK_BIND(mesytec_mdpp16scp, cmvlc_fetch);
#endif
}

void
mesytec_mdpp16scp_use_pedestals(struct Module *a_module)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_use_pedestals(&mdpp16scp->mdpp);
}

void
mesytec_mdpp16scp_zero_suppress(struct Module *a_module, int a_yes)
{
	struct MesytecMdpp16scpModule *mdpp16scp;

	MODULE_CAST(KW_MESYTEC_MDPP16SCP, mdpp16scp, a_module);
	mesytec_mdpp_zero_suppress(&mdpp16scp->mdpp, a_yes);
}
