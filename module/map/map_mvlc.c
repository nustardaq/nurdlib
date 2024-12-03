/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Anna Kawecka
 * Florian Lüke
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

#	define MVLCC_CALL(func, args, label) do { \
	int ec_; \
	ec_ = func args; \
	if (0 != ec_) { \
		log_error(LOGL, \
		    "MVLCC call " #func #args " failed, %d = %s.", \
		    ec_, mvlcc_strerror(ec_)); \
		goto label; \
	} \
} while (0)

static void	mvlcc_init(void);

mvlcc_t g_mvlc;
static struct {
	char	ip[32];
} g_override = {""};

struct MvlcPrivate {
	enum	Keyword mode;
	int	do_fifo;
	int	do_mblt_swap;
};

void
mvlcc_init(void)
{
	LOGF(verbose)(LOGL, "mvlcc_init {");

	if (NULL == g_mvlc) {
		struct ConfigBlock *mvlc_cfg;
		char const *str;

		mvlc_cfg = config_get_block(NULL, KW_MESYTEC_MVLC);

		str = config_get_string(mvlc_cfg, KW_LOG_LEVEL);
		mvlcc_set_global_log_level(str);

		str = config_get_string(mvlc_cfg, KW_LINK_IP);
		if ('\0' != g_override.ip[0]) {
			LOGF(verbose)(LOGL, "Overriding ip='%s' by '%s'.",
			    str, g_override.ip);
			str = g_override.ip;
		}
		g_mvlc = mvlcc_make_mvlc(str);
		if (!mvlcc_is_mvlc_valid(g_mvlc)) {
			log_die(LOGL, "mvlcc_make_mvlc did not return a "
			    "valid MVLC instance for url='%s'.", str);
		}
		MVLCC_CALL(mvlcc_connect, (g_mvlc), fail);
fail:;
	}

	LOGF(verbose)(LOGL, "mvlcc_init }");
}

void
sicy_deinit(void)
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	if (NULL != g_mvlc) {
		mvlcc_disconnect(g_mvlc);
		mvlcc_free_mvlc(g_mvlc);
		g_mvlc = NULL;
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
	parser_include_file("mesytec_mvlc.cfg", 0);
	LOGF(verbose)(LOGL, "sicy_setup }");
}


uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;

	u32 = 0;
	MVLCC_CALL(mvlcc_single_vme_read,
	    (g_mvlc, a_map->address + a_ofs, &u32, 32, 32), fail);
	return u32;
fail:
	log_die(LOGL, "No recovery.");
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;

	u32 = 0;
	MVLCC_CALL(mvlcc_single_vme_read,
	    (g_mvlc, a_map->address + a_ofs, &u32, 32, 16), fail);
	return (uint16_t)u32;
fail:
	log_die(LOGL, "No recovery.");
}


void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	MVLCC_CALL(mvlcc_single_vme_write,
	    (g_mvlc, a_map->address + a_ofs, a_u32, 32, 32), fail);
fail:
	log_die(LOGL, "No recovery.");
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	MVLCC_CALL(mvlcc_single_vme_write,
	    (g_mvlc, a_map->address + a_ofs, a_u16, 32, 16), fail);
fail:
	log_die(LOGL, "No recovery.");
}

MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

#	ifdef POKE_MVLC

