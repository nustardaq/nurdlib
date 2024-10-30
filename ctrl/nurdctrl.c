/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctrl/ctrl.h>
#include <module/module.h>
#include <module/reggen/registerlist.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/argmatch.h>
#include <util/err.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/memcpy.h>
#include <util/pack.h>
#include <util/string.h>

static void	conn(void);
static void	crate_array_print(struct CtrlCrateArray const *);
static void	my_exit(void);
static void	usage(char const *, ...) FUNC_PRINTF(1, 2) FUNC_NORETURN;

static char const *g_argv0;
static char *g_host;
static uint16_t g_port;
static struct CtrlClient *g_client;

/* If not already 'connected' (udp...), connect to running nurdlib. */
void
conn(void)
{
	if (NULL != g_client) {
		return;
	}
	LOGF(verbose)(LOGL, "Connecting to %s:%d.", g_host, g_port);
	g_client = ctrl_client_create(g_host, g_port);
	if (NULL == g_client) {
		exit(EXIT_FAILURE);
	}
}

void
crate_array_print(struct CtrlCrateArray const *a_array)
{
	size_t i;

	for (i = 0; i < a_array->num; ++i) {
		struct CtrlCrate const *crate;
		size_t j;

		crate = &a_array->array[i];
		printf("Crate[%"PRIz"] = %s\n", i, crate->name);
		for (j = 0; j < crate->module_num; ++j) {
			struct CtrlModule const *module;
			size_t k;

			module = &crate->module_array[j];
			printf(" Module[%"PRIz"] = %s\n", j,
			    keyword_get_string(module->type));
			for (k = 0; k < module->submodule_num; ++k) {
				struct CtrlSubModule const *submodule;

				submodule = &module->submodule_array[k];
				printf("  Sub-module[%"PRIz"] = %s\n", k,
				    keyword_get_string(submodule->type));
			}
		}
	}
}

/* Clean up potential connection. */
void
my_exit(void)
{
	if (NULL != g_client) {
		ctrl_client_free(&g_client);
		LOGF(verbose)(LOGL, "Closed client.");
	}
}

void
usage(char const *a_fmt, ...)
{
	FILE *str;
	int exit_code;

	/* Some gcc:s don't like null or empty format strings. */
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
#define P fprintf(str,
#define Q "\n");
P"Usage: %s <options>%s", g_argv0, Q
P"  -h, --help                    Print this help and exit."Q
P"  -v, --verbosity=log-level     Log-level, number or name:"Q
P"                                 0 = info"Q
P"                                 1 = verbose"Q
P"                                 2 = debug"Q
P"                                 3 = spam"Q
P"  -a, --addr=host               Talk to given host, default=localhost:"
    STRINGIFY_VALUE(CTRL_DEFAULT_PORT)"."Q
P"                                Example: -a localhost, -a 1.1.1.1:11111."Q
P"  -D, --config-dump             Dump config for host."Q
P"  -s, --spec=(print|i[,j[,k]])  'print' = print crate tree with modules,"Q
P"                                much smaller than the full config dump,"Q
P"                                used to index modules for other commands."Q
P"                                i = crate index."Q
P"                                j = module index."Q
P"                                k = sub-module index."Q
P"                                Examples:"Q
P"                                -s print"Q
P"                                -s 0,1"Q
P"  -C, --crate-info              Show various fun info."Q
P"  -c, --config str              Write config to module given with -s."Q
P"                                '-' to read from stdin."Q
P"                                Limited to 256 bytes because reasons."Q
P"  -d, --register-dump           Register dump for module given by -s."Q
P"  -g, --goc=r,sfp,card,ofs[,num]"Q
P"           =w,sfp,card,ofs[,value[,num]]"Q
P"                                Reads/writes via GSI Pexor/Kinpex"Q
P"                                like 'gosipcmd'. Examples:"Q
P"                                -g r,0,0,0x8"Q
P"                                -g r,0,0,0x8,32"Q
P"                                -g w,0,0,0x8,1"Q
P"                                -g w,0,0,0x8,1,32"Q
P"  -m, --module=ofs0:bits0[:val0][,ofsN:bitsN[:valN]...]"Q
P"                                Reads/writes register in module given by"Q
P"                                -s. Only offset+bits pairs listed for a"Q
P"                                module will be accessed. Examples:"Q
P"                                -m 0x1000:16"Q
P"                                -m 0x1000:32:1"Q
P"                                -m 0x1000:16,0x1008:32:1"Q
#undef P
#undef Q
	exit(exit_code);
}

