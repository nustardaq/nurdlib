/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019-2024
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
#include <stdio.h>
#include <stdlib.h>
#include <module/reggen2/reggen2.h>
#include <nurdlib/base.h>
#include <util/string.h>

#define KW_NONE "none"

/* #define DEBUG 1 */

static int	addr_cmp(void const *, void const *);
static void	str_to_upper(char *const *);

static const struct Register **g_map;
static int g_n_registers;
static struct Blocks g_blocks;
static struct Block *cur_block;
static int g_n_blocks;

void
str_to_upper(char *const *a_str)
{
	char *s = *a_str;
	for (; s < *a_str + strlen(*a_str); ++s) {
		*s = islower(*s) ? toupper(*s) : *s;
	}
}

int
addr_cmp(void const *a_a, void const *a_b)
{
	uint32_t a;
	uint32_t b;

	a = (*((struct Register const *const *)a_a))->address;
	b = (*((struct Register const *const *)a_b))->address;

	return a < b ? -1 : a > b ? 1 : 0;
}

void
register_init(void)
{
	TAILQ_INIT(&g_blocks);
	g_n_registers = 0;
}

void
register_add(const char *a_name, uint32_t a_address, uint8_t a_bits, const
    char *a_access)
{
	struct Register *reg;
	int i;
	char name[256];

	if (a_bits != 8 && a_bits != 16
	 && a_bits != 32 && a_bits != 64) {
		fprintf(stderr, "ERROR, bits must be 8, 16, 32 or 64.\n");
		abort();
	}

	for (i = 0; i < cur_block->repeat; ++i) {
		uint32_t address;

		snprintf_(name, sizeof name, "%s%s", cur_block->prefix,
		    a_name);
		address = register_calc_offset(i, a_address);

		CALLOC(reg, 1);
		reg->name = strdup_(name);
		reg->address = address;
		reg->bits = a_bits;
		reg->access = strdup_(a_access);
		reg->n = i;
		reg->block = cur_block;
#ifdef DEBUG
		printf("  Added register '%s/%u' (0x%08x) %u bits (%s)\n",
			reg->name, reg->n, reg->address,
			reg->bits, reg->access);
#endif
		TAILQ_INSERT_TAIL(&(cur_block->regs), reg, tailq);
		++g_n_registers;
	}
}

uint32_t
register_calc_offset(int i, uint32_t a_address)
{
	if (cur_block->offset2 == 0
			&& cur_block->offset3 == 0) {
		return a_address
			+ i * cur_block->offset1;
	} else if (cur_block->offset3 == 0) {
		return a_address
			+ (i>>1) * cur_block->offset1
			+ ((i%2 == 0) ? 0 : cur_block->offset2);
	} else {
		return a_address
			+ (i>>2) * cur_block->offset3
			+ ((i%4 < 2) ? 0 : cur_block->offset2)
			+ ((i%2 == 0) ? 0 : cur_block->offset1);
	}
}

void
register_block_start(
    char *a_prefix,
    uint8_t a_repeat,
    uint32_t a_offset1,
    uint32_t a_offset2,
    uint32_t a_offset3)
{
	struct Block* block;

	CALLOC(block, 1);
	cur_block = block;

	if (strcmp(a_prefix, KW_NONE) == 0) {
		block->prefix = "";
	} else {
		char *buf;
		size_t len;

		len = strlen(a_prefix) + 2;
		CALLOC(buf, len);
		snprintf_(buf, len, "%s_", a_prefix);
		block->prefix = buf;
	}

	block->repeat = a_repeat;
	block->offset1 = a_offset1;
	block->offset2 = a_offset2;
	block->offset3 = a_offset3;
	TAILQ_INIT(&(block->regs));

	TAILQ_INSERT_TAIL(&g_blocks, block, tailq);

	++g_n_blocks;
#ifdef DEBUG
	printf("Starting block '%s' rep = %u, offs = (0x%08x,0x%08x,0x%08x)\n",
			a_prefix, a_repeat, a_offset1,
			a_offset2, a_offset3);
#endif
}

void
register_block_end(void)
{
	cur_block = 0;
}

