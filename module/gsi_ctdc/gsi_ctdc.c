/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2024
 * Manuel Xarepe
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

#include <module/gsi_ctdc/gsi_ctdc.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_ctdc/internal.h>
#include <nurdlib/config.h>

#define NAME "Gsi CTDC"

#if NCONF_mGSI_PEX_bYES

MODULE_PROTOTYPES(gsi_ctdc);
static struct ConfigBlock	*gsi_ctdc_get_submodule_config(struct Module
    *, unsigned);
static void			gsi_ctdc_sub_module_pack(struct Module *,
    struct PackerList *);
static int			threshold_set(struct ConfigBlock *, struct
    GsiPex *, unsigned, unsigned, int32_t, uint16_t const *) FUNC_RETURNS;
static int			threshold_set_bjt(struct ConfigBlock const *,
    struct GsiPex *, unsigned, unsigned, int32_t, uint16_t const *)
	FUNC_RETURNS;
static int			threshold_set_padi(struct ConfigBlock const *,
    struct GsiPex *, unsigned, unsigned, int32_t, uint16_t const *)
	FUNC_RETURNS;

uint32_t
gsi_ctdc_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_ctdc_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiCTDCModule *ctdc;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(ctdc);
	gsi_ctdc_proto_create(a_block, &ctdc->ctdcp, KW_GSI_CTDC_CARD);
	ctdc->ctdcp.threshold_set = threshold_set;
	LOGF(verbose)(LOGL, NAME" create }");
	return &ctdc->ctdcp.module;
}

void
gsi_ctdc_deinit(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" deinit.");
}

void
gsi_ctdc_destroy(struct Module *a_module)
{
	struct GsiCTDCModule *ctdc;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	gsi_ctdc_proto_destroy(&ctdc->ctdcp);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
gsi_ctdc_get_map(struct Module *a_module)
{
	(void)a_module;
	LOGF(verbose)(LOGL, NAME" get_map.");
	return NULL;
}

void
gsi_ctdc_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

struct ConfigBlock *
gsi_ctdc_get_submodule_config(struct Module *a_module, unsigned a_i)
{
	struct GsiCTDCModule *ctdc;
	struct ConfigBlock *config;

	LOGF(verbose)(LOGL, NAME" get_submodule_config(%u) {", a_i);
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	config = gsi_ctdc_proto_get_submodule_config(&ctdc->ctdcp, a_i);
	LOGF(verbose)(LOGL, NAME" get_submodule_config }");
	return config;
}

int
gsi_ctdc_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	unsigned clock_switch[] = {2, 1};
	struct GsiCTDCModule *ctdc;
	int ret;

	LOGF(verbose)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	ret = gsi_ctdc_proto_init_fast(a_crate, &ctdc->ctdcp, clock_switch);
	LOGF(verbose)(LOGL, NAME" init_fast }");
	return ret;
}

int
gsi_ctdc_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiCTDCModule *ctdc;

	LOGF(verbose)(LOGL, NAME" init_slow {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	gsi_ctdc_proto_init_slow(a_crate, &ctdc->ctdcp);
	LOGF(verbose)(LOGL, NAME" init_slow }");
	return 1;
}

void
gsi_ctdc_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_ctdc_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiCTDCModule *ctdc;
	int ret;

	(void)a_do_pedestals;
	LOGF(spam)(LOGL, NAME" parse {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	ret = gsi_ctdc_proto_parse_data(a_crate, &ctdc->ctdcp,
	    a_event_buffer);
	LOGF(spam)(LOGL, NAME" parse }");
	return ret;
}

uint32_t
gsi_ctdc_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiCTDCModule *ctdc;
	uint32_t ret;

	LOGF(spam)(LOGL, NAME" readout {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	ret = gsi_ctdc_proto_readout(a_crate, &ctdc->ctdcp, a_event_buffer);
	LOGF(spam)(LOGL, NAME" readout }");
	return ret;
}

