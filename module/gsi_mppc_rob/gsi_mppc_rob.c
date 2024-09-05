/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2022-2024
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

#include <module/gsi_mppc_rob/gsi_mppc_rob.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_mppc_rob/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/time.h>

#define NAME "Gsi Mppc-Rob"

#if NCONF_mGSI_PEX_bYES

#define MPPC_ROB_WR(a0, a1, label) do {\
	if (!gsi_pex_slave_write(pex, sfp_i, card_i, a0, a1)) {\
		log_error(LOGL, NAME" SFP=%u:card=%u "\
		    "write("#a0", "#a1") failed.", sfp_i, card_i);\
		goto label;\
	}\
} while (0)

MODULE_PROTOTYPES(gsi_mppc_rob);
static struct ConfigBlock	*gsi_mppc_rob_get_submodule_config(struct
    Module *, unsigned);
static void			gsi_mppc_rob_sub_module_pack(struct Module *,
    struct PackerList *);
static int			threshold_set(struct ConfigBlock *, struct
    GsiPex *, unsigned, unsigned, int32_t, uint16_t const *) FUNC_RETURNS;

uint32_t
gsi_mppc_rob_check_empty(struct Module *a_module)
{
	(void)a_module;
	return 0;
}

struct Module *
gsi_mppc_rob_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	struct GsiMppcRobModule *mppc_rob;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");
	MODULE_CREATE(mppc_rob);
	gsi_ctdc_proto_create(a_block, &mppc_rob->ctdcp,
	    KW_GSI_MPPC_ROB_CARD);
	mppc_rob->ctdcp.threshold_set = threshold_set;
	LOGF(verbose)(LOGL, NAME" create }");
	return &mppc_rob->ctdcp.module;
}

void
gsi_mppc_rob_deinit(struct Module *a_module)
{
	(void)a_module;
}

void
gsi_mppc_rob_destroy(struct Module *a_module)
{
	struct GsiMppcRobModule *mppc_rob;

	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	gsi_ctdc_proto_destroy(&mppc_rob->ctdcp);
}

struct Map *
gsi_mppc_rob_get_map(struct Module *a_module)
{
	(void)a_module;
	return NULL;
}

void
gsi_mppc_rob_get_signature(struct ModuleSignature const **a_array, size_t
    *a_num)
{
	MODULE_SIGNATURE_BEGIN
	MODULE_SIGNATURE_END(a_array, a_num)
}

struct ConfigBlock *
gsi_mppc_rob_get_submodule_config(struct Module *a_module, unsigned a_i)
{
	struct GsiMppcRobModule *mppc_rob;

	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	return gsi_ctdc_proto_get_submodule_config(&mppc_rob->ctdcp, a_i);
}

int
gsi_mppc_rob_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	unsigned clock_switch[] = {2, 1};
	struct GsiMppcRobModule *mppc_rob;
	int ret;

	ret = 0;
	LOGF(info)(LOGL, NAME" init_fast {");
	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	if (!gsi_ctdc_proto_init_fast(a_crate, &mppc_rob->ctdcp,
	    clock_switch)) {
		goto gsi_mppc_rob_init_fast_done;
	}
	ret = 1;
gsi_mppc_rob_init_fast_done:
	LOGF(info)(LOGL, NAME" init_fast }");
	return ret;
}

int
gsi_mppc_rob_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct GsiMppcRobModule *mppc_rob;

	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	gsi_ctdc_proto_init_slow(a_crate, &mppc_rob->ctdcp);
	return 1;
}

void
gsi_mppc_rob_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
gsi_mppc_rob_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct GsiMppcRobModule *mppc_rob;

	(void)a_do_pedestals;
	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	return gsi_ctdc_proto_parse_data(a_crate, &mppc_rob->ctdcp,
	    a_event_buffer);
}

uint32_t
gsi_mppc_rob_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct GsiMppcRobModule *mppc_rob;

	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	return gsi_ctdc_proto_readout(a_crate, &mppc_rob->ctdcp, a_event_buffer);
}

uint32_t
gsi_mppc_rob_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt(ctr=0x%08x).",
	    a_module->event_counter.value);
	return 0;
}

void
gsi_mppc_rob_setup_(void)
{
	MODULE_SETUP(gsi_mppc_rob, MODULE_FLAG_EARLY_DT);
	MODULE_CALLBACK_BIND(gsi_mppc_rob, get_submodule_config);
	MODULE_CALLBACK_BIND(gsi_mppc_rob, sub_module_pack);
}

void
gsi_mppc_rob_sub_module_pack(struct Module *a_module, struct PackerList
    *a_list)
{
	struct GsiMppcRobModule *mppc_rob;

	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	gsi_ctdc_proto_sub_module_pack(&mppc_rob->ctdcp, a_list);
}

