/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2018, 2021-2022, 2024
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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <nurdlib/keyword.h>
#include <util/funcattr.h>
#include <util/stdint.h>

struct ConfigBlock;
struct ConfigUnit;
struct ScalarList;

void			config_load_without_global(char const *);
int			parse_file(char const *) FUNC_RETURNS;
void			parse_snippet(char const *);
void			parser_check_log_level(void);
void			parser_copy_scalar(struct ScalarList *, unsigned,
    unsigned);
void			parser_create_bitmask(struct ScalarList *);
struct ConfigBlock	*parser_create_block(void) FUNC_RETURNS;
void			parser_include_file(char const *, int);
void			parser_pop_block(void);
struct ScalarList	*parser_prepare_block(enum Keyword, char const *,
    int, int);
void			parser_push_block(void);
struct ScalarList	*parser_push_config(enum Keyword, int, char const *,
    int, int) FUNC_RETURNS;
void			parser_push_double(struct ScalarList *, unsigned,
    double);
void			parser_push_integer(struct ScalarList *, unsigned,
    int32_t);
void			parser_push_keyword(struct ScalarList *, unsigned,
    enum Keyword);
void			parser_push_range(struct ScalarList *, unsigned,
    unsigned, unsigned);
void			parser_push_simple_block(void);
void			parser_push_string(struct ScalarList *, unsigned, char
    const *);
void			parser_push_unit(struct ScalarList *, unsigned, struct
    ConfigUnit const *);
void			parser_scalar_list_clear(struct ScalarList *);
void			parser_src_get(struct ConfigBlock *, enum Keyword,
    char const **, int *, int *);

#endif
