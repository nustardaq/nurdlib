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

#include <module/module.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <module/genlist.h>
#include <module/map/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/pack.h>
#include <util/ssort.h>
#include <util/string.h>

struct Module *
module_create(struct Crate *a_crate, enum Keyword a_module_type, struct
    ConfigBlock *a_config_block)
{
	size_t i;

	for (i = 0;; ++i) {
		struct ModuleListEntry const *e;

		e = &c_module_list_[i];
		if (KW_NONE == e->type) {
			log_die(LOGL, "module_create: Unsupported module "
			    "type '%s'.", keyword_get_string(a_module_type));
		}
		if (a_module_type == e->type) {
			enum Keyword c_log_levels[] = {KW_OFF, KW_INFO,
				KW_VERBOSE, KW_DEBUG, KW_SPAM};
			struct Module *module;
			enum Keyword log_level;

			module = e->create(a_crate, a_config_block);
			if (0 == module->type) {
				module->type = a_module_type;
			}
			module->config = a_config_block;
			log_level = CONFIG_GET_KEYWORD(a_config_block,
			    KW_LOG_LEVEL, c_log_levels);
			module->log_level = KW_OFF == log_level ? NULL :
			    log_level_get_from_keyword(log_level);
			return module;
		}
	}
}

struct Module *
module_create_base(size_t a_inherited_size, struct ModuleProps const *a_props)
{
	struct Module *module;

	assert(sizeof *module <= a_inherited_size);
	module = calloc(a_inherited_size, 1);
	module->props = a_props;
	module->id = -1;
	module->event_counter.mask = ~0;
	return module;
}

void
module_free(struct Module **a_module)
{
	struct Module *module;

	module = *a_module;
	if (NULL != module->props) {
		module->props->destroy(module);
	}
	FREE(module->pedestal.array);
	FREE(*a_module);
}

void
module_gate_get(struct ModuleGate *a_gate, struct ConfigBlock *a_block, double
    a_trigger_before_ns, double a_trigger_after_ns, double a_width_min, double
    a_width_max)
{
	struct ConfigBlock *block_gate;

	block_gate = config_get_block(a_block, KW_GATE);
	if (NULL == block_gate) {
		log_die(LOGL, "Could not get requested 'GATE' block.");
	}
	a_gate->time_after_trigger_ns = config_get_double(block_gate,
	    KW_TIME_AFTER_TRIGGER, CONFIG_UNIT_NS, a_trigger_before_ns,
	    a_trigger_after_ns);
	a_gate->width_ns = config_get_double(block_gate, KW_WIDTH,
	    CONFIG_UNIT_NS, a_width_min, a_width_max);
	LOGF(verbose)(LOGL, "Module gate (start=%gns, width=%gns).",
	    a_gate->time_after_trigger_ns, a_gate->width_ns);
}

enum Keyword
module_get_type(struct Module const *a_module)
{
	return a_module->type;
}

void
module_parse_error(struct LogFile const *a_file, unsigned a_line, struct
    EventConstBuffer const *a_event_buffer, void const *a_p, char const
    *a_fmt, ...)
{
	char str[1024];
	uint8_t const *p8;
	va_list args;
	uintptr_t ofs;

	va_start(args, a_fmt);
	vsnprintf(str, sizeof str, a_fmt, args);
	va_end(args);
	ofs = (uintptr_t)a_p - (uintptr_t)a_event_buffer->ptr;
	p8 = a_p;
	log_error((void const *)a_file, a_line,
	    "%s (ofs=0x%08"PRIzx",u8[4]=0x%02x%02x%02x%02x).",
	    str, ofs, p8[0], p8[1], p8[2], p8[3]);
}

void
module_pedestal_add(struct Pedestal *a_pedestal, uint16_t a_value)
{
	int idx;

	/*
	 * buf_idx < PEDESTAL_BUF_LEN means we haven't even collected one full
	 * buffer, then we cycle in [BUF_LEN, 2 * BUF_LEN).
	 */
	if (2 * PEDESTAL_BUF_LEN <= a_pedestal->buf_idx) {
		log_die(LOGL, "Pedestal buffering indexing corrupt, were all"
		    " pedestal structs zeroed?");
	}
	idx = (PEDESTAL_BUF_LEN - 1) & a_pedestal->buf_idx;
	a_pedestal->buf[idx] = a_value;
	++a_pedestal->buf_idx;
	if (2 * PEDESTAL_BUF_LEN == a_pedestal->buf_idx) {
		a_pedestal->buf_idx = PEDESTAL_BUF_LEN;
	}
}

