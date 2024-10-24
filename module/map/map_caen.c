/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
 * Håkan T Johansson
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

#ifdef SICY_CAEN

#	include <CAENVMElib.h>
#	include <config/parser.h>
#	include <nurdlib/base.h>
#	include <nurdlib/config.h>
#	include <nurdlib/log.h>
#	include <util/string.h>
#	include <util/time.h>

#	define CALL_(func, args, label, is_init) do { \
		CVErrorCodes code_; \
		code_ = func args; \
		if (cvSuccess != code_) { \
			log_error(LOGL, "%d=%s.", code_, \
			    CAENVME_DecodeError(code_)); \
			if (is_init) { \
				log_error(LOGL, \
"If link_number or conet_node != 0, try to swap them, due to a regression " \
"in CAENVMElib under certain conditions (3.2.0 <= version < 3.4.0)."); \
			} \
			goto label; \
		} \
	} while (0)
#	define CALL(func, args, label) CALL_(func, args, label, 0)

#define TYPE(name) { cv##name, #name }
struct BoardType {
	CVBoardTypes type;
	char const *str;
};

/* From version 3.4.4. */
static const struct BoardType g_board_type_v3_4_4[] = {
	TYPE(V1718),
        TYPE(V2718),
        TYPE(A2818),
        TYPE(A2719),
        TYPE(A3818),

        TYPE(USB_A4818_V2718_LOCAL),
        TYPE(USB_A4818_V2718),
        TYPE(USB_A4818_LOCAL),
        TYPE(USB_A4818_V3718_LOCAL),
        TYPE(USB_A4818_V3718),
        TYPE(USB_A4818_V4718_LOCAL),
        TYPE(USB_A4818_V4718),
        TYPE(USB_A4818),
        TYPE(USB_A4818_A2719_LOCAL),

	TYPE(USB_V3718_LOCAL),
        TYPE(PCI_A2818_V3718_LOCAL),
        TYPE(PCIE_A3818_V3718_LOCAL),

        TYPE(USB_V3718),
        TYPE(PCI_A2818_V3718),
        TYPE(PCIE_A3818_V3718),

        TYPE(USB_V4718_LOCAL),
        TYPE(PCI_A2818_V4718_LOCAL),
        TYPE(PCIE_A3818_V4718_LOCAL),
        TYPE(ETH_V4718_LOCAL),

        TYPE(USB_V4718),
        TYPE(PCI_A2818_V4718),
        TYPE(PCIE_A3818_V4718),
        TYPE(ETH_V4718)
};

static int32_t g_handle = -1;
static struct {
	char	board_type[32];
	char	link_ip[32];
	int	link_number;
	int	conet_node;
} g_override = {"", "", -1, -1};

