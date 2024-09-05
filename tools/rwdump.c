/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2024
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

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctrl/ctrl.h>
#include <module/map/map.h>
#include <module/module.h>
#include <util/argmatch.h>
#include <util/pack.h>
#include <util/string.h>
#include <module/genlist.h>

static struct Map	*get_map(struct Module *) FUNC_RETURNS;
static void	usage(char const *, ...) FUNC_PRINTF(1, 2);

static char const *g_arg0;
static struct Map *g_map;

struct Map *
get_map(struct Module *a_module)
{
	(void)a_module;
	return g_map;
}

void
usage(char const *a_fmt, ...)
{
	FILE *str;
	int exit_code;

	/* Some platforms don't like null or empty format strings. */
	if (0 == strcmp(a_fmt, "null")) {
		str = stdout;
		exit_code = EXIT_SUCCESS;
	} else {
		va_list args;

		va_start(args, a_fmt);
		vfprintf(stderr, a_fmt, args);
		fprintf(stderr, "\n");
		va_end(args);
		str = stderr;
		exit_code = EXIT_FAILURE;
	}
	fprintf(str, "Usage: %s <options>\n", g_arg0);
	fprintf(str, "  -h, --help                  Print this help and "
	    "exit.\n");
	fprintf(str, "  -v, --verbosity=level       Log-level, number or "
	    "name:\n");
	fprintf(str, "                               0 = info (default)\n");
	fprintf(str, "                               1 = verbose\n");
	fprintf(str, "                               2 = debug\n");
	fprintf(str, "                               3 = spam\n");
#ifdef SICY_CAEN
	fprintf(str, "  -c, --caen=type[,link/ip[,conet]]\n");
	fprintf(str, "                              CAEN controller config "
	    "override.\n");
#endif
	fprintf(str, "  -l, --list                  List module types and "
	    "exit.\n");
	fprintf(str, "  -t, --type=type             ASCII name of module "
	    "types, try '-l'.\n");
	fprintf(str, "  -a, --addr=address          VME hex address, e.g. "
	    "'4000000'.\n");
	fprintf(str, "  -d, --register-dump         Dump registers for module"
	    " 'type' at 'address'.\n");
	fprintf(str, "  -r, --raw-read=bits         Read \"bits\"-bit value "
	    "at address.\n");
	fprintf(str, "  -w, --raw-write=bits,value  Write \"bits\"-bit value "
	    "to offset.\n");
	exit(exit_code);
}

