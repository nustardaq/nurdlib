/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Manuel Xarepe
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

#include <module/gsi_kilom/gsi_kilom.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_kilom/internal.h>
#include <nurdlib/gsi_kilom.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/time.h>

#define NAME "Gsi Kilom"

#if !NCONF_mGSI_PEX_bNO

#define REG_KILOM_TEMP 0x20005c

#define KILOM_INIT_RD(a0, a1, label) do {\
	if (!gsi_pex_slave_read(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "read("#a0", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)
#define KILOM_INIT_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "write("#a0", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)

MODULE_PROTOTYPES(gsi_kilom);
static struct ConfigBlock	*gsi_kilom_get_submodule_config(struct Module
    *, unsigned);
static void			gsi_kilom_sub_module_pack(struct Module *,
    struct PackerList *);
static int			threshold_set(struct ConfigBlock *, struct
    GsiPex *, unsigned, unsigned, int32_t, uint16_t const *) FUNC_RETURNS;

uint32_t
gsi_kilom_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_kilom_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiKilomModule *kilom;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(kilom);
	gsi_ctdc_proto_create(a_block, &kilom->ctdcp, KW_GSI_KILOM_CARD);
	kilom->ctdcp.threshold_set = threshold_set;
	LOGF(verbose)(LOGL, NAME" create }");
	return &kilom->ctdcp.module;
}

void
gsi_kilom_deinit(struct Module *a_module)
{
	(void)a_module;
}

void
gsi_kilom_destroy(struct Module *a_module)
{
	struct GsiKilomModule *kilom;

	LOGF(verbose)(LOGL, NAME" destroy {");
	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	gsi_ctdc_proto_destroy(&kilom->ctdcp);
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
gsi_kilom_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_kilom_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

struct ConfigBlock *
gsi_kilom_get_submodule_config(struct Module *a_module, unsigned a_i)
{
	struct GsiKilomModule *kilom;

	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	return gsi_ctdc_proto_get_submodule_config(&kilom->ctdcp, a_i);
}

int
gsi_kilom_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	unsigned clock_switch[] = {2, 1};
	struct GsiKilomModule *kilom;
	struct GsiPex *pex;
	unsigned card_i, sfp_i;
	int ret;

	ret = 0;
	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	if (!gsi_ctdc_proto_init_fast(a_crate, &kilom->ctdcp, clock_switch)) {
		goto gsi_kilom_init_fast_done;
	}
	/* Dark mode, all LED:s off. */
	pex = crate_gsi_pex_get(a_crate);
	sfp_i = kilom->ctdcp.sfp_i;
	for (card_i = 0; card_i < kilom->ctdcp.card_num; ++card_i) {
		int is_dark;

		is_dark = config_get_boolean(a_module->config, KW_DARK);
		LOGF(info)(LOGL, "Dark mode = %u", is_dark);
		KILOM_INIT_WR(0x200114, is_dark, gsi_kilom_init_fast_done);
	}
	ret = 1;
gsi_kilom_init_fast_done:
	LOGF(info)(LOGL, NAME" init_fast }");
	return ret;
}

int
gsi_kilom_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiKilomModule *kilom;

	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	gsi_ctdc_proto_init_slow(a_crate, &kilom->ctdcp);
	return 1;
}

void
gsi_kilom_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_kilom_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiKilomModule *kilom;

	(void)a_do_pedestals;
	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	return gsi_ctdc_proto_parse_data(a_crate, &kilom->ctdcp,
	    a_event_buffer);
}

uint32_t
gsi_kilom_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiKilomModule *kilom;

	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	return gsi_ctdc_proto_readout(a_crate, &kilom->ctdcp, a_event_buffer);
}

