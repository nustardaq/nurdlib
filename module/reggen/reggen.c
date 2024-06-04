/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2020-2024
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
/* Some platforms need stdarg to declare vfprintf and similar. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <module/map/internal.h>
#include <nurdlib/base.h>
#include <util/fmtmod.h>
#include <util/queue.h>
#include <util/stdint.h>
#include <util/string.h>

#define LINE_SIZE 80
#define NAME_SIZE 80
#define PATH_SIZE 256

TAILQ_HEAD(EntryList, Entry);
struct Entry {
	uint32_t	addr;
	uint32_t	array_length;
	char	name[NAME_SIZE];
	unsigned	bits;
	unsigned	mod;
	unsigned	byte_step;
	TAILQ_ENTRY(Entry)	next;
};

static int	addr_cmp(void const *, void const *);
static void	die(char const *, ...) FUNC_PRINTF(1, 2) FUNC_NORETURN;
static void	my_strcat(char *, size_t, char const *);
static void	my_strcpy(char *, size_t, char const *, size_t);
static void	print_list_client(FILE *);
static void	print_list_server(FILE *);
static void	print_offsets(FILE *);
static void	usage(void);

static char *g_argv0;
static char *g_path_out;
static char *g_path_in;
static struct EntryList g_entry_list = TAILQ_HEAD_INITIALIZER(g_entry_list);
static size_t g_entry_num;
static struct Entry const **g_map;

int
addr_cmp(void const *a_1, void const *a_2)
{
	struct Entry const *const *e_1;
	struct Entry const *const *e_2;
	uint32_t addr1, addr2;

	e_1 = a_1;
	e_2 = a_2;
	addr1 = (*e_1)->addr;
	addr2 = (*e_2)->addr;
	return addr1 < addr2 ? -1 : addr1 > addr2 ? 1 : 0;
}

void
die(char const *a_fmt, ...)
{
	va_list args;

	fprintf(stderr, "%s out=\"%s\" in=\"%s\": ",
	    g_argv0, g_path_out, g_path_in);
	va_start(args, a_fmt);
	vfprintf(stderr, a_fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(EXIT_FAILURE);
}

void
my_strcat(char *a_dst, size_t a_dst_bytes, char const *a_src)
{
	size_t dst_len, src_len;

	dst_len = strlen(a_dst);
	src_len = strlen(a_src);
	if (dst_len + src_len >= a_dst_bytes) {
		die("strcat with huge strings (dst=%"PRIz"+src=%"PRIz" >= "
		    "max=%"PRIz"), I give up.", dst_len, src_len,
		    a_dst_bytes);
	}
	strlcat(a_dst, a_src, a_dst_bytes);
}

void
my_strcpy(char *a_dst, size_t a_dst_bytes, char const *a_src, size_t
    a_src_bytes)
{
	size_t len;

	len = 0 != a_src_bytes ? a_src_bytes : strlen(a_src);
	if (len >= a_dst_bytes) {
		die("strcpy with huge strings (len=%"PRIz" >= max=%"PRIz"), "
		    "I give up.", len, a_dst_bytes);
	}
	strlcpy(a_dst, a_src, len + 1);
}

void
print_list_client(FILE *a_file)
{
	size_t i;

	for (i = 0; g_entry_num > i; ++i) {
		struct Entry const *entry;

		entry = g_map[i];
		if (0 != (MAP_MOD_R & entry->mod)) {
			fprintf(a_file, "\t{0x%08x, %2u, %2u, \"%s\"},\n",
			    entry->addr, entry->array_length, entry->bits,
			    entry->name);
		}
	}
	fprintf(a_file, "\t{0x00000000,  0,  0, NULL}\n");
}

void
print_list_server(FILE *a_file)
{
	size_t i;

	for (i = 0; g_entry_num > i; ++i) {
		struct Entry const *entry;

		entry = g_map[i];
		if (0 != (MAP_MOD_R & entry->mod)) {
			fprintf(a_file, "\t{0x%08x, %2u, %2u, %u},\n",
			    entry->addr, entry->array_length, entry->bits,
			    entry->byte_step);
		}
	}
	fprintf(a_file, "\t{0x00000000,  0,  0, 0}\n");
}

void
print_offsets(FILE *a_file)
{
	size_t i;
	uint32_t tot_size;

	tot_size = 0;
	for (i = 0; g_entry_num > i; ++i) {
		struct Entry const *entry;
		char const *index_suff = "";
		char const *index_val = "0";
		uint32_t len, size;

		entry = g_map[i];

		if (0 < entry->array_length) {
			index_suff = "(i)";
			index_val = "i";
			len = entry->array_length;
		} else {
			len = 1;
		}
		fprintf(a_file, "#define MOD_%s%s %u\n",
		    entry->name, index_suff,
		    entry->mod);
		fprintf(a_file, "#define BITS_%s%s %u\n",
		    entry->name, index_suff,
		    entry->bits);
		fprintf(a_file, "#define  LEN_%s%s %u\n",
		    entry->name, index_suff,
		    entry->array_length);
		fprintf(a_file, "#define  OFS_%s%s "
		    "(0x%x + %s * %u)\n",
		    entry->name, index_suff,
		    entry->addr,
		    index_val,
		    entry->byte_step);

		size = entry->addr + len * entry->bits / 8;
		tot_size = MAX(tot_size, size);
	}
	fprintf(a_file, "#define MAP_SIZE 0x%x\n", tot_size);
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s <out-path> <in-path>.\n", g_argv0);
	fprintf(stderr, " out-path  "
	    "directory to put generated stuff in.\n");
	fprintf(stderr," in-path   "
	    "\"module/<module_name>/register_list[suffix].txt\".\n");
	fprintf(stderr, "Creates offsets[suffix].txt and "
	    "registerlist[suffix].c in out-path.\n");
}

int
main(int argc, char **argv)
{
	char path_final[PATH_SIZE];
	char path_h[PATH_SIZE];
	char path_rl[PATH_SIZE];
	char line[LINE_SIZE];
	char name_camel[NAME_SIZE];
	char name_lower[NAME_SIZE];
	char name_upper[NAME_SIZE];
	char suffix[NAME_SIZE];
	FILE *file;
	struct Entry *entry;
	char const *mod_name;
	char const *filename;
	char *p, *pc, *pl, *pu;
	size_t name_len, len;
	ssize_t idx_addr, idx_bits, idx_mod, idx_name, idx_step;
	size_t idx_ofs[5];
	unsigned idx_num, i;
	int camel_up, line_no;

	if (3 != argc) {
		usage();
		die("Missing arguments.");
	}
	g_argv0 = argv[0];
	g_path_out = argv[1];
	g_path_in = argv[2];

	/* Extract module name from path to camel and upper cases. */
	if (0 != strncmp("module/", g_path_in, 7)) {
		usage();
		die("in-path must start with \"module/\".");
	}
	mod_name = g_path_in + 7;
	filename = strstr(mod_name, "/register_list");
	if (NULL == filename) {
		usage();
		die("in-path filename base must be \"register_list\".");
	}
	name_len = filename - mod_name;
	if (NAME_SIZE <= name_len) {
		die("in-path module name too big.");
	}
	my_strcpy(name_lower, sizeof name_lower, mod_name, name_len);
	pc = name_camel;
	pl = name_lower;
	pu = name_upper;
	camel_up = 1;
	for (; '\0' != *pl; ++pl) {
		int c, l, u;

		c = *pl;
		if (!isalnum(c) && '_' != c) {
			usage();
			die("in-path has a weird module name.");
		}
		/* tolower is more common than islower! */
		l = tolower(c);
		if (c != l) {
			usage();
			die("in-path must be all lower-case.");
		}
		u = toupper(c);
		if ('_' != c) {
			*pc++ = camel_up ? u : l;
			camel_up = 0;
		} else {
			camel_up = 1;
		}
		*pu++ = u;
	}
	*pc = '\0';
	*pu = '\0';
	/* Continue to extract the suffix. */
	len = strlen(filename);
	if (0 != strcmp(filename + len - 4, ".txt")) {
		usage();
		die("in-path filename must end with .txt.");
	}
	if (18 == len) {
		suffix[0] = '\0';
	} else {
		my_strcpy(suffix, sizeof suffix, filename + 14, len - 18);
	}

	/* Open register list. */
	file = fopen(g_path_in, "rb");
	if (NULL == file) {
		die("Could not open in-file.");
	}

	/* Figure out format. */
	idx_num = 0;
	if (NULL == fgets(line, sizeof line, file)) {
		fclose(file);
		die("%s: Error reading file.", g_path_in);
	}