int
main(int argc, char *argv[])
{
	uint32_t address;
	int type;

	g_arg0 = argv[0];
	if (1 == argc) {
		usage("null");
	}
	log_level_push(g_log_level_info_);
	address = 0;
	type = 0;
	while (argc > g_argind) {
		char const *str;

		if (arg_match(argc, argv, 'h', "help", NULL)) {
			usage("null");
		} else if (arg_match(argc, argv, 'v', "verbosity", &str)) {
			struct LogLevel const *l;

			if (0 == strcmp(str, "0") ||
			    0 == strcmp(str, "info")) {
				l = g_log_level_info_;
			} else if (0 == strcmp(str, "1") ||
			    0 == strcmp(str, "verbose")) {
				l = g_log_level_verbose_;
			} else if (0 == strcmp(str, "2") ||
			    0 == strcmp(str, "debug")) {
				l = g_log_level_debug_;
			} else if (0 == strcmp(str, "3") ||
			    0 == strcmp(str, "spam")) {
				l = g_log_level_spam_;
			} else {
				log_die(LOGL, "Invalid log-level '%s'.", str);
			}
			log_level_pop();
			log_level_push(l);
#ifdef SICY_CAEN
		} else if (arg_match(argc, argv, 'c', "caen", &str)) {
			char buf[64];
			char *p, *p2;
			char const *board_type;
			char link_ip[32] = "";
			int link_number = -1;
			int conet_node = -1;

			strlcpy_(buf, str, sizeof buf);
			p = buf;
			/* Parse and check board type. */
			p2 = strsep_(&p, ",");
			board_type = p2;
			if (!map_caen_type_exists(board_type)) {
				size_t i;

				log_error(LOGL, "CAEN controller '%s' not "
				    "supported, the following are:",
				    board_type);
				for (i = 0;; ++i) {
					char const *name;

					name = map_caen_type_get(i);
					if (NULL == name) {
						break;
					}
					log_error(LOGL, " %s", name);
				}
				log_die(LOGL, "Bailing.");
			}
			/* Parse link/ip. */
			p2 = strsep_(&p, ",");
			if (NULL != p2) {
				char *end;

				link_number = strtol(p2, &end, 10);
				if (p2 == end || '\0' != *end) {
					link_number = -1;
					strlcpy_(link_ip, p2, sizeof link_ip);
				}
			}
			/* Parse conet node. */
			p2 = strsep_(&p, ",");
			if (NULL != p2) {
				char *end;

				conet_node = strtol(p2, &end, 10);
				if (p2 == end || '\0' != *end) {
					log_die(LOGL, "Invalid CAEN "
					    "conet-node in '%s'.", str);
				}
			}
			LOGF(info)(LOGL, "CAEN type=%s link_ip=%s "
			    "link_number=%d conet_node=%d.",
			    board_type, link_ip, link_number, conet_node);

			map_caen_type_set(board_type, link_ip, link_number,
			    conet_node);
#endif
		} else if (arg_match(argc, argv, 'l', "list", NULL)) {
			size_t i;

			LOGF(info)(LOGL, "Module type names for -t:");
			for (i = 0; KW_NONE != c_module_list_[i].type; ++i) {
				LOGF(info)(LOGL, " %s", keyword_get_string(
				    c_module_list_[i].type));
			}
			exit(EXIT_SUCCESS);
		} else if (arg_match(argc, argv, 't', "type", &str)) {
			size_t i;

			for (i = 0;; ++i) {
				if (KW_NONE == c_module_list_[i].type) {
					usage("Unknown module type.");
				}
				type = c_module_list_[i].type;
				if (0 == strcmp(str,
				    keyword_get_string(type))) {
					break;
				}
			}
			LOGF(info)(LOGL, "Type %s=%d.", str, type);
		} else if (arg_match(argc, argv, 'a', "addr", &str)) {
			char const *p;
			unsigned base;

			if (0 == strncmp(str, "0x", 2)) {
				p = str + 2;
				base = 16;
			} else {
				p = str;
				base = 10;
			}
			address = strtoi32(p, NULL, base);
			LOGF(info)(LOGL, "Address=0x%08x.", address);
		} else if (arg_match(argc, argv, 'd', "register-dump", NULL))
		{
			struct ModuleProps props;
			struct Module module;
			struct PackerList packer_list;
			struct DatagramArray dgram_array;
			struct CtrlRegisterArray register_array;
			struct Packer packer;
			struct PackerNode *p;
			size_t dgram_array_i;

			if (0 == type) {
				usage("Missing module type.");
			}
			if (0 == address) {
				usage("Missing address.");
			}
			ZERO(props);
			ZERO(module);
			module.type = type;
			props.get_map = get_map;
			module.props = &props;
			map_setup();
			/* TODO: Don't know what to poke here. */
			g_map = map_map(address, 0x10000, KW_NOBLT, 0, 0,
			    0, 0, 0, 0, 0, 0, 0);
			TAILQ_INIT(&packer_list);
			module_register_list_pack(&packer_list, &module, 0);
			/* Convert packer list to dgram array. */
			dgram_array.num = 0;
			TAILQ_FOREACH(p, &packer_list, next) {
				++dgram_array.num;
			}
			MALLOC(dgram_array.array, dgram_array.num);
			dgram_array.num = 0;
			TAILQ_FOREACH(p, &packer_list, next) {
				COPY(dgram_array.array[dgram_array.num],
				    p->dgram);
				dgram_array.array[dgram_array.num].bytes =
				    p->packer.ofs;
				++dgram_array.num;
			}
			packer_list_free(&packer_list);
			dgram_array_i = -1;
			if (ctrl_client_register_array_unpack(
			    &register_array, &dgram_array, &dgram_array_i,
			    &packer)) {
				ctrl_client_register_array_print(
				    &register_array);
			}
			FREE(dgram_array.array);
			ctrl_client_register_array_free(&register_array);
			map_unmap(&g_map);
			map_deinit();
			map_shutdown();
		} else if (arg_match(argc, argv, 'r', "raw-read", &str)) {
			struct Map *map;
			char *end;
			unsigned int bits;

			bits = strtol(str, &end, 10);
			if (16 != bits && 32 != bits && '\0' != *end) {
				usage("Invalid raw-read option \"%s\".", str);
			}
			map_setup();
			map = map_map(address, sizeof(uint32_t), KW_NOBLT, 0,
			    0,
			    3, 0, bits, 0, 0, 0, 0);
			LOGF(info)(LOGL, "Raw-read.");
			switch (bits) {
			case 16:
				 LOGF(info)(LOGL, "Value=0x%04x.",
				     map_sicy_read(map, 3, 16, 0));
				 break;
			case 32:
				 LOGF(info)(LOGL, "Value=0x%08x.",
				     map_sicy_read(map, 3, 32, 0));
				 break;
			}
			map_unmap(&map);
			map_deinit();
			map_shutdown();
		} else if (arg_match(argc, argv, 'w', "raw-write", &str)) {
			struct Map *map;
			char *end;
			uint32_t value;
			unsigned int bits;

			bits = strtol(str, &end, 10);
			if (16 != bits && 32 != bits && ',' != *end) {
				usage("Invalid raw-write option \"%s\".",
				    str);
			}
			str = end + 1;
			if (0 == strncmp(str, "0x", 2)) {
				str += 2;
				value = strtol(str, &end, 16);
			} else {
				value = strtol(str, &end, 10);
			}
			if (end == str) {
				usage("Invalid raw-write option \"%s\".",
				    str);
			}
			map_setup();
			map = map_map(address, sizeof(uint32_t), KW_NOBLT, 0,
			    0,
			    0, 0, 0, 4, 0, bits, 0);
			LOGF(info)(LOGL, "Raw-write.");
			switch (bits) {
			case 16:
				map_sicy_write(map, 4, 16, 0, value);
				break;
			case 32:
				map_sicy_write(map, 4, 32, 0, value);
				break;
			}
			LOGF(info)(LOGL, "Done.");
			map_unmap(&map);
			map_deinit();
			map_shutdown();
		} else {
			usage("Weird argument \"%s\".", argv[g_argind]);
		}
	}
	return 0;
}