uint32_t
gsi_kilom_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x).",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_kilom_setup_(void)
{
	MODULE_SETUP(gsi_kilom, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(gsi_kilom, get_submodule_config);
	MODULE_CALLBACK_BIND(gsi_kilom, sub_module_pack);
}

void
gsi_kilom_sub_module_pack(struct Module *a_module, struct PackerList *a_list)
{
	struct GsiKilomModule *kilom;

	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	gsi_ctdc_proto_sub_module_pack(&kilom->ctdcp, a_list);
}

uint32_t
gsi_kilom_temp_read(struct Crate *a_crate, struct EventBuffer *a_event_buffer)
{
	struct GsiPex *pex;
	struct GsiKilomCrate *kilom_crate;
	uint32_t *p32, *pnum;
	unsigned sfp_i;
	uint32_t ret = 0;

	LOGF(spam)(LOGL, NAME" kilom_temp_read {");
	pex = crate_gsi_pex_get(a_crate);
	kilom_crate = crate_get_kilom_crate(a_crate);
	pnum = p32 = a_event_buffer->ptr;
	++p32;
	for (sfp_i = 0; sfp_i < LENGTH(kilom_crate->sfp); ++sfp_i) {
		struct GsiKilomModule *module;

		module = kilom_crate->sfp[sfp_i];
		if (NULL != module) {
			struct GsiCTDCProtoModule *ctdcp;
			unsigned card_i;

			ctdcp = &module->ctdcp;
			for (card_i = 0; card_i < ctdcp->card_num; ++card_i) {
				uint32_t temp = 0;

				if (!MEMORY_CHECK(*a_event_buffer, p32)) {
					ret =
					    CRATE_READOUT_FAIL_DATA_TOO_MUCH;
					goto gsi_kilom_temp_read_abort;
				}
				KILOM_INIT_RD(REG_KILOM_TEMP, &temp,
				    gsi_kilom_temp_read_fail);
				goto gsi_kilom_temp_read_ok;
gsi_kilom_temp_read_fail:
				ret = CRATE_READOUT_FAIL_ERROR_DRIVER;
gsi_kilom_temp_read_ok:
				*p32++ = temp;
			}
		}
	}
gsi_kilom_temp_read_abort:
	*pnum = p32 - pnum - 1;
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
	LOGF(spam)(LOGL, NAME" kilom_temp_read }");
	return ret;
}

int
threshold_set(struct ConfigBlock *a_block, struct GsiPex *a_pex, unsigned
    a_sfp_i, unsigned a_card_i, int32_t a_offset, uint16_t const
    *a_threshold_array)
{
	struct GsiPex *pex = a_pex;
	unsigned sfp_i = a_sfp_i;
	unsigned card_i = a_card_i;
	unsigned ch_i;
	int ret;

	ret = 0;
	LOGF(verbose)(LOGL, NAME" threshold_set {");
	if (a_offset < -0x200 || 0x200 < a_offset) {
		log_error(LOGL, "-0x200 < offset=0x%08x < 0x200 failed.",
		    a_offset);
	}
	for (ch_i = 0; ch_i < 128; ++ch_i) {
		uint32_t value;
		int32_t i32;
		unsigned chip_i, cch_i;

		i32 = a_threshold_array[ch_i] + a_offset;
		if (i32 < 0 || 0x3ff < i32) {
			log_error(LOGL, "0 < thr[%u]=0x%08x < 0x3ff failed.",
			    ch_i, i32);
		}

		chip_i = ch_i / 8;
		cch_i = ch_i & 7;
		value = chip_i << 16 |
		    4 << 13 |
		    cch_i << 10 |
		    i32;
		KILOM_INIT_WR(REG_PADI_SPI_DATA, value, threshold_set_fail);
		/* Magic. */
		KILOM_INIT_WR(REG_PADI_SPI_CTRL, 1, threshold_set_fail);
		KILOM_INIT_WR(REG_PADI_SPI_CTRL, 0, threshold_set_fail);
		/* More magic. */
		time_sleep(500e-6);
	}
	if (config_get_boolean(a_block, KW_INVERT_SIGNAL)) {
		/* One-time polarity switch, needed for Kilom1. */
		KILOM_INIT_WR(0x200100, 0x200, threshold_set_fail);
	}
	ret = 1;
threshold_set_fail:
	LOGF(verbose)(LOGL, NAME" threshold_set(ret=%d) }", ret);
	return ret;
}

#else

struct Module *
gsi_kilom_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	(void)a_crate;
	(void)a_block;
	log_die(LOGL, NAME" not supported in this build/platform.");
}

void
gsi_kilom_setup_(void)
{
}

#endif

void
gsi_kilom_crate_add(struct GsiKilomCrate *a_crate, struct Module *a_module)
{
	struct GsiKilomModule *kilom;
	size_t sfp_i;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	MODULE_CAST(KW_GSI_KILOM, kilom, a_module);
	sfp_i = kilom->ctdcp.sfp_i;
	if (NULL != a_crate->sfp[sfp_i]) {
		log_die(LOGL, "Multiple Kilom chains on SFP=%"PRIz"!",
		    sfp_i);
	}
	a_crate->sfp[sfp_i] = kilom;
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_kilom_crate_create(struct GsiKilomCrate *a_crate)
{
	LOGF(verbose)(LOGL, NAME" crate_create {");
	ZERO(*a_crate);
	LOGF(verbose)(LOGL, NAME" crate_create }");
}

int
gsi_kilom_crate_init_slow(struct GsiKilomCrate *a_crate)
{
	size_t i;

	LOGF(verbose)(LOGL, NAME" crate_init_slow {");
	for (i = 0; i < LENGTH(a_crate->sfp); ++i) {
		struct GsiKilomModule *kilom;

		kilom = a_crate->sfp[i];
		if (NULL != kilom) {
			kilom->ctdcp.module.event_counter.value = 0;
		}
	}
	LOGF(verbose)(LOGL, NAME" crate_init_slow }");
	return 1;
}