void
caen_init()
{
	char link_ip[32];
	struct ConfigBlock *caenvme;
	char const *str;
	size_t i;
	static CVBoardTypes board_type = cvInvalid;
	uint32_t link_number, conet_node;

	if (-1 != g_handle) {
		return;
	}

	LOGF(verbose)(LOGL, "caen_init {");

	caenvme = config_get_block(NULL, KW_CAEN_VMELIB);
	str = config_get_string(caenvme, KW_TYPE);
	if ('\0' != g_override.board_type[0]) {
		LOGF(verbose)(LOGL, "Overriding board_type='%s' by '%s'.",
		    str, g_override.board_type);
		str = g_override.board_type;
	}
	for (i = 0; i < LENGTH(g_board_type_v3_4_4); ++i) {
		struct BoardType const *t;

		t = &g_board_type_v3_4_4[i];
		if (0 == strcmp(str, t->str)) {
			board_type = t->type;
			break;
		}
	}
	if (cvInvalid == board_type) {
		log_error(LOGL,
		    "Available CAENVME type values (as of v3.4.4):");
		for (i = 0; i < LENGTH(g_board_type_v3_4_4); ++i) {
			log_error(LOGL, "%s", g_board_type_v3_4_4[i].str);
		}
		log_die(LOGL, "type=\"%s\" not recognized, I bail.", str);
	}

	str = config_get_string(caenvme, KW_LINK_IP);
	if ('\0' != g_override.link_ip[0]) {
		LOGF(verbose)(LOGL, "Overriding link_ip='%s' by '%s'.",
		    str, g_override.link_ip);
		str = g_override.link_ip;
	}
	strlcpy_(link_ip, str, sizeof link_ip);

	link_number = config_get_int32(caenvme, KW_LINK_NUMBER,
	    CONFIG_UNIT_NONE, 0, 10);
	conet_node = config_get_int32(caenvme, KW_CONET_NODE,
	    CONFIG_UNIT_NONE, 0, 10);

	if (-1 != g_override.link_number) {
		LOGF(verbose)(LOGL, "Overriding link_number='%d' by '%d'.",
		    link_number, g_override.link_number);
		link_number = g_override.link_number;
	}
	if (-1 != g_override.conet_node) {
		LOGF(verbose)(LOGL, "Overriding conet_node='%d' by '%d'.",
		    conet_node, g_override.conet_node);
		conet_node = g_override.conet_node;
	}

#if CAENVME_VERSION_NUMBER < 30400
	LOGF(verbose)(LOGL,
	    "CAENVME_Init board_type=%d, link_number=%u, conet_node=%u.",
	    board_type, link_number, conet_node);
	CALL_(CAENVME_Init, (board_type, link_number, conet_node, &g_handle),
	    fail, 1);
#else
	{
		void *arg1;

		if ('\0' == link_ip[0]) {
			arg1 = &link_number;
			LOGF(verbose)(LOGL, "CAENVME Init "
			    "arg1=link-number=%u.", link_number);
		} else {
			arg1 = link_ip;
			LOGF(verbose)(LOGL, "CAENVME Init arg1=link-ip=%s.",
			    link_ip);
		}
		LOGF(verbose)(LOGL,
		    "CAENVME_Init2 board_type=%d, link_ip=\"%s\" "
		    "conet_node=%u.",
		    board_type, link_ip, conet_node);
		CALL_(CAENVME_Init2, (board_type, arg1, conet_node,
		    &g_handle), fail, 1);
	}
#endif
	/* TODO: Make config! */
	time_sleep(0.5);

	LOGF(verbose)(LOGL, "caen_init }");
	return;

fail:
	log_die(LOGL, "CAEN init failed.");
}

void
sicy_deinit()
{
	LOGF(verbose)(LOGL, "sicy_deinit {");
	if (-1 != g_handle) {
		CALL(CAENVME_End, (g_handle), fail);
fail:
		g_handle = -1;
	}
	LOGF(verbose)(LOGL, "sicy_deinit }");
}

void
sicy_map(struct Map *a_map)
{
	(void)a_map;
	LOGF(verbose)(LOGL, "sicy_map {");
	caen_init();
	LOGF(verbose)(LOGL, "sicy_map }");
}

uint16_t
sicy_r16(struct Map *a_map, size_t a_ofs)
{
	uint16_t u16;

	u16 = 0;
	CALL(CAENVME_ReadCycle, (g_handle, a_map->address + a_ofs, &u16,
	    cvA32_U_DATA, cvD16), fail);
	return u16;
fail:
	log_error(LOGL, "Failed reading addr=0x%08x ofs=0x%x.",
	    (uint32_t)a_map->address, (uint32_t)a_ofs);
	return 0;
}

uint32_t
sicy_r32(struct Map *a_map, size_t a_ofs)
{
	uint32_t u32;

	u32 = 0;
	CALL(CAENVME_ReadCycle, (g_handle, a_map->address + a_ofs, &u32,
	    cvA32_U_DATA, cvD32), fail);
	return u32;
fail:
	log_error(LOGL, "Failed reading addr=0x%08x ofs=0x%x.",
	    (uint32_t)a_map->address, (uint32_t)a_ofs);
	return 0;
}

void
sicy_setup(void)
{
	LOGF(verbose)(LOGL, "sicy_setup {");
	parser_include_file("caen_vmelib.cfg", 0);
	LOGF(verbose)(LOGL, "sicy_setup }");
}

MAP_FUNC_EMPTY(sicy_shutdown);
UNMAP_FUNC_EMPTY(sicy);

void
sicy_w16(struct Map *a_map, size_t a_ofs, uint16_t a_u16)
{
	CALL(CAENVME_WriteCycle, (g_handle, a_map->address + a_ofs, &a_u16,
	    cvA32_U_DATA, cvD16), fail);
	return;
fail:
	log_error(LOGL, "Failed writing addr=0x%08x ofs=0x%x val=0x%x.",
	    (uint32_t)a_map->address, (uint32_t)a_ofs, a_u16);
}

void
sicy_w32(struct Map *a_map, size_t a_ofs, uint32_t a_u32)
{
	CALL(CAENVME_WriteCycle, (g_handle, a_map->address + a_ofs, &a_u32,
	    cvA32_U_DATA, cvD32), fail);
	return;
fail:
	log_error(LOGL, "Failed writing addr=0x%08x ofs=0x%x val=0x%x.",
	    (uint32_t)a_map->address, (uint32_t)a_ofs, a_u32);
}