uint32_t
gsi_ctdc_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x).",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_ctdc_setup_(void)
{
	MODULE_SETUP(gsi_ctdc, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(gsi_ctdc, get_submodule_config);
	MODULE_CALLBACK_BIND(gsi_ctdc, sub_module_pack);
}

void
gsi_ctdc_sub_module_pack(struct Module *a_module, struct PackerList *a_list)
{
	struct GsiCTDCModule *ctdc;

	LOGF(debug)(LOGL, NAME" sub_module_pack {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	gsi_ctdc_proto_sub_module_pack(&ctdc->ctdcp, a_list);
	LOGF(debug)(LOGL, NAME" sub_module_pack }");
}

#define CTDC_INIT_RD(a0, a1, label) do {\
	if (!gsi_pex_slave_read(a_pex, a_sfp_i, a_card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "read("#a0", "#a1") failed.", a_sfp_i, a_card_i);\
		goto label;\
	}\
} while (0)
#define CTDC_INIT_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(a_pex, a_sfp_i, a_card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "write("#a0", "#a1") failed.", a_sfp_i, a_card_i);\
		goto label;\
	}\
} while (0)

int
threshold_set(struct ConfigBlock *a_block, struct GsiPex *a_pex, unsigned
    a_sfp_i, unsigned a_card_i, int32_t a_offset, uint16_t const
    *a_threshold_array)
{
	enum Keyword const c_frontend[] = {
		KW_BJT,
		KW_PADI
	};
	enum Keyword frontend;

	frontend = CONFIG_GET_KEYWORD(a_block, KW_FRONTEND, c_frontend);

	if (KW_BJT == frontend) {
		if (!threshold_set_bjt(a_block, a_pex, a_sfp_i, a_card_i,
		    a_offset, a_threshold_array)) {
			goto threshold_set_fail;
		}
	} else if (KW_PADI == frontend) {
		if (!threshold_set_padi(a_block, a_pex, a_sfp_i, a_card_i,
		    a_offset, a_threshold_array)) {
			goto threshold_set_fail;
		}
	}
	if (config_get_boolean(a_block, KW_INVERT_SIGNAL)) {
		CTDC_INIT_WR(0x200100, 1, threshold_set_fail);
	}
	return 1;
threshold_set_fail:
	return 0;
}

int
threshold_set_bjt(struct ConfigBlock const *a_block, struct GsiPex *a_pex,
    unsigned a_sfp_i, unsigned a_card_i, int32_t a_offset, uint16_t const
    *a_threshold_array)
{
	unsigned ch_i;
	int ret;

	(void)a_block;
	ret = 0;
	LOGF(verbose)(LOGL, NAME" threshold_set_bjt {");
	if (a_offset < -0x5555 || 0xaaaa < a_offset) {
		log_error(LOGL, "-0x5555 < offset=0x%08x < 0xffff failed.",
		    a_offset);
	}
	for (ch_i = 0; ch_i < 128; ++ch_i) {
		uint32_t thr, on, off;
		int32_t i32;

		i32 = a_threshold_array[ch_i] + a_offset;
		if (i32 < 0 || 0xffff < i32) {
			log_error(LOGL, "0 < thr[%u]=0x%08x < 0xffff failed.",
			    ch_i, i32);
		}

		thr = 0x00800000 |
		    (ch_i & 15) << 16 |
		    i32;
		CTDC_INIT_WR(REG_PADI_SPI_DATA, thr, threshold_set_fail);
		/* Magic. */
		on = (ch_i / 16) << 4 | 1;
		CTDC_INIT_WR(REG_PADI_SPI_CTRL, on, threshold_set_fail);
		off = on & ~1;
		CTDC_INIT_WR(REG_PADI_SPI_CTRL, off, threshold_set_fail);
		/* More magic. */
		usleep(500);
		LOGF(debug)(LOGL, "thr[%u]=0x%08x,0x%08x,0x%08x.",
		    ch_i, thr, on, off);
	}
	ret = 1;
threshold_set_fail:
	LOGF(verbose)(LOGL, NAME" threshold_set_bjt(ret=%d) }", ret);
	return ret;
}

