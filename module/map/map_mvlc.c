/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Håkan T Johansson
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

#include <module/map/internal.h>
#include <nurdlib/base.h>

#ifdef SICY_MVLC

#	include <mvlcc_wrap.h>
#	include <nurdlib/log.h>
#	include <nurdlib/config.h>
#	include <util/string.h>
#	include <util/time.h>
#	include <config/parser.h>

static void	mvlcc_init(void);

#define Write 1
#define Read 2

int ec;
mvlcc_t mvlc;

struct MvlcPrivate
{
	enum Keyword mode;
	int do_fifo;
	int do_mblt_swap;
};

void
mvlcc_init(void)
{
	char const *str;
	struct ConfigBlock *mvlc_cfg;

	if (mvlc)
	{
		LOGF(verbose)(LOGL, "mvlcc_init: already initialized");
		return;
	}

	LOGF(verbose)(LOGL, "mvlcc_init {");

	mvlc_cfg = config_get_block(NULL, KW_MESYTEC_MVLC);

	str = config_get_string(mvlc_cfg, KW_LOG_LEVEL);
	mvlcc_set_global_log_level(str);

	str = config_get_string(mvlc_cfg, KW_LINK_IP);
	mvlc = mvlcc_make_mvlc(str);

	if (!mvlcc_is_mvlc_valid(mvlc))
	{
		log_die(LOGL, "mvlcc_make_mvlc did not return a valid MVLC instance for url='%s'.", str);
	}

	ec = mvlcc_connect(mvlc);

	if (ec)
	{
		log_die(LOGL, "mvlcc_init failed with error code %d (%s).", ec, mvlcc_strerror(ec));
	}
	LOGF(verbose)(LOGL, "mvlcc_init }");
}

void
sicy_deinit()
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	if (NULL != mvlc) {
		mvlcc_disconnect(mvlc);
		mvlcc_free_mvlc(mvlc);
		mvlc = NULL;
	}
	LOGF(verbose)(LOGL, "sicy_deinit }");
}

void
sicy_map(struct Map *a_map)
{
	(void)a_map;
	LOGF(verbose)(LOGL, "sicy_map {");
	mvlcc_init();
	LOGF(verbose)(LOGL, "sicy_map }");
}

void
sicy_setup(void)
{
	LOGF(verbose)(LOGL, "sicy_setup {");
	parser_include_file("mvlc.cfg", 0);
	LOGF(verbose)(LOGL, "sicy_setup }");
}


uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;
	u32 = 0;

	ec = mvlcc_single_vme_read(mvlc, a_map->address + a_ofs, &u32, 32, 32);

	return u32;
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;
	u32 = 0;

	ec = mvlcc_single_vme_read(mvlc, a_map->address + a_ofs, &u32, 32, 16);

	return (uint16_t)u32;
}


void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	ec = mvlcc_single_vme_write(mvlc, a_map->address + a_ofs, a_u32, 32, 32);
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	uint32_t a_u32;
	a_u32 = (uint32_t)a_u16;
	ec = mvlcc_single_vme_write(mvlc, a_map->address + a_ofs, a_u32, 32, 16);
}

MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

#ifdef POKE_MVLC

#define POKE(Mode, m, value) \
{ \
	LOGF(verbose)(LOGL, "poke_" #m " %u bits {", a_bits); \
	mvlcc_init(); \
	switch (a_bits) { \
	case 16: { \
		uint32_t u32 = (uint32_t)value;	\
		if (Mode == Read) { \
			ec = mvlcc_single_vme_read(mvlc, a_address + a_ofs, &u32, 32, 16); \
		} else if (Mode == Write) { \
			ec = mvlcc_single_vme_write(mvlc, a_address + a_ofs, u32, 32, 16); \
		} else { \
			log_die(LOGL, "Invalid mode."); \
		} \
		} \
		break; \
	case 32: { \
		uint32_t u32_ = value; \
		if (Mode == Read) { \
			ec = mvlcc_single_vme_read(mvlc, a_address + a_ofs, &u32_, 32, 32); \
		} else if (Mode == Write) { \
			ec = mvlcc_single_vme_write(mvlc, a_address + a_ofs, u32_, 32, 32); \
		} else { \
			log_die(LOGL, "Invalid mode."); \
		} \
		} \
		break; \
	default: \
		log_die(LOGL, "Poking %u bits unsupported.", a_bits); \
	} \
	sicy_deinit(); \
	LOGF(verbose)(LOGL, "poke_" #m " }"); \
}


void
poke_r(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits)
POKE(Read, r, 0)

void
poke_w(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits, uint32_t a_value)
POKE(Write, w, a_value)

#endif

#ifdef BLT_MVLC

MAP_FUNC_EMPTY(blt_deinit);

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int a_do_mblt_swap)
{
	LOGF(verbose)(LOGL, "blt_map {");

	mvlcc_init();

	struct MvlcPrivate *private = NULL;
	CALLOC(private, 1);

	private->mode = a_mode;
	private->do_fifo = a_do_fifo;
	private->do_mblt_swap = a_do_mblt_swap;

	a_map->private = private;

	LOGF(verbose)(LOGL, "  blt_map(address=0x%08x, a_mode=0x%x, a_do_fifo=%d, "
	    "a_do_mblt_swap=%d)", a_map->address, a_mode, a_do_fifo, a_do_mblt_swap);

	LOGF(verbose)(LOGL, "blt_map }");
}

static const uint8_t a32UserBlock   = 0x0B;
static const uint8_t a32UserBlock64 = 0x08;

int
blt_read(struct Map *a_map, size_t a_ofs, void *a_target, size_t a_bytes, int a_berr_ok)
{
	(void) a_berr_ok; // no clue

	struct MvlcPrivate *private = NULL;
	size_t wordsOut = 0;
	struct MvlccBlockReadParams params = {};
	int ret = -1;

	LOGF(verbose)(LOGL, "blt_read(address=0x%08x, offset=0x%"PRIzx", "
	    "target=%p, bytes=%"PRIz") {",
	    a_map->address, a_ofs, a_target, a_bytes);

	private = (struct MvlcPrivate *)a_map->private;

	if (private->mode == KW_BLT)
		params.amod = a32UserBlock;
	else if (private->mode == KW_MBLT)
		params.amod = a32UserBlock64;
	else
		log_die(LOGL, "BLT type %s not supported.",
		    keyword_get_string(private->mode));

	params.fifo = private->do_fifo;
	params.swap = private->do_mblt_swap;

	ret = mvlcc_vme_block_read(mvlc, a_map->address + a_ofs, a_target, a_bytes/sizeof(uint32_t),
		&wordsOut, params);

	LOGF(verbose)(LOGL, "blt_read(): ec=%d (%s), wordsOut=%"PRIz", a_mode=0x%x, do_fifo=%d, do_mblt_swap=%d",
	    ret, mvlcc_strerror(ret), wordsOut, params.amod, private->do_fifo, private->do_mblt_swap);

	if (ret)
	{
		LOGF(verbose)(LOGL, "blt_read }");
		return -ret;
	}

	ret = wordsOut * sizeof(uint32_t);

	LOGF(verbose)(LOGL, "blt_read }");
	return ret;
}

void
blt_unmap(struct Map *a_map)
{
	LOGF(verbose)(LOGL, "blt_unmap {");
	FREE(a_map->private);
	LOGF(verbose)(LOGL, "blt_unmap }");
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);

#endif

#endif