int
module_pedestal_calculate(struct Pedestal *a_pedestal)
{
	static uint16_t array[PEDESTAL_BUF_LEN];
	size_t i, i_max, len;

	a_pedestal->threshold = 0;

	/* Still didn't collect one full pedestal buffer. */
	if (PEDESTAL_BUF_LEN > a_pedestal->buf_idx) {
		return 0;
	}
	/*
	 * Shell sort does very well with few unique keys in small arrays.
	 * Need copy to not disturb time-order of pedestal buf.
	 */
	COPY(array, a_pedestal->buf);
	len = LENGTH(array);
	shell_sort(array, len);

	/*
	 * Line between (x=0,y=0) to (array[i],i), let it walk along the
	 * integrated channel histogram ridge and remember where the slope
	 * starts decreasing, add 12.5% and we're done.
	 */
	for (i = 0;; ++i) {
		if (LENGTH(array) == i) {
			/* Hmm, all zeros. */
			return 0;
		}
		if (0 != array[i]) {
			break;
		}
	}
	/* Skip ahead 50% of the non-zero data. */
	i = (i + LENGTH(array)) / 2;
	/* Search for the maximum slope between (0,0) and (array[i],i). */
	i_max = i;
	for (; LENGTH(array) > i; ++i) {
		if (i * array[i_max] > i_max * array[i]) {
			i_max = i;
		}
	}
	i = array[i_max];
	a_pedestal->threshold = i + (i >> 3);

	/*
	 * Uh, that's it? Yeah. Deal with it.
	 *
	 *   ################################
	 * ##       ## # ####### ## # #######
	 *           ## # #####   ## # #####
	 *            ## # ###     ## # ###
	 *             ######       ######
	 */

	/* TODO: Some sanity checks? */

	return 1;
}

struct RegisterEntryClient const *
module_register_list_client_get(enum Keyword a_module_type)
{
	size_t i;

	for (i = 0;; ++i) {
		enum Keyword type;

		type = c_module_register_list_client_[i].type;
		if (KW_NONE == type) {
			log_die(LOGL, "Unsupported module type %d='%s'.",
			    a_module_type, keyword_get_string(a_module_type));
		}
		if (a_module_type == type) {
			return c_module_register_list_client_[i].list;
		}
	}
}

void
module_register_list_pack(struct PackerList *a_list, struct Module *a_module,
    int a_submodule_l)
{
	struct ModuleRegisterListEntryServer const *list;
	struct Map *map;
	struct Packer *packer;
	size_t i;

	/* TODO: To do. */
	(void)a_submodule_l;
	for (i = 0;; ++i) {
		list = &c_module_register_list_server_[i];
		if (KW_NONE == list->type) {
			packer = packer_list_get(a_list, 16);
			pack16(packer, -1);
			PACK_LOC(packer);
			return;
		}
		if (a_module->type == list->type) {
			break;
		}
	}
	packer = packer_list_get(a_list, 16);
	pack16(packer, a_module->type);
	map = a_module->props->get_map(a_module);
	if (!map) {
		/* Module currently unmapped or undumpable, nop. */
		return;
	}
	for (i = 0;; ++i) {
		struct RegisterEntryServer const *entry;
		uint32_t ofs;
		unsigned j, num;

		entry = &list->list[i];
		if (0 == entry->bits) {
			break;
		} else if (16 != entry->bits && 32 != entry->bits) {
			log_die(LOGL, "Invalid # bits %u.", entry->bits);
		}
		ofs = entry->address;
		num = MAX(1, entry->array_length);
		for (j = 0; num > j; ++j) {
			uint32_t val;

			/* If requested, must be dumpable! */
			if (16 == entry->bits) {
				val = map_sicy_read(map, MAP_MOD_R, 16, ofs);
				packer = packer_list_get(a_list, 16);
				pack16(packer, val);
			} else if (32 == entry->bits) {
				val = map_sicy_read(map, MAP_MOD_R, 32, ofs);
				packer = packer_list_get(a_list, 32);
				pack32(packer, val);
			}
			ofs += entry->byte_step;
		}
	}
	if (NULL != a_module->props->register_list_pack) {
		a_module->props->register_list_pack(a_module, a_list);
	}
}

void
module_setup(void)
{
	size_t i;

	STATIC_ASSERT(IS_POW2(PEDESTAL_BUF_LEN));

	for (i = 0;; ++i) {
		struct ModuleListEntry const *e;

		e = &c_module_list_[i];
		if (KW_NONE == e->type) {
			break;
		}
		e->setup();
		config_auto_register(e->type, e->auto_cfg);
		config_auto_register(e->type, "module_log_level.cfg");
	}
	config_auto_register(KW_GSI_CTDC_CARD, "gsi_ctdc_card.cfg");
	config_auto_register(KW_GSI_FEBEX_CARD, "gsi_febex_card.cfg");
	config_auto_register(KW_GSI_KILOM_CARD, "gsi_kilom_card.cfg");
	config_auto_register(KW_GSI_MPPC_ROB_CARD, "gsi_mppc_rob_card.cfg");
	config_auto_register(KW_GSI_PEX, "gsi_pex.cfg");
	config_auto_register(KW_GSI_SIDEREM, "gsi_siderem.cfg");
	config_auto_register(KW_GSI_TAMEX_CARD, "gsi_tamex_card.cfg");
	config_auto_register(KW_PNPI_CROS3, "pnpi_cros3.cfg");
}