int
threshold_set_padi(struct ConfigBlock const *a_block, struct GsiPex *a_pex,
    unsigned a_sfp_i, unsigned a_card_i, int32_t a_offset, uint16_t const
    *a_threshold_array)
{
	unsigned padi_i;
	int ret;

	(void)a_block;
	ret = 0;
	LOGF(verbose)(LOGL, NAME" threshold_set_padi {");
	if (a_offset < -0x200 || 0x200 < a_offset) {
		log_error(LOGL, "-0x200 < offset=0x%08x < 0x200 failed.",
		    a_offset);
	}
	for (padi_i = 0; padi_i < 8; ++padi_i) {
		unsigned ch_i;

		for (ch_i = 0; ch_i < 8; ++ch_i) {
			int32_t i32;
			unsigned ofs0, ofs8;

			ofs0 = 16 * padi_i + ch_i;
			ofs8 = ofs0 + 8;

			i32 = a_threshold_array[ofs0] + a_offset;
			if (i32 < 0 || 0x3ff < i32) {
				log_error(LOGL,
				    "0 < thr[%u]=0x%08x < 0x3ff failed.",
				    ofs0, i32);
			}
			i32 = a_threshold_array[ofs8] + a_offset;
			if (i32 < 0 || 0x3ff < i32) {
				log_error(LOGL,
				    "0 < thr[%u]=0x%08x < 0x3ff failed.",
				    ofs8, i32);
			}

			/* Write to 2 ch (i & i+8) at once. */
			CTDC_INIT_WR(REG_PADI_SPI_DATA,
			    0x80008000 | /* Enable both PADI:s. */
			    /* Channel i. */
			    (ch_i << 10) |
			    (a_threshold_array[ofs0] + a_offset) |
			    /* Channel i+8. */
			    (ch_i << 26) |
			    ((a_threshold_array[ofs8] + a_offset) << 16),
			    threshold_set_fail);
			/* Magic. */
			CTDC_INIT_WR(REG_PADI_SPI_CTRL, 1 << padi_i,
			    threshold_set_fail);
			/* More magic. */
			usleep(500);
		}
	}
	ret = 1;
threshold_set_fail:
	LOGF(verbose)(LOGL, NAME" threshold_set_padi(ret=%d) }", ret);
	return ret;
}

#else

struct Module *
gsi_ctdc_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	(void)a_crate;
	(void)a_block;
	log_die(LOGL, NAME" not supported in this build/platform.");
}

void
gsi_ctdc_setup_(void)
{
}

#endif

void
gsi_ctdc_crate_add(struct GsiCTDCCrate *a_crate, struct Module *a_module)
{
	struct GsiCTDCModule *ctdc;
	size_t sfp_i;

	LOGF(spam)(LOGL, NAME" crate_add {");
	MODULE_CAST(KW_GSI_CTDC, ctdc, a_module);
	sfp_i = ctdc->ctdcp.sfp_i;
	if (NULL != a_crate->sfp[sfp_i]) {
		log_die(LOGL, "Multiple CTDC chains on SFP=%"PRIz"!",
		    sfp_i);
	}
	a_crate->sfp[sfp_i] = ctdc;
	LOGF(spam)(LOGL, NAME" crate_add }");
}

void
gsi_ctdc_crate_create(struct GsiCTDCCrate *a_crate)
{
	ZERO(*a_crate);
}

int
gsi_ctdc_crate_init_slow(struct GsiCTDCCrate *a_crate)
{
	size_t i;

	for (i = 0; i < LENGTH(a_crate->sfp); ++i) {
		struct GsiCTDCModule *ctdc;

		ctdc = a_crate->sfp[i];
		if (NULL != ctdc) {
			ctdc->ctdcp.module.event_counter.value = 0;
		}
	}
	return 1;
}