void
register_dump(const char *module_name, const char *dump_opt)
{
	register_sort();

	if (0 == strcmp(dump_opt, "--offsets")) {
		register_dump_header(module_name);
		register_dump_struct();
		register_dump_struct_init();
		printf("#endif\n");
		return;
	}

	if (0 == strcmp(dump_opt, "--functions")) {
		register_dump_init(module_name);
		return;
	}

	fprintf(stderr,"ERROR: Nothing dumped!\n");
	abort();

}

void
register_sort(void)
{
	int i = 0;
	const struct Register* reg;
	const struct Block* block;

	CALLOC(g_map, g_n_registers);
	TAILQ_FOREACH(block, &g_blocks, tailq) {
		TAILQ_FOREACH(reg, &(block->regs), tailq) {
			g_map[i++] = reg;
		}
	}
	qsort(g_map, g_n_registers, sizeof(size_t), addr_cmp);
}

void
register_dump_header(const char *module_name)
{
	char *name_upper = strdup_(module_name);
	str_to_upper(&name_upper);

	printf("#ifndef MODULE_%s_OFFSETS_H\n", name_upper);
	printf("#define MODULE_%s_OFFSETS_H\n", name_upper);
	printf("\n");
	printf("#include <util/stdint.h>\n");
	printf("\n");

	free(name_upper);
}

void
register_dump_struct(void)
{
	int i;
	const struct Register* reg;
	uint32_t addr_prev = 0;

	printf("/* Single registers. */\n");
	for (i = 0; i < g_n_registers; ++i) {
		char index_suff[10] = "";

		reg = g_map[i];
		if (addr_prev > reg->address) {
			fprintf(stderr, "ERROR: Addresses not in order!"
			    " (prev=%08x > curr=%08x)\n",
			    addr_prev, reg->address);
			abort();
		}

		if (reg->block->repeat > 1) {
			snprintf_(index_suff, sizeof index_suff, "_%u",
			    reg->n);
		}
		printf("#define MOD_%s%s 5\n",
		    reg->name, index_suff);
		printf("#define BITS_%s%s %u\n",
		    reg->name, index_suff, reg->bits);
		printf("#define  LEN_%s%s 1\n",
		    reg->name, index_suff);
		printf("#define  OFS_%s%s 0x%x\n",
		    reg->name, index_suff, reg->address);

		addr_prev = reg->address + reg->bits / 8;
	}
	printf("#define MAP_SIZE 0x%x\n", addr_prev);
	printf("\n");
}

void
register_dump_struct_init(void)
{
	const struct Register* reg;
	const struct Block* block;

	printf("/* Array registers. */\n");
	TAILQ_FOREACH(block, &g_blocks, tailq) {
		TAILQ_FOREACH(reg, &(block->regs), tailq) {
			if (block->repeat > 1 && reg->n == 0) {
				printf("#define MOD_%s(i) MOD_%s_0\n",
				    reg->name, reg->name);
				printf("#define BITS_%s(i) BITS_%s_0\n",
				    reg->name, reg->name);
				printf("#define  LEN_%s(i) %u\n",
				    reg->name, block->repeat);
				printf("#define  OFS_%s(i) ARR_%s[i]\n",
				    reg->name, reg->name);
				printf("extern uint32_t const ARR_%s[%u];\n",
				    reg->name, block->repeat);
			}
		}
	}
	printf("\n");
}

void
register_dump_init(const char *module_name)
{
	const struct Register* reg;
	const struct Block* block;

	printf("#include <module/%s/offsets.h>\n", module_name);
	printf("\n");
	printf("/* Array register offsets. */\n");

	TAILQ_FOREACH(block, &g_blocks, tailq) {
		TAILQ_FOREACH(reg, &(block->regs), tailq) {
			if (block->repeat > 1) {
				if (0 == reg->n) {
					printf(
					    "uint32_t const ARR_%s[%u] = {\n",
					    reg->name, block->repeat);
				}
				printf("\tOFS_%s_%u", reg->name, reg->n);
				if (reg->n + 1 < block->repeat) {
					printf(",");
				}
				printf("\n");
				if (reg->n + 1 == block->repeat) {
					printf("};\n");
				}
			}
		}
	}
}