int
threshold_set(struct ConfigBlock *a_block, struct GsiPex *a_pex, unsigned
    a_sfp_i, unsigned a_card_i, int32_t a_offset, uint16_t const
    *a_threshold_array)
{
	char const ch_map2[] = {
		  0,   2,   5,   8,  10,  15,  16,  20,
		 24,  27,   3,   9,  14,  21,  26,   1,
		  7,  12,  18,  23,  28,   6,  13,  19,
		 25,  31,  30,   4,  11,  17,  22,  29,
		 32,  35,  34,  33,  36,  37,  42,  43,
		 46,  47,  52,  53,  58,  59,  38,  40,
		 44,  48,  54,  60,  50,  56,  61,  49,
		 55,  62,  39,  41,  45,  51,  57,  63,
		 64,  68,  75,  82,  83,  79,  73,  85,
		 89,  92,  91,  93,  95,  97,  65,  70,
		 78,  74,  84,  88,  67,  71,  72,  66,
		 81,  76,  69,  77,  87,  80,  86,  90,
		103, 101, 109, 108, 115, 114, 122, 119,
		124, 125, 102, 106, 113, 120, 123,  96,
		100, 105, 111, 117, 127,  94,  99, 104,
		110, 116, 121,  98, 107, 112, 118, 126
	};
	char const ch_map3[] = {
		  0,   2,   1,   4,   3,   6,   5,   8,
		  7,  11,   9,  13,  10,  15,  12,  17,
		 14,  19,  16,  20,  18,  22,  21,  25,
		 24,  27,  23,  29,  26,  31,  28,  35,
		 30,  33,  32,  37,  34,  39,  36,  41,
		 38,  43,  40,  45,  42,  47,  44,  49,
		 46,  51,  48,  53,  50,  55,  52,  57,
		 54,  59,  56,  62,  58,  63,  60,  68,
		 61,  66,  64,  69,  65,  81,  67,  77,
		 75,  82,  70,  72,  71,  76,  83,  79,
		 78,  80,  73,  85,  74,  87,  84,  86,
		 88,  92,  89,  93,  90,  91,  95,  96,
		 94,  98,  97,  99, 100, 101, 102, 103,
		105, 104, 106, 107, 108, 109, 111, 110,
		113, 112, 114, 116, 115, 118, 117, 119,
		120, 121, 122, 127, 123, 126, 124, 125
	};
	char const *ch_map;
	struct GsiPex *pex = a_pex;
	unsigned sfp_i = a_sfp_i;
	unsigned card_i = a_card_i;
	unsigned ch_i;
	unsigned version;
	int ret;

	ret = 0;
	LOGF(verbose)(LOGL, NAME" threshold_set {");

	version = config_get_int32(a_block, KW_VERSION, CONFIG_UNIT_NONE, 0,
	    4);
	switch (version) {
	case 2: ch_map = ch_map2; break;
	case 3: ch_map = ch_map3; break;
	default:
		log_die(LOGL, "MPPC-ROB version %u invalid!", version);
		break;
	}

	for (ch_i = 0; ch_i < 128; ++ch_i) {
		uint32_t value;
		int32_t i32;
		unsigned mapped_ch, sat_ch, subreg;

		i32 = a_threshold_array[ch_i] + a_offset;
		if (i32 < 0 || 0xffff < i32) {
			log_error(LOGL, "0 < thr[%u]=0x%08x < 0xffff failed.",
			    ch_i, i32);
		}

		mapped_ch = ch_map[ch_i];
		sat_ch = mapped_ch >> 4;
		subreg = mapped_ch & 15;
		value =
		    0 << 24 /* Register. */ |
		    8 << 20 /* Write. */ |
		    subreg << 16 /* Sub-register. */ |
		    i32;
		MPPC_ROB_WR(REG_PADI_SPI_DATA, value, threshold_set_fail);
		/* Magic. */
		value = sat_ch << 4;
		MPPC_ROB_WR(REG_PADI_SPI_CTRL, value | 1, threshold_set_fail);
		MPPC_ROB_WR(REG_PADI_SPI_CTRL, value | 0, threshold_set_fail);
		/* More magic. */
		time_sleep(500e-6);
	}
	if (config_get_boolean(a_block, KW_INVERT_SIGNAL)) {
		MPPC_ROB_WR(0x200100, 1, threshold_set_fail);
	} else {
		MPPC_ROB_WR(0x200100, 0, threshold_set_fail);
	}
	ret = 1;
threshold_set_fail:
	LOGF(verbose)(LOGL, NAME" threshold_set(ret=%d) }", ret);
	return ret;
}

#else

struct Module *
gsi_mppc_rob_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	(void)a_crate;
	(void)a_block;
	log_die(LOGL, NAME" not supported in this build/platform.");
}

void
gsi_mppc_rob_setup_(void)
{
}

#endif

void
gsi_mppc_rob_crate_add(struct GsiMppcRobCrate *a_crate, struct Module
    *a_module)
{
	struct GsiMppcRobModule *mppc_rob;
	size_t sfp_i;

	LOGF(verbose)(LOGL, NAME" crate_add {");
	MODULE_CAST(KW_GSI_MPPC_ROB, mppc_rob, a_module);
	sfp_i = mppc_rob->ctdcp.sfp_i;
	if (NULL != a_crate->sfp[sfp_i]) {
		log_die(LOGL, "Multiple MppcRob chains on SFP=%"PRIz"!",
		    sfp_i);
	}
	a_crate->sfp[sfp_i] = mppc_rob;
	LOGF(verbose)(LOGL, NAME" crate_add }");
}

void
gsi_mppc_rob_crate_create(struct GsiMppcRobCrate *a_crate)
{
	ZERO(*a_crate);
}

int
gsi_mppc_rob_crate_init_slow(struct GsiMppcRobCrate *a_crate)
{
	size_t i;

	for (i = 0; i < LENGTH(a_crate->sfp); ++i) {
		struct GsiMppcRobModule *mppc_rob;

		mppc_rob = a_crate->sfp[i];
		if (NULL != mppc_rob) {
			mppc_rob->ctdcp.module.event_counter.value = 0;
		}
	}
	return 1;
}