#		define POKE(m, value) \
{ \
	LOGF(verbose)(LOGL, "poke_" #m " %u bits {", a_bits); \
	mvlcc_init(); \
	switch (a_bits) { \
	case 16: { \
		uint32_t u32 = (uint32_t)value;	\
		if ('r' == *#m) { \
			MVLCC_CALL(mvlcc_single_vme_read, \
			    (g_mvlc, a_address + a_ofs, &u32, 32, 16), \
			    fail); \
		} else if ('w' == *#m) { \
			MVLCC_CALL(mvlcc_single_vme_write, \
			    (g_mvlc, a_address + a_ofs, u32, 32, 16), \
			    fail); \
		} else { \
			log_die(LOGL, "Invalid mode."); \
		} \
		} \
		break; \
	case 32: { \
		uint32_t u32_ = value; \
		if ('r' == *#m) { \
			MVLCC_CALL(mvlcc_single_vme_read, \
			    (g_mvlc, a_address + a_ofs, &u32_, 32, 32), \
			    fail); \
		} else if ('w' == *#m) { \
			MVLCC_CALL(mvlcc_single_vme_write, \
			    (g_mvlc, a_address + a_ofs, u32_, 32, 32), \
			    fail); \
		} else { \
			log_die(LOGL, "Invalid mode."); \
		} \
		} \
		break; \
	default: \
		log_die(LOGL, "Poking %u bits unsupported.", a_bits); \
	} \
	LOGF(verbose)(LOGL, "poke_" #m " }"); \
fail: \
	log_die(LOGL, "poke_" #m " failed."); \
}


void
poke_r(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits)
POKE(r, 0)

void
poke_w(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits, uint32_t a_value)
POKE(w, a_value)

#	endif

void
map_mvlc_config_override(char const *a_ip)
{
	if (NULL == a_ip) {
		g_override.ip[0] = '\0';
	} else {
		strlcpy_(g_override.ip, a_ip, sizeof g_override.ip);
	}
}

#ifdef BLT_MVLC

MAP_FUNC_EMPTY(blt_deinit);

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	struct MvlcPrivate *private = NULL;

	LOGF(verbose)(LOGL, "blt_map {");

	mvlcc_init();

	CALLOC(private, 1);

	private->mode = a_mode;
	private->do_fifo = a_do_fifo;
	private->do_mblt_swap = a_do_mblt_swap;

	a_map->private = private;

	LOGF(verbose)(LOGL, "blt_map(address=0x%08x, a_mode=0x%x, "
	    "a_do_fifo=%d, a_do_mblt_swap=%d)",
	    a_map->address, a_mode, a_do_fifo, a_do_mblt_swap);

	LOGF(verbose)(LOGL, "blt_map }");
}

static const uint8_t a32UserBlock   = 0x0B;
static const uint8_t a32UserBlock64 = 0x08;

int
blt_read(struct Map *a_map, size_t a_ofs, void *a_target, size_t a_bytes, int
    a_berr_ok)
{
	struct MvlcPrivate *private = NULL;
	size_t wordsOut = 0;
	struct MvlccBlockReadParams params;
	int ret = -1;

	(void)a_berr_ok;

	LOGF(spam)(LOGL, "blt_read(address=0x%08x, offset=0x%"PRIzx", "
	    "target=%p, bytes=%"PRIz") {",
	    a_map->address, a_ofs, a_target, a_bytes);

	private = a_map->private;

	ZERO(params);
	if (private->mode == KW_BLT) {
		params.amod = a32UserBlock;
	} else if (private->mode == KW_MBLT) {
		params.amod = a32UserBlock64;
	} else {
		log_die(LOGL, "BLT type %s not supported.",
		    keyword_get_string(private->mode));
	}

	params.fifo = private->do_fifo;
	params.swap = private->do_mblt_swap;

	ret = mvlcc_vme_block_read(g_mvlc, a_map->address + a_ofs, a_target,
	    a_bytes/sizeof(uint32_t), &wordsOut, params);

	LOGF(spam)(LOGL, "blt_read(): ec=%d (%s), wordsOut=%"PRIz", "
	    "a_mode=0x%x, do_fifo=%d, do_mblt_swap=%d",
	    ret, mvlcc_strerror(ret), wordsOut, params.amod, private->do_fifo,
	    private->do_mblt_swap);

	ret = ret ? -ret : (int)(wordsOut * sizeof(uint32_t));

	LOGF(spam)(LOGL, "blt_read(ret=%d) }", ret);
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