int
main(int argc, char **argv)
{
	int crate_i, module_j, submodule_k;

	g_argv0 = argv[0];
	if (1 == argc) {
		usage("null");
	}

	g_client = NULL;
	atexit(my_exit);

	g_host = strdup_("127.0.0.1");
	g_port = CTRL_DEFAULT_PORT;

	log_level_push(g_log_level_info_);

	crate_i = -1;
	module_j = -1;
	submodule_k = -1;
	while (argc > g_argind) {
		char const *str;

		if (arg_match(argc, argv, 'h', "help", NULL)) {
			usage("null");
		} else if (arg_match(argc, argv, 'v', "verbose", &str)) {
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
		} else if (arg_match(argc, argv, 'a', "addr", &str)) {
			char const *p;
			char *end;

			p = strchr(str, ':');
			FREE(g_host);
			if (NULL == p) {
				g_host = strdup_(str);
				g_port = CTRL_DEFAULT_PORT;
			} else {
				unsigned len;

				len = p - str;
				g_host = malloc(len + 1);
				memcpy_(g_host, str, len);
				g_host[len] = '\0';
				g_port = strtol(p + 1, &end, 10);
				if (p + 1 == end) {
					usage("Invalid address '%s'.", str);
				}
			}
		} else if (arg_match(argc, argv, 'D', "crate-dump", NULL)) {
			struct CtrlConfigList list;

			LOGF(verbose)(LOGL, "Getting config dump.");
			conn();
			TAILQ_INIT(&list);
			if (ctrl_client_config_dump(g_client, &list)) {
				ctrl_config_dump_print(&list, 0);
				ctrl_config_dump_free(&list);
			}
		} else if (arg_match(argc, argv, 's', "spec", &str)) {
			if (0 == strcmp(str, "print")) {
				struct CtrlCrateArray array;

				LOGF(verbose)(LOGL, "Getting crate array.");
				conn();
				if (ctrl_client_crate_array_get(g_client,
				    &array)) {
					crate_array_print(&array);
					ctrl_client_crate_array_free(&array);
				}
			} else {
				char const *p;
				char *end;
				unsigned i;

				crate_i = -1;
				module_j = -1;
				submodule_k = -1;
				p = str;
				for (i = 0;; ++i) {
					int value;

					value = strtol(p, &end, 10);
					if (0 > value || 255 < value ||
					    p == end) {
						usage("Invalid syntax '%s'.",
						    str);
					}
					switch (i) {
					case 0: crate_i = value; break;
					case 1: module_j = value; break;
					case 2: submodule_k = value; break;
					}
					p = end;
					if ('\0' == *p) {
						break;
					}
					if (3 == i || ',' != *p) {
						usage("Invalid syntax '%s'.",
						    str);
					}
					++p;
				}
			}
		} else if (arg_match(argc, argv, 'C', "crate-info", NULL)) {
			struct CtrlCrateInfo crate_info;

			LOGF(verbose)(LOGL, "Getting crate info.");
			if (-1 == crate_i) {
				usage("Please specify the crate to get info "
				    "from!");
			}
			conn();
			if (ctrl_client_crate_info_get(g_client, &crate_info,
			    crate_i)) {
				printf("Events-override..: %u\n",
				    crate_info.event_max_override);
				printf("DT release.......: %s\n",
				    crate_info.dt_release ? "yes" : "no");
				printf("ACVT.............: %u\n",
				    crate_info.acvt);
				printf("Shadow buf bytes.: %u\n",
				    crate_info.shadow.buf_bytes);
				printf("Shadow fill bytes: %u\n",
				    crate_info.shadow.max_bytes);
			}
		} else if (arg_match(argc, argv, 'c', "config", &str)) {
			char buf[256];
			struct ConfigBlock *block;
			char const *snippet;

			LOGF(verbose)(LOGL, "Writing config snippet.");
			if (-1 == crate_i || -1 == module_j) {
				usage("Please specify the crate and module "
				    "to configure!");
				/*
				 * It's a bit tricky to check if we need the
				 * sub-module ID here, let's leave it for the
				 * server.
				 */
			}

			/*
			 * Don't allow huge configs via this interface, so a
			 * fixed buffer is fine.
			 */
			if (0 == strcmp(str, "-")) {
				ssize_t ret;

				ret = read(STDIN_FILENO, buf, sizeof buf - 1);
				if (-1 == ret) {
					warn_("-c read(stdin)");
					continue;
				}
				buf[ret] = '\0';
				snippet = buf;
			} else {
				snippet = str;
			}
			/* Wastes 1 byte, whatever. */
			if (strlen(snippet) >= sizeof buf - 1) {
				usage("Config too big!");
			}

			/* Do tricky parsing in this client-side process. */
			block = config_snippet_parse(snippet);
			conn();
			ctrl_client_config(g_client, crate_i, module_j,
			    submodule_k, block);
			config_snippet_free(&block);
		} else if (arg_match(argc, argv, 'd', "register-dump", NULL))
		{
			struct CtrlRegisterArray register_array;

			LOGF(verbose)(LOGL, "Getting module register dump.");
			if (-1 == crate_i || -1 == module_j) {
				usage("Please specify the crate and module "
				    "to register-dump!");
				/*
				 * Again we don't know if we need the
				 * sub-module ID.
				 */
			}
			conn();
			if (ctrl_client_register_array_get(g_client,
			    &register_array, crate_i, module_j, submodule_k))
			{
				ctrl_client_register_array_print(
				    &register_array);
				ctrl_client_register_array_free(
				    &register_array);
			}
		} else if (arg_match(argc, argv, 'g', "goc", &str)) {
			uint32_t args[5];
			size_t argn;
			char const *p;
			unsigned num;
			int do_read;

			LOGF(verbose)(LOGL, "Performing goc.");
			p = str;
			if (('r' != p[0] && 'w' != p[0]) ||
			    ',' != p[1]) {
				usage("Invalid goc argument.");
			}
			do_read = 'r' == *p;
			p += 2;
			for (argn = 0;;) {
				char buf[32];
				char *end;
				char const *q;
				size_t len;

				q = strchr(p, ',');
				if (NULL == q) {
					q = p + strlen(p);
				}
				len = q - p;
				if (LENGTH(args) == argn) {
					usage("Too many goc arguments.");
				}
				if (sizeof buf <= len) {
					usage("Weird goc arguments.");
				}
				memcpy_(buf, p, len);
				buf[len] = '\0';
				args[argn++] = strtol(buf, &end, 0);
				if ('\0' != *end) {
					usage("Non-numeric goc arguments.");
				}
				if ('\0' == *q) {
					break;
				}
				p = q + 1;
			}
			if (do_read) {
				uint32_t *reads;

				if (3 == argn) {
					num = 1;
				} else if (4 == argn) {
					num = args[3];
				} else {
					usage("Too few goc arguments.");
				}
				LOGF(verbose)(LOGL, "goc read SFP=%u, "
				    "card=%u, offset=0x%x, num=%u.",
				    args[0], args[1], args[2], num);
				MALLOC(reads, num);
				conn();
				if (ctrl_client_goc_read(g_client, crate_i,
				    args[0], args[1], args[2], num, reads)) {
					unsigned i;

					for (i = 0; i < num; ++i) {
						printf("%3u: 0x%08x\n",
						    i, reads[i]);
					}
				}
				FREE(reads);
			} else {
				if (4 == argn) {
					num = 1;
				} else if (5 == argn) {
					num = args[4];
				} else {
					usage("Too few goc arguments.");
				}
				LOGF(verbose)(LOGL, "goc write SFP=%u, "
				    "card=%u, offset=0x%x, value=0x%x, "
				    "num=%u.",
				    args[0], args[1], args[2], args[3], num);
				conn();
				ctrl_client_goc_write(g_client, crate_i,
				    args[0], args[1], args[2], num, args[3]);
			}
		} else if (arg_match(argc, argv, 'm', "module", &str)) {
			struct CtrlModuleAccess arr[256];
			size_t arrn;
			char const *p;

			LOGF(verbose)(LOGL, "Accessing module.");
			p = str;
			for (arrn = 0; '\0' != *p; ++arrn) {
				char *end;

				arr[arrn].ofs = strtol(p, &end, 0);
				if (':' != *end) {
					usage("Missing bits.");
				}
				p = end + 1;
				arr[arrn].bits = strtol(p, &end, 0);
				if (':' == *end) {
					p = end + 1;
					arr[arrn].value = strtol(p, &end, 0);
					arr[arrn].do_read = 0;
				} else {
					arr[arrn].do_read = 1;
				}
				if ('\0' == *end) {
					p = end;
				} else if (',' == *end) {
					p = end + 1;
				} else {
					usage("Invalid access argument.");
				}
			}
			LOGF(verbose)(LOGL, "Module access.");
			conn();
			if (ctrl_client_module_access_get(g_client, crate_i,
			    module_j, submodule_k, arr, arrn)) {
				size_t i;

				for (i = 0; i < arrn; ++i) {
					if (arr[i].do_read) {
						printf("0x%08x:%u = 0x%08x\n",
						    arr[i].ofs,
						    arr[i].bits,
						    arr[i].value);
					}
				}
			}
		} else if (argc > g_argind) {
			usage("Weird argument \"%s\".", argv[g_argind]);
		}
	}

	return 0;
}