#define OFS_GET(ofs, c)\
	do { \
		p = strchr(line, c);\
		if (NULL == p) {\
			die("%s:1: Missing '%c' in format \"%s\".", \
			    g_path_in, c, line);\
		}\
		ofs = p - line;\
		if (4 < ofs) {\
			die("%s:1: Corrupt format \"%s\".", \
			    g_path_in, line);\
		}\
		++idx_num;\
	} while (0)
#define OFS_GET_OPT(ofs, c)\
	do { \
		p = strchr(line, c);\
		if (NULL != p) {\
			ofs = p - line;\
			if (4 < ofs) {\
				die("%s:1: Corrupt format \"%s\".", \
				    g_path_in, line);\
			}\
			++idx_num;\
		}\
	} while (0)
	OFS_GET(idx_addr, 'a');
	OFS_GET(idx_bits, 'b');
	OFS_GET(idx_mod , 'm');
	OFS_GET(idx_name, 'n');
	idx_step = -1;
	OFS_GET_OPT(idx_step, 's');

	/* Read registers. */
	for (line_no = 2; fgets(line, sizeof line, file); ++line_no) {
		char name[NAME_SIZE];
		char *token[20];
		uint32_t length;
		uint32_t addr, addr_end, is_range;
		unsigned token_num;
		unsigned bits, mod, byte_step;

		/* Extract tokens. */
		p = line;
		token_num = 0;
		for (;;) {
			while (' ' == *p || '\t' == *p) {
				++p;
			}
			if ('\0' == *p || '\n' == *p) {
				break;
			}
			if (LENGTH(token) == token_num) {
				die("%s:%d: Too many tokens.",
				    g_path_in, line_no);
			}
			token[token_num++] = p;
			while (!isspace(*p)) {
				++p;
			}
			*p++ = '\0';
		}
		if (0 == token_num) {
			continue;
		}
		if (token_num < idx_num) {
			die("%s:%d: Expected at least %u tokens, got %u.",
			    g_path_in, line_no, idx_num, token_num);
		}

		/* Pack name tokens together via an index-map. */
		for (i = 0; i <= (unsigned)idx_name; ++i) {
			idx_ofs[i] = i;
		}
		for (; i < LENGTH(idx_ofs); ++i) {
			idx_ofs[i] = (token_num - idx_num) + i;
		}

		/* Address. */
		addr = strtol(token[idx_ofs[idx_addr]], &p, 0);
		if (p == token[idx_ofs[idx_addr]]) {
			die("%s:%d: Invalid address.", g_path_in, line_no);
		}
		is_range = 0;
		addr_end = 0;
		if (0 == strncmp("..", p, 2)) {
			p += 2;
			addr_end = strtol(p, NULL, 0);
			is_range = 1;
		}

		/* Bits. */
		bits = strtol(token[idx_ofs[idx_bits]], NULL, 0);
		if (bits <= 16) {
			bits = 16;
		} else if (bits <= 32) {
			bits = 32;
		} else {
			die("%s:%d: Cannot handle bitsizes > 32!",
			    g_path_in, line_no);
		}

		/* Access modifiers. */
		mod = 0;
		p = token[idx_ofs[idx_mod]];
		if (strstr(p, "R")) {
			mod |= MAP_MOD_R;
		}
		if (strstr(p, "r")) {
			if (MAP_MOD_R & mod) {
				die("%s:%d: Cannot have both 'R' and 'r' "
				    " register modifiers.",
				    g_path_in, line_no);
			}
			mod |= MAP_MOD_r;
		}
		if (strstr(p, "W")) {
			mod |= MAP_MOD_W;
		}

		/* Name. */
		name[0] = '\0';
		length = 0;
		for (i = idx_name; token_num > i &&
		    idx_ofs[idx_addr] != i &&
		    idx_ofs[idx_bits] != i &&
		    idx_ofs[idx_mod ] != i; ++i) {
			char const *rp;
			char *wp;

			if ((unsigned)idx_name != i) {
				my_strcat(name, sizeof name, "_");
			}
			rp = wp = token[i];
			while ('\0' != *rp) {
				char *end;
				int c;

				c = tolower(*rp++);
				switch (c) {
				case '/':
				case '-':
					*wp++ = '_';
					break;
				case '(':
				case ')':
					break;
				case '[':
					if (is_range) {
						die("%s:%d: Both length and "
						    "range.",
						    g_path_in, line_no);
					}
					length = strtol(rp, &end, 10);
					if (rp == end || ']' != *end) {
						die("%s:%d: Invalid array.",
						    g_path_in, line_no);
					}
					rp = end + 1;
					break;
				default:
					*wp++ = c;
					break;
				}
			}
			*wp = '\0';
			my_strcat(name, sizeof name, token[i]);
		}

		/* Step per entry. */
		byte_step = 0;
		if (-1 != idx_step) {
			byte_step = strtol(token[idx_ofs[idx_step]], NULL, 0);
		}
		if (0 == byte_step) {
			byte_step = bits / 8;
		}

		CALLOC(entry, 1);
		entry->addr = addr;
		if (is_range) {
			addr_end += bits / 8;
			entry->array_length = (addr_end - addr) / (bits / 8);
		} else {
			entry->array_length = length;
		}
		my_strcpy(entry->name, sizeof entry->name, name, 0);
		entry->bits = bits;
		entry->mod = mod;
		entry->byte_step = byte_step;
		TAILQ_INSERT_TAIL(&g_entry_list, entry, next);
		++g_entry_num;
	}
	fclose(file);

	/* Create indirection-map sorted by address. */
	CALLOC(g_map, g_entry_num);
	i = 0;
	TAILQ_FOREACH(entry, &g_entry_list, next) {
		g_map[i++] = entry;
	}
	qsort(g_map, g_entry_num, sizeof(intptr_t), addr_cmp);

	/* TODO: Check that no registers overlap. */

	/* Structs. */
	my_strcpy(path_h, sizeof path_h, g_path_out, 0);
	my_strcat(path_h, sizeof path_h, "/offsets");
	my_strcat(path_h, sizeof path_h, suffix);
	my_strcat(path_h, sizeof path_h, ".h.tmp");
	file = fopen(path_h, "wb");
	if (NULL == file) {
		die("%s: Could not open file.", path_h);
	}
	fprintf(file, "#ifndef MODULE_%s_OFFSETS_H\n", name_upper);
	fprintf(file, "#define MODULE_%s_OFFSETS_H\n", name_upper);
	fprintf(file, "\n");
	print_offsets(file);
	fprintf(file, "\n");
	fprintf(file, "#endif\n");
	fclose(file);

	/* Register lists code. */
	my_strcpy(path_rl, sizeof path_rl, g_path_out, 0);
	my_strcat(path_rl, sizeof path_rl, "/registerlist");
	my_strcat(path_rl, sizeof path_rl, suffix);
	my_strcat(path_rl, sizeof path_rl, ".c.tmp");
	file = fopen(path_rl, "wb");
	if (NULL == file) {
		die("%s: Could not open file.", path_rl);
	}
	fprintf(file, "#include <stdlib.h>\n");
	fprintf(file, "#include <module/reggen/registerlist.h>\n");
	fprintf(file, "\n");
	fprintf(file, "struct RegisterEntryClient const "
	    "c_%s_register_list_client_[] = {\n", name_lower);
	print_list_client(file);
	fprintf(file, "};\n");
	fprintf(file, "\n");
	fprintf(file, "struct RegisterEntryServer const "
	    "c_%s_register_list_server_[] = {\n", name_lower);
	print_list_server(file);
	fprintf(file, "};\n");
	fclose(file);

	/* Files done, move them to their final place. */
	strcpy(path_final, path_h);
	path_final[strlen(path_final) - 4] = '\0';
	rename(path_h, path_final);
	strcpy(path_final, path_rl);
	path_final[strlen(path_final) - 4] = '\0';
	rename(path_rl, path_final);

	return 0;
}
