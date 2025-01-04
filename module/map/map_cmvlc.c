/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2020-2024
 * Anna Kawecka
 * Florian Lüke
 * Hans Toshihide Törnqvist
 * Håkan Torbjörn Johansson
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

#ifdef SICY_CMVLC

#	include <cmvlc.h>
#	include <nurdlib/log.h>
#	include <nurdlib/config.h>
#	include <util/string.h>
#	include <util/time.h>
#	include <config/parser.h>

#	define CMVLC_CALL(func, args, label) do { \
	int ret_; \
	ret_ = func args; \
	if (ret_ < 0) { \
		log_error(LOGL, \
		    "CMVLC call " #func #args " failed, %d = %s.", \
		    ret_, cmvlc_last_error(g_cmvlc)); \
		goto label; \
	} \
} while (0)

static void	cmvlc_init(void);

static struct cmvlc_client *g_cmvlc;
static struct {
	char	ip[32];
} g_override = {""};

struct CmvlcPrivate {
	enum	Keyword mode;
	int	do_fifo;
	int	do_mblt_swap;
};

void
cmvlc_init(void)
{
	LOGF(verbose)(LOGL, "cmvlc_init {");

	if (NULL == g_cmvlc) {
		struct ConfigBlock *cmvlc_cfg;
		char const *str;
		char const *error_str;

		cmvlc_cfg = config_get_block(NULL, KW_MESYTEC_MVLC);

		/* cmvlc has no log level (it does (should) not print)
		str = config_get_string(cmvlc_cfg, KW_LOG_LEVEL);
		cmvlc_set_global_log_level(str);
		*/

		str = config_get_string(cmvlc_cfg, KW_LINK_IP);
		if ('\0' != g_override.ip[0]) {
			LOGF(verbose)(LOGL, "Overriding ip='%s' by '%s'.",
			    str, g_override.ip);
			str = g_override.ip;
		}
		g_cmvlc = cmvlc_connect(str,
					CMVLC_CONNECT_FLAGS_DATA,
					&error_str, NULL/*stdout*/);
		if (g_cmvlc == NULL) {
			log_die(LOGL, "cmvlc_connect failure for "
				"url='%s': '%s'.", str, error_str);
		}
	}

	LOGF(verbose)(LOGL, "cmvlc_init }");
}

void
sicy_deinit(void)
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	if (NULL != g_cmvlc) {
		cmvlc_close(g_cmvlc);
		g_cmvlc = NULL;
	}
	LOGF(verbose)(LOGL, "sicy_deinit }");
}

void
sicy_map(struct Map *a_map)
{
	(void)a_map;

	LOGF(verbose)(LOGL, "sicy_map {");
	cmvlc_init();
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
	CMVLC_CALL(cmvlc_single_vme_read,
	    (g_cmvlc, a_map->address + a_ofs, &u32,
	     vme_user_A32, vme_D32), fail);
	return u32;
fail:
	log_die(LOGL, "No recovery.");
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;

	u32 = 0;
	CMVLC_CALL(cmvlc_single_vme_read,
	    (g_cmvlc, a_map->address + a_ofs, &u32,
	     vme_user_A32, vme_D16), fail);
	return (uint16_t)u32;
fail:
	log_die(LOGL, "No recovery.");
}


void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	CMVLC_CALL(cmvlc_single_vme_write,
	    (g_cmvlc, a_map->address + a_ofs, a_u32,
	     vme_user_A32, vme_D32), fail);
fail:
	log_die(LOGL, "No recovery.");
}

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	CMVLC_CALL(cmvlc_single_vme_write,
	    (g_cmvlc, a_map->address + a_ofs, a_u16,
	     vme_user_A32, vme_D16), fail);
fail:
	log_die(LOGL, "No recovery.");
}

MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

#	ifdef POKE_CMVLC

#		define POKE(m, value) \
{ \
	LOGF(verbose)(LOGL, "poke_" #m " %u bits {", a_bits); \
	cmvlc_init(); \
	switch (a_bits) { \
	case 16: { \
		uint32_t u32 = (uint32_t)value;	\
		if ('r' == *#m) { \
			CMVLC_CALL(cmvlc_single_vme_read, \
			    (g_cmvlc, a_address + a_ofs, &u32, \
			     vme_user_A32, vme_D16), \
			    fail); \
		} else if ('w' == *#m) { \
			CMVLC_CALL(cmvlc_single_vme_write, \
			    (g_cmvlc, a_address + a_ofs, u32, \
			     vme_user_A32, vme_D16), \
			    fail); \
		} else { \
			log_die(LOGL, "Invalid mode."); \
		} \
		} \
		break; \
	case 32: { \
		uint32_t u32_ = value; \
		if ('r' == *#m) { \
			CMVLC_CALL(cmvlc_single_vme_read, \
			    (g_cmvlc, a_address + a_ofs, &u32_, \
			     vme_user_A32, vme_D32), \
			    fail); \
		} else if ('w' == *#m) { \
			CMVLC_CALL(cmvlc_single_vme_write, \
			    (g_cmvlc, a_address + a_ofs, u32_, \
			     vme_user_A32, vme_D32), \
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
	return; \
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

#endif
