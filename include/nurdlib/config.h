/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2022, 2024-2025
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

#ifndef NURDLIB_CONFIG_H
#define NURDLIB_CONFIG_H

#include <stdlib.h>
#include <nurdlib/keyword.h>
#include <util/funcattr.h>
#include <util/pack.h>
#include <util/queue.h>
#include <util/stdint.h>
#include <util/udp.h>

#define CONFIG_GET_DOUBLE_ARRAY(dst, block, name, unit, min, max) \
    config_get_double_array(dst, sizeof dst, block, name, unit, min, max)
#define CONFIG_GET_INT_ARRAY(dst, block, name, unit, min, max) \
    config_get_int_array(dst, sizeof dst, sizeof dst[0], block, name,\
	unit, min, max)
#define CONFIG_GET_UINT_ARRAY(dst, block, name, unit, min, max) \
    config_get_uint_array(dst, sizeof dst, sizeof dst[0], block, name,\
	unit, min, max)

#define CONFIG_GET_KEYWORD(block, name, keyword_list) \
    config_get_keyword_(block, name, LENGTH(keyword_list), keyword_list)
#define CONFIG_GET_KEYWORD_ARRAY(dst, block, name, keyword_table) \
    config_get_keyword_array_(dst, LENGTH(dst), block, name, \
	LENGTH(keyword_table), keyword_table)
#define CONFIG_GET_BLOCK_PARAM_KEYWORD(block, idx, keyword_list) \
    config_get_block_param_keyword_(block, idx, LENGTH(keyword_list),\
    keyword_list)
#define CONFIG_SET_INT_ARRAY(block, name, unit, src) \
    config_set_int_array_(block, name, unit, dst, sizeof dst, sizeof dst[0])
#define CONFIG_SET_UINT_ARRAY(block, name, unit, src) \
    config_set_uint_array_(block, name, unit, dst, sizeof dst, sizeof dst[0])

struct ConfigBlock;
struct ConfigUnit;

/* Type-safe, although not switch-safe, units with strict GCC flags. */
struct ConfigUnit {
	unsigned	idx;
};
extern struct ConfigUnit const *const CONFIG_UNIT_NONE;
extern struct ConfigUnit const *const CONFIG_UNIT_MHZ;
extern struct ConfigUnit const *const CONFIG_UNIT_KHZ;
extern struct ConfigUnit const *const CONFIG_UNIT_NS;
extern struct ConfigUnit const *const CONFIG_UNIT_PS;
extern struct ConfigUnit const *const CONFIG_UNIT_US;
extern struct ConfigUnit const *const CONFIG_UNIT_MS;
extern struct ConfigUnit const *const CONFIG_UNIT_S;
extern struct ConfigUnit const *const CONFIG_UNIT_V;
extern struct ConfigUnit const *const CONFIG_UNIT_MV;
extern struct ConfigUnit const *const CONFIG_UNIT_B;
extern struct ConfigUnit const *const CONFIG_UNIT_KIB;
extern struct ConfigUnit const *const CONFIG_UNIT_MIB;
extern struct ConfigUnit const *const CONFIG_UNIT_FC;

enum ConfigScalarType {
	CONFIG_SCALAR_EMPTY,
	CONFIG_SCALAR_DOUBLE,
	CONFIG_SCALAR_INTEGER,
	CONFIG_SCALAR_KEYWORD,
	CONFIG_SCALAR_RANGE,
	CONFIG_SCALAR_STRING
};
enum ConfigType {
	CONFIG_BLOCK,
	CONFIG_CONFIG
};

struct PackerList;

/* Big cheese configuration. */
void			config_auto_register(enum Keyword, char const *)
	FUNC_NONNULL(());
char const		*config_default_path_get(void) FUNC_RETURNS;
void			config_default_path_set(char const *)
	FUNC_NONNULL(());
void			config_dump(struct ConfigBlock const *);
void			config_load(char const *) FUNC_NONNULL(());
int			config_merge(struct ConfigBlock *, struct Packer *)
	FUNC_NONNULL((2)) FUNC_RETURNS;
void			config_pack_children(struct PackerList *, struct
    ConfigBlock const *) FUNC_NONNULL((1));
void			config_snippet_free(struct ConfigBlock **)
	FUNC_NONNULL(());
