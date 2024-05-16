/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2020-2021, 2023-2024
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

#ifndef MODULE_REGGEN2_H
#define MODULE_REGGEN2_H

#include <stdlib.h>
#include <util/funcattr.h>
#include <util/queue.h>
#include <util/stdint.h>

TAILQ_HEAD(Registers, Register);
TAILQ_HEAD(Blocks, Block);

struct Register {
	TAILQ_ENTRY(Register)	tailq;
	char	*name;
	char	*access;
	uint32_t	address;
	uint8_t	bits;
	uint8_t	n;
	struct	Block *block;
};
struct Block {
	TAILQ_ENTRY(Block)	tailq;
	char	*prefix;
	uint8_t	repeat;
	uint32_t	offset1;
	uint32_t	offset2;
	uint32_t	offset3;
	struct	Registers regs;
};

void register_add(const char *, uint32_t, uint8_t, const char *);
void register_block_start(char *, uint8_t, uint32_t, uint32_t, uint32_t);
void register_block_end(void);
uint32_t register_calc_offset(int, uint32_t);
void register_dump(const char*, const char*);
void register_dump_header(const char*);
void register_dump_struct(void);
void register_dump_struct_init(void);
void register_dump_init(const char*);
void register_init(void);
void register_sort(void);

#endif