#endif

#ifdef POKE_CAEN

#define POKE(Mode, m, value) \
{ \
	CVErrorCodes code; \
	LOGF(verbose)(LOGL, "poke_" #m " %u bits {", a_bits); \
	caen_init(); \
	switch (a_bits) { \
	case 16: { \
		uint16_t u16_ = value; \
		code = CAENVME_##Mode##Cycle(g_handle, \
		    a_address + a_ofs, &u16_, cvA32_U_DATA, cvD16); \
		} \
		break; \
	case 32: { \
		uint32_t u32_ = value; \
		code = CAENVME_##Mode##Cycle(g_handle, \
		    a_address + a_ofs, &u32_, cvA32_U_DATA, cvD32); \
		} \
		break; \
	default: \
		log_die(LOGL, "Poking %u bits unsupported.", a_bits); \
	} \
	if (cvSuccess != code) { \
		log_die(LOGL, "poke_" #m " failed."); \
	} \
	LOGF(verbose)(LOGL, "poke_" #m " }"); \
}

void
poke_r(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits)
POKE(Read, r, 0)

void
poke_w(uint32_t a_address, uintptr_t a_ofs, unsigned a_bits, uint32_t a_value)
POKE(Write, w, a_value)

#endif

#ifdef BLT_CAEN

MAP_FUNC_EMPTY(blt_deinit);

void
blt_map(struct Map *a_map, enum Keyword a_mode, int a_do_fifo, int
    a_do_mblt_swap)
{
	(void)a_do_fifo;
	(void)a_do_mblt_swap;

	LOGF(verbose)(LOGL, "blt_map {");
	caen_init();
	a_map->private = (void *)(uintptr_t)a_mode;
	LOGF(verbose)(LOGL, "blt_map }");
}

int
blt_read(struct Map *a_map, size_t a_ofs, void *a_target, size_t a_bytes, int
    a_berr_ok)
{
	enum Keyword mode;
	int bytes_out;
	CVAddressModifier am;
	CVDataWidth dw;
	int ret;

	ret = -1;
	LOGF(spam)(LOGL, "blt_read(address=0x%08x, offset=0x%"PRIzx", "
	    "target=%p, bytes=%"PRIz") {",
	    a_map->address, a_ofs, a_target, a_bytes);
	mode = (enum Keyword)(uintptr_t)a_map->private;
	switch (mode) {
	case KW_BLT:
		am = cvA32_U_BLT;
		dw = cvD32;
		break;
	case KW_MBLT:
		am = cvA32_U_MBLT;
		dw = cvD64;
		break;
	default:
		log_die(LOGL, "BLT type %s not supported.",
		    keyword_get_string(mode));
	}
	bytes_out = 0;
	CALL(CAENVME_BLTReadCycle, (g_handle, a_map->address + a_ofs,
	    a_target, a_bytes, am, dw, &bytes_out), fail);
	ret = bytes_out;
fail:
	LOGF(spam)(LOGL, "blt_read }");
	return ret;
}

MAP_FUNC_EMPTY(blt_setup);
MAP_FUNC_EMPTY(blt_shutdown);
UNMAP_FUNC_EMPTY(blt);

int
map_caen_type_exists(char const *a_name)
{
	size_t i;

	for (i = 0; i < LENGTH(g_board_type_v3_4_4); ++i) {
		struct BoardType const *t;

		t = &g_board_type_v3_4_4[i];
		if (0 == strcmp(a_name, t->str)) {
			return 1;
		}
	}
	return 0;
}

char const *
map_caen_type_get(size_t a_i)
{
	return a_i < LENGTH(g_board_type_v3_4_4) ?
	    g_board_type_v3_4_4[a_i].str : NULL;
}

void
map_caen_config_override(char const *a_type, char const *a_link_ip, int
    a_link_number, int a_conet_node)
{
	if (NULL == a_type) {
		g_override.board_type[0] = '\0';
	} else {
		strlcpy_(g_override.board_type, a_type,
		    sizeof g_override.board_type);
	}
	if (NULL == a_link_ip) {
		g_override.link_ip[0] = '\0';
	} else {
		strlcpy_(g_override.link_ip, a_link_ip,
		    sizeof g_override.link_ip);
	}
	g_override.link_number = a_link_number;
	g_override.conet_node = a_conet_node;
}

#endif