struct ConfigBlock	*config_snippet_parse(char const *) FUNC_NONNULL(())
	FUNC_RETURNS;
void			config_shutdown(void);
void			config_touched_assert(struct ConfigBlock *, int);

/* Getters. */
struct ConfigBlock		*config_get_block(struct ConfigBlock *, enum
    Keyword) FUNC_RETURNS;
enum Keyword			config_get_block_name(struct ConfigBlock const
    *) FUNC_NONNULL(()) FUNC_RETURNS;
struct ConfigBlock		*config_get_block_next(struct ConfigBlock *,
    enum Keyword) FUNC_NONNULL(()) FUNC_RETURNS;

int				config_get_block_param_exists(struct
    ConfigBlock const *, unsigned) FUNC_NONNULL(()) FUNC_RETURNS;
int32_t				config_get_block_param_int32(struct
    ConfigBlock const *, unsigned) FUNC_NONNULL(()) FUNC_RETURNS;
uint32_t			config_get_block_param_uint32(struct
    ConfigBlock const *, unsigned) FUNC_NONNULL(()) FUNC_RETURNS;
enum Keyword			config_get_block_param_keyword_(struct
    ConfigBlock *, unsigned, size_t, enum Keyword const *) FUNC_NONNULL(())
FUNC_RETURNS;
char const			*config_get_block_param_string(struct
    ConfigBlock const *, unsigned) FUNC_NONNULL(()) FUNC_RETURNS;

uint32_t			config_get_bitmask(struct ConfigBlock *, enum
    Keyword, unsigned, unsigned) FUNC_RETURNS;
int				config_get_boolean(struct ConfigBlock *, enum
    Keyword) FUNC_RETURNS;
double				config_get_double(struct ConfigBlock *, enum
    Keyword, struct ConfigUnit const *, double, double) FUNC_NONNULL((3))
FUNC_RETURNS;
void				config_get_double_array(double *, size_t,
    struct ConfigBlock *, enum Keyword, struct ConfigUnit const *, double,
    double) FUNC_NONNULL((1, 5));
int32_t				config_get_int32(struct ConfigBlock *, enum
    Keyword, struct ConfigUnit const *, int32_t, int32_t) FUNC_NONNULL((3))
FUNC_RETURNS;
void				config_get_int_array(void *, size_t, size_t,
    struct ConfigBlock *, enum Keyword, struct ConfigUnit const *, int32_t,
    int32_t) FUNC_NONNULL((1, 6));
uint32_t			config_get_uint32(struct ConfigBlock *, enum
    Keyword, struct ConfigUnit const *, uint32_t, uint32_t) FUNC_NONNULL((3))
FUNC_RETURNS;
void				config_get_uint_array(void *, size_t, size_t,
    struct ConfigBlock *, enum Keyword, struct ConfigUnit const *, uint32_t,
    uint32_t) FUNC_NONNULL((1, 6));
enum Keyword			config_get_keyword_(struct ConfigBlock *, enum
    Keyword, size_t, enum Keyword const *) FUNC_NONNULL((4)) FUNC_RETURNS;
void				config_get_keyword_array_(enum Keyword *,
    size_t, struct ConfigBlock *, enum Keyword, size_t, enum Keyword const *)
	FUNC_NONNULL((1, 6));
void				config_get_source(struct ConfigBlock const *,
    char const **, int *, int *);
char const			*config_get_string(struct ConfigBlock *, enum
    Keyword) FUNC_RETURNS;

void				config_set_bool(struct ConfigBlock *, enum
    Keyword, int) FUNC_NONNULL(());
void				config_set_int32(struct ConfigBlock *, enum
    Keyword, struct ConfigUnit const *, int32_t) FUNC_NONNULL(());
void				config_set_int_array_(struct ConfigBlock *,
    enum Keyword, struct ConfigUnit const *, void const *, size_t, size_t)
	FUNC_NONNULL(());
void				config_set_uint32(struct ConfigBlock *, enum
    Keyword, struct ConfigUnit const *, uint32_t) FUNC_NONNULL(());
void				config_set_uint_array_(struct ConfigBlock *,
    enum Keyword, struct ConfigUnit const *, void const *, size_t, size_t)
	FUNC_NONNULL(());

#endif
