/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
 * Oliver Papst
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

#include <nurdlib/config.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <config/parser.h>
#include <ctrl/ctrl.h>
#include <util/assert.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/path.h>
#include <util/queue.h>
#include <util/string.h>
#include <nurdlib/base.h>
#include <nurdlib/log.h>

#define DEFAULT_PATH_SIZE 256
#define BLOCK_BUILDER_DEPTH 20
#define SNIPPET_SRC "<snippet>", 1, 1

TAILQ_HEAD(AutoConfigList, AutoConfig);
TAILQ_HEAD(ItemList, Item);
TAILQ_HEAD(PathList, Path);
TAILQ_HEAD(ScalarList, Scalar);

/*
 * An auto config is loaded when a specific type of block is parsed, i.e. a
 * block named "name" is filled with configs from "path".
 */
struct AutoConfig {
	enum	Keyword name;
	char	const *path;
	TAILQ_ENTRY(AutoConfig)	next;
};
/* Config or block, i.e. ConfigBlock is exactly this. */
struct Item {
	enum	ConfigType type;
	enum	Keyword name;
	char	const *src_path;
	int	src_line_no;
	int	src_col_no;
	struct	ScalarList scalar_list;
	struct	ItemList item_list;
	int	is_touched;
	TAILQ_ENTRY(Item)	next;
};
struct Path {
	TAILQ_ENTRY(Path)	next;
	char	path[1];
};
struct Scalar {
	enum	ConfigScalarType type;
	unsigned	vector_index;
	union {
		double	d;
		int32_t	i;
		enum	Keyword k;
		struct {
			unsigned	first;
			unsigned	last;
		} r;
		char	*s;
	} value;
	struct	ConfigUnit const *unit;
	/* Only the first scalar in a scalar list will hold a bitmask. */
	int	has_bitmask;
	uint32_t	bitmask;
	TAILQ_ENTRY(Scalar)	next;
};

static void		assert_src_path(struct Item const *, char const *,
    int, int);
struct Item		*block_find_matching(struct ItemList *, enum Keyword,
    struct ScalarList const *, char const *, int, int) FUNC_RETURNS;
static void		item_free(struct Item **);
static void		item_list_clear(struct ItemList *);
static struct ItemList	*item_list_get(void) FUNC_RETURNS;
static void		keyword_match(struct Item *, size_t, enum Keyword
    const *, enum Keyword);
static void		pack_item_list(struct PackerList *, struct ItemList
    const *);
static void		pack_scalar(struct PackerList *, struct Scalar const
    *);
static void		pack_scalar_list(struct PackerList *, struct
    ScalarList const *);
static struct ItemList	*parent_get(struct ConfigBlock *) FUNC_RETURNS;
static char const	*path_get_instance(char const *) FUNC_RETURNS;
static int		scalar_are_equal(struct Scalar const *, struct Scalar
    const *);
static void		scalar_list_dump(struct ScalarList const *);
static struct Scalar	*scalar_get(struct ScalarList *, unsigned)
	FUNC_RETURNS;
static void		touched_traverse(enum Keyword, struct ItemList const
    *, int);
static char const	*unit_dump(struct ConfigUnit const *) FUNC_RETURNS;
struct ConfigUnit const	*unit_from_id(unsigned) FUNC_RETURNS;
static double		unit_get_factor(struct ConfigUnit const *)
	FUNC_RETURNS;
static double		unit_get_mult(struct ConfigUnit const *, struct
    ConfigUnit const *) FUNC_RETURNS;
static int		unit_is_compatible(struct ConfigUnit const *, struct
    ConfigUnit const *);
static int		unpack_snippet(struct ConfigBlock *, struct Packer *,
    int) FUNC_RETURNS;
static int		unpack_snippet_scalar(struct Packer *, struct
    ScalarList *) FUNC_RETURNS;

/* TODO: Reduce the number of globals. */

static char *g_default_path;
static char *g_current_root_path = ".";
static struct AutoConfigList g_auto_config_list =
    TAILQ_HEAD_INITIALIZER(g_auto_config_list);
/* This is the config root. */
static struct ItemList g_item_list = TAILQ_HEAD_INITIALIZER(g_item_list);
static struct PathList g_path_list = TAILQ_HEAD_INITIALIZER(g_path_list);
/*
 * A block + params are parsed here before being matched to previous blocks,
 * in case we'd like to merge from several files.
 */
static struct Item *g_temp_block;
/* Block stack while parsing. */
static struct Item *g_block_builder[BLOCK_BUILDER_DEPTH];
static size_t g_block_builder_index;

#define MAKE_UNIT(name, NAME, value) \
static struct ConfigUnit g_config_unit_##name##_ = {value};\
struct ConfigUnit const *const CONFIG_UNIT_##NAME = &g_config_unit_##name##_
MAKE_UNIT(none, NONE, 0);
MAKE_UNIT(MHz, MHZ, 1);
MAKE_UNIT(kHz, KHZ, 2);
MAKE_UNIT(ns, NS, 3);
MAKE_UNIT(ps, PS, 4);
MAKE_UNIT(us, US, 5);
MAKE_UNIT(ms, MS, 6);
MAKE_UNIT(s, S, 7);
MAKE_UNIT(V, V, 8);
MAKE_UNIT(mV, MV, 9);
MAKE_UNIT(B, B, 10);
MAKE_UNIT(KiB, KIB, 11);
MAKE_UNIT(MiB, MIB, 12);

void
assert_src_path(struct Item const *a_l, char const *a_rp, int a_rl, int a_rc)
{
	if (0 == strcmp(a_l->src_path, a_rp)) {
		log_die(LOGL, "%s:%d:%d looks like a duplicate of %s:%d:%d, "
		    "copy-paste error?", a_l->src_path, a_l->src_line_no,
		    a_l->src_col_no, a_rp, a_rl, a_rc);
	}
}

void
config_auto_register(enum Keyword a_name, char const *a_path)
{
	struct AutoConfig *auto_config;

	TAILQ_FOREACH(auto_config, &g_auto_config_list, next) {
		if (a_name == auto_config->name &&
		    0 == strcmp(a_path, auto_config->path)) {
			log_die(LOGL, "Duplicate auto config %d='%s' '%s'.",
			    a_name, keyword_get_string(a_name), a_path);
		}
	}
	CALLOC(auto_config, 1);
	auto_config->name = a_name;
	auto_config->path = a_path;
	TAILQ_INSERT_TAIL(&g_auto_config_list, auto_config, next);
}

char const *
config_default_path_get(void)
{
	if (NULL == g_default_path) {
		char *env;

		env = getenv("NURDLIB_DEF_PATH");
		if (NULL == env) {
			env = "../nurdlib/cfg/default";
		}
		LOGF(info)(LOGL, "Will try default cfg path='%s', can be set "
		    "with NURDLIB_DEF_PATH.", env);
		g_default_path = strdup(env);
	}
	return g_default_path;
}

void
config_default_path_set(char const *a_path)
{
	LOGF(info)(LOGL, "Setting default config path '%s'.", a_path);
	FREE(g_default_path);
	g_default_path = strdup(a_path);
}

void
config_dump(struct ConfigBlock const *a_parent)
{
	struct Item const *parent, *item;
	struct ItemList const *item_list;

	parent = (struct Item const *)a_parent;
	if (NULL == parent) {
		LOGF(debug)(LOGL, "Block=<root> {");
		item_list = &g_item_list;
	} else {
		LOGF(debug)(LOGL, "Block=%s {",
		    keyword_get_string(parent->name));
		LOGF(debug)(LOGL, "Block-params {");
		scalar_list_dump(&parent->scalar_list);
		LOGF(debug)(LOGL, "Block-params }");
		item_list = &parent->item_list;
	}
	TAILQ_FOREACH(item, item_list, next) {
		switch (item->type) {
		case CONFIG_BLOCK:
			config_dump((struct ConfigBlock const *)item);
			break;
		case CONFIG_CONFIG:
			LOGF(debug)(LOGL, "Config=%s {",
			    keyword_get_string(item->name));
			scalar_list_dump(&item->scalar_list);
			LOGF(debug)(LOGL, "Config }");
			break;
		}
	}
	LOGF(debug)(LOGL, "Block }");
}

uint32_t
config_get_bitmask(struct ConfigBlock *a_parent, enum Keyword a_name, unsigned
    a_first, unsigned a_last)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (scalar->has_bitmask) {
				item->is_touched = 1;
				return scalar->bitmask & (0xffffffff <<
				    a_first) & (0xffffffff >> (31 - a_last));
			}
		}
	}
	log_die(LOGL, "Could not find bitmask config '%s'.",
	    keyword_get_string(a_name));
}

int
config_get_boolean(struct ConfigBlock *a_parent, enum Keyword a_name)
{
	enum Keyword const c_true_false[] = {KW_TRUE, KW_FALSE};

	return KW_TRUE == CONFIG_GET_KEYWORD(a_parent, a_name, c_true_false);
}

struct ConfigBlock *
config_get_block(struct ConfigBlock *a_parent, enum Keyword a_name)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_BLOCK == item->type &&
		    (KW_NONE == a_name || item->name == a_name)) {
			LOGF(debug)(LOGL, "Found block of type '%s'.",
			    keyword_get_string(item->name));
			item->is_touched = 1;
			return (void *)item;
		}
	}
	LOGF(debug)(LOGL, "Did not find block of type '%s'.",
	    keyword_get_string(a_name));
	return NULL;
}

enum Keyword
config_get_block_name(struct ConfigBlock const *a_block)
{
	return ((struct Item const *)a_block)->name;
}

struct ConfigBlock *
config_get_block_next(struct ConfigBlock *a_current, enum Keyword a_name)
{
	struct Item *item;

	item = (struct Item *)a_current;
	for (;;) {
		item = TAILQ_NEXT(item, next);
		if (TAILQ_END(item_list) == item) {
			break;
		}
		if (CONFIG_BLOCK == item->type &&
		    (KW_NONE == a_name || item->name == a_name)) {
			item->is_touched = 1;
			return (void *)item;
		}
	}
	return NULL;
}

int
config_get_block_param_exists(struct ConfigBlock const *a_parent, unsigned
    a_index)
{
	struct Item const *block;
	struct ScalarList const *param_list;
	struct Scalar *param;

	block = (void const *)a_parent;
	param_list = &block->scalar_list;
	TAILQ_FOREACH(param, param_list, next) {
		if (a_index == param->vector_index) {
			return 1;
		}
		if (a_index < param->vector_index) {
			break;
		}
	}
	return 0;
}

int32_t
config_get_block_param_int32(struct ConfigBlock const *a_parent, unsigned
    a_index)
{
	struct Item const *block;
	struct ScalarList const *param_list;
	struct Scalar *param;

	block = (void const *)a_parent;
	param_list = &block->scalar_list;
	TAILQ_FOREACH(param, param_list, next) {
		if (a_index == param->vector_index) {
			if (CONFIG_SCALAR_INTEGER != param->type) {
				log_die(LOGL, "Block '%s' (%s:%d:%d) param "
				    "'%u' not an integer.",
				    keyword_get_string(block->name),
				    block->src_path, block->src_line_no,
				    block->src_col_no, a_index);
			}
			return param->value.i;
		}
		if (a_index < param->vector_index) {
			break;
		}
	}
	log_die(LOGL, "Block '%s' (%s:%d:%d) does not have requested param "
	    "[%u].", keyword_get_string(block->name), block->src_path,
	    block->src_line_no, block->src_col_no, a_index);
}

enum Keyword
config_get_block_param_keyword_(struct ConfigBlock *a_parent, unsigned
    a_index, size_t a_keyword_num, enum Keyword const *a_keyword_list)
{
	struct Item *block;
	struct ScalarList *param_list;
	struct Scalar *param;
	unsigned i;

	block = (struct Item *)a_parent;
	param_list = &block->scalar_list;
	i = 0;
	TAILQ_FOREACH(param, param_list, next) {
		if (a_index == i) {
			if (CONFIG_SCALAR_KEYWORD != param->type) {
				log_die(LOGL, "Block '%s' (%s:%d:%d) param "
				    "index=%u not a keyword.",
				    keyword_get_string(block->name),
				    block->src_path, block->src_line_no,
				    block->src_col_no, a_index);
			}
			keyword_match(block, a_keyword_num, a_keyword_list,
			    param->value.k);
			return param->value.k;
		}
		++i;
	}
	log_die(LOGL, "Block '%s' (%s:%d:%d) has %u params, param %u "
	    "(0-base) requested.", keyword_get_string(block->name),
	    block->src_path, block->src_line_no, block->src_col_no, i,
	    a_index);
}

char const *
config_get_block_param_string(struct ConfigBlock const *a_parent, unsigned
    a_index)
{
	struct Item const *block;
	struct ScalarList const *param_list;
	struct Scalar *param;
	unsigned i;

	block = (struct Item const *)a_parent;
	param_list = &block->scalar_list;
	i = 0;
	TAILQ_FOREACH(param, param_list, next) {
		if (a_index == i) {
			if (CONFIG_SCALAR_STRING != param->type) {
				log_die(LOGL, "Block '%s' (%s:%d:%d) param "
				    "'%u' not a string.",
				    keyword_get_string(block->name),
				    block->src_path, block->src_line_no,
				    block->src_col_no, a_index);
			}
			return param->value.s;
		}
		++i;
	}
	log_die(LOGL, "Block '%s' (%s:%d:%d) has %u params, param %u "
	    "(0-base) requested.", keyword_get_string(block->name),
	    block->src_path, block->src_line_no, block->src_col_no, i,
	    a_index);
}

double
config_get_double(struct ConfigBlock *a_parent, enum Keyword a_name, struct
    ConfigUnit const *a_unit, double a_min, double a_max)
{
	struct ItemList *item_list;
	struct Item *item;
	double value;

	value = 0.0;
	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar const *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_DOUBLE == scalar->type) {
				value = scalar->value.d *
				    unit_get_mult(scalar->unit, a_unit);
				break;
			}
			if (CONFIG_SCALAR_INTEGER == scalar->type) {
				value = scalar->value.i *
				    unit_get_mult(scalar->unit, a_unit);
				break;
			}
		}
	}
	if (TAILQ_END(item_list) != item) {
		item->is_touched = 1;
		return MIN(MAX(value, a_min), a_max);
	}
	log_die(LOGL, "Could not find double config '%s'.",
	    keyword_get_string(a_name));
}

void
config_get_double_array(double *a_dst, size_t a_bytes, struct ConfigBlock
    *a_parent, enum Keyword a_name, struct ConfigUnit const *a_unit, double
    a_min, double a_max)
{
	struct ItemList *item_list;
	struct Item *item;
	size_t dst_len;

	ALIGN_CHECK(a_bytes, sizeof *a_dst);
	dst_len = a_bytes / sizeof *a_dst;
	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;
			double *p;
			size_t length;
			int i_prev;

			length = 0;
			i_prev = -1;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				if ((int)scalar->vector_index <= i_prev) {
					log_die(LOGL, "List items out of "
					    "order (prev=%d,cur=%d) for '%s' "
					    "(%s:%d:%d)!", i_prev,
					    scalar->vector_index,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				i_prev = scalar->vector_index;
				if (CONFIG_SCALAR_DOUBLE != scalar->type) {
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) not double!",
					    length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				if (!unit_is_compatible(a_unit, scalar->unit))
				{
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) has incompatible"
					    " unit %s (!=%s)!", length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no,
					    unit_dump(scalar->unit),
					    unit_dump(a_unit));
				}
				++length;
			}
			if (length != dst_len) {
				log_die(LOGL, "List '%s' (%s:%d:%d) has "
				    "srclen=%"PRIz" != user dstlen=%"PRIz"!",
				    keyword_get_string(item->name),
				    item->src_path, item->src_line_no,
				    item->src_col_no, length, dst_len);
			}

			p = a_dst;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				double value;

				value = scalar->value.d;
				if (CONFIG_UNIT_NONE != a_unit ||
				    CONFIG_UNIT_NONE != scalar->unit) {
					value *= unit_get_mult(scalar->unit,
					    a_unit);
				}
				value = MIN(MAX(value, a_min), a_max);
				*p++ = value;
			}

			item->is_touched = 1;
			return;
		}
	}
	log_die(LOGL, "Could not find double config '%s'.",
	    keyword_get_string(a_name));
}

int32_t
config_get_int32(struct ConfigBlock *a_parent, enum Keyword a_name, struct
    ConfigUnit const *a_unit, int32_t a_min, int32_t a_max)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_INTEGER == scalar->type) {
				int32_t value;

				if (CONFIG_UNIT_NONE == a_unit &&
				    CONFIG_UNIT_NONE == scalar->unit) {
					value = scalar->value.i;
				} else {
					value = i32_round_double(
					    scalar->value.i *
					    unit_get_mult(scalar->unit,
					    a_unit));
				}
				item->is_touched = 1;
				return MIN(MAX(value, a_min), a_max);
			}
		}
	}
	log_die(LOGL, "Could not find integer config '%s'.",
	    keyword_get_string(a_name));
}

void
config_get_int_array(void *a_dst, size_t a_dst_size, size_t a_dst_elem_size,
    struct ConfigBlock *a_parent, enum Keyword a_name, struct ConfigUnit const
    *a_unit, int32_t a_min, int32_t a_max)
{
	struct ItemList *item_list;
	struct Item *item;
	size_t dst_length;

	dst_length = a_dst_size / a_dst_elem_size;
	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;
			uint32_t *p32;
			uint16_t *p16;
			uint8_t *p8;
			size_t length;
			int i_prev;

			length = 0;
			i_prev = -1;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				if ((int)scalar->vector_index <= i_prev) {
					log_die(LOGL, "List items out of "
					    "order (prev=%d,cur=%d) for '%s' "
					    "(%s:%d:%d)!", i_prev,
					    scalar->vector_index,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				i_prev = scalar->vector_index;
				if (CONFIG_SCALAR_INTEGER != scalar->type) {
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) not integer!",
					    length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				if (!unit_is_compatible(a_unit, scalar->unit))
				{
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) has incompatible"
					    " unit %s (!=%s)!", length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no,
					    unit_dump(scalar->unit),
					    unit_dump(a_unit));
				}
				++length;
			}
			if (length != dst_length) {
				log_die(LOGL, "List '%s' (%s:%d:%d) has "
				    "srclen=%"PRIz" != user dstlen=%"PRIz"!",
				    keyword_get_string(item->name),
				    item->src_path, item->src_line_no,
				    item->src_col_no, length, dst_length);
			}

			p32 = a_dst;
			p16 = a_dst;
			p8 = a_dst;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				int32_t value;

				if (CONFIG_UNIT_NONE == a_unit &&
				    CONFIG_UNIT_NONE == scalar->unit) {
					value = scalar->value.i;
				} else {
					value = i32_round_double(
					    scalar->value.i *
					    unit_get_mult(scalar->unit,
					    a_unit));
				}
				value = MIN(MAX(value, a_min), a_max);
				switch (a_dst_elem_size) {
				case 1: *p8++ = value; break;
				case 2: *p16++ = value; break;
				case 4: *p32++ = value; break;
				default:
					log_die(LOGL, "Funny integer size "
					    "%"PRIz".", a_dst_elem_size);
				}
			}

			item->is_touched = 1;
			return;
		}
	}
	log_die(LOGL, "Could not find integer config '%s'.",
	    keyword_get_string(a_name));
}

enum Keyword
config_get_keyword_(struct ConfigBlock *a_parent, enum Keyword a_name, size_t
    a_keyword_num, enum Keyword const *a_keyword_list)
{
	struct ItemList *item_list;
	struct Item *item;
	enum Keyword keyword;

	keyword = KW_NONE;
	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_KEYWORD == scalar->type) {
				keyword = scalar->value.k;
				break;
			}
		}
	}
	if (KW_NONE == keyword) {
		log_die(LOGL, "Could not find keyword config '%s'.",
		    keyword_get_string(a_name));
	}
	keyword_match(item, a_keyword_num, a_keyword_list, keyword);
	return keyword;
}

void
config_get_keyword_array_(enum Keyword *a_dst, size_t a_dst_num, struct
    ConfigBlock *a_parent, enum Keyword a_name, size_t a_kw_num, enum Keyword
    const *a_kw_array)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;
			enum Keyword *pk;
			size_t length;
			int i_prev;
			unsigned kw_i0, kw_i1;

			length = 0;
			i_prev = -1;
			kw_i0 = 0;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				if ((int)scalar->vector_index <= i_prev) {
					log_die(LOGL, "List items out of "
					    "order (prev=%d,cur=%d) for '%s' "
					    "(%s:%d:%d)!", i_prev,
					    scalar->vector_index,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				i_prev = scalar->vector_index;
				if (CONFIG_SCALAR_KEYWORD != scalar->type) {
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) not keyword!",
					    length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				/* Extract keywords for this index. */
				for (kw_i1 = kw_i0; kw_i1 < a_kw_num &&
				    KW_NONE != a_kw_array[kw_i1]; ++kw_i1)
					;
				if (kw_i1 == kw_i0) {
					log_die(LOGL, "Keyword table for %s "
					    "not large enough!",
					    keyword_get_string(a_name));
				}
				keyword_match(item, kw_i1 - kw_i0,
				    &a_kw_array[kw_i0], scalar->value.k);
				kw_i0 = kw_i1 + 1;
				++length;
			}
			if (length != a_dst_num) {
				log_die(LOGL, "List '%s' (%s:%d:%d) has "
				    "srclen=%"PRIz" != user dstlen=%"PRIz"!",
				    keyword_get_string(item->name),
				    item->src_path, item->src_line_no,
				    item->src_col_no, length, a_dst_num);
			}

			pk = a_dst;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				*pk++ = scalar->value.k;
			}

			item->is_touched = 1;
			return;
		}
	}
	log_die(LOGL, "Could not find keyword config '%s'.",
	    keyword_get_string(a_name));
}

void
config_get_source(struct ConfigBlock const *a_config, char const **a_path, int
    *a_line_no, int *a_col_no)
{
	struct Item const *item;

	item = (struct Item const *)a_config;
	if (NULL != a_path) {
		*a_path = item->src_path;
	}
	if (NULL != a_line_no) {
		*a_line_no = item->src_line_no;
	}
	if (NULL != a_col_no) {
		*a_col_no = item->src_col_no;
	}
}

char const *
config_get_string(struct ConfigBlock *a_parent, enum Keyword a_name)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_STRING == scalar->type) {
				item->is_touched = 1;
				return scalar->value.s;
			}
		}
	}
	log_die(LOGL, "Could not find string config '%s'.",
	    keyword_get_string(a_name));
}

void
config_load(char const *a_filename)
{
	parser_include_file("global.cfg", 0);
	config_load_without_global(a_filename);
}

void
config_load_without_global(char const *a_filename)
{
	parser_include_file(a_filename, 1);
	config_dump(NULL);
}

int
config_merge(struct ConfigBlock *a_block, struct Packer *a_packer)
{
	size_t ofs;

	if (0 == a_packer->bytes) {
		return 1;
	}
	ofs = a_packer->ofs;
	if (!unpack_snippet(a_block, a_packer, 0)) {
		log_error(LOGL, "unpack_snippet failed.");
		return 0;
	}
	if (a_packer->ofs != a_packer->bytes) {
		log_error(LOGL, "a_packer ofs (%"PRIz") != bytes (%"PRIz").",
		    a_packer->ofs, a_packer->bytes);
		return 0;
	}
	a_packer->ofs = ofs;
	if (!unpack_snippet(a_block, a_packer, 1) ||
	    a_packer->ofs != a_packer->bytes) {
		log_die(LOGL, "This should not happen!");
	}
	return 1;
}

void
config_pack_children(struct PackerList *a_list, struct ConfigBlock const
    *a_block)
{
	pack_item_list(a_list, NULL == a_block ? &g_item_list :
	    &((struct Item const *)a_block)->item_list);
}

void
config_set_bool(struct ConfigBlock *a_parent, enum Keyword a_name, int a_yes)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_KEYWORD == scalar->type) {
				scalar->value.k = a_yes ? KW_TRUE : KW_FALSE;
				item->is_touched = 1;
				return;
			}
		}
	}
	log_die(LOGL, "Could not find bool config '%s'.",
	    keyword_get_string(a_name));
}

void
config_set_int32(struct ConfigBlock *a_parent, enum Keyword a_name, struct
    ConfigUnit const *a_unit, int32_t a_value)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;

			scalar = TAILQ_FIRST(&item->scalar_list);
			if (CONFIG_SCALAR_INTEGER == scalar->type) {
				scalar->value.i = a_value;
				scalar->unit = a_unit;
				item->is_touched = 1;
				return;
			}
		}
	}
	log_die(LOGL, "Could not find integer config '%s'.",
	    keyword_get_string(a_name));
}

void
config_set_int_array_(struct ConfigBlock *a_parent, enum Keyword a_name,
    struct ConfigUnit const *a_unit, void const *a_src, size_t a_src_byte_num,
    size_t a_src_elem_size)
{
	struct ItemList *item_list;
	struct Item *item;
	size_t src_length;

	src_length = a_src_byte_num / a_src_elem_size;
	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			struct Scalar *scalar;
			uint32_t const *p32;
			uint16_t const *p16;
			uint8_t const *p8;
			size_t length;
			int i_prev;

			length = 0;
			i_prev = -1;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				if ((int)scalar->vector_index != i_prev + 1) {
					log_die(LOGL, "List items not in "
					    "consecutive order "
					    "(prev=%d,cur=%d) for '%s' "
					    "(%s:%d:%d)!", i_prev,
					    scalar->vector_index,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				++i_prev;
				if (CONFIG_SCALAR_INTEGER != scalar->type) {
					log_die(LOGL, "List item %"PRIz" of "
					    "'%s' (%s:%d:%d) not integer!",
					    length,
					    keyword_get_string(item->name),
					    item->src_path, item->src_line_no,
					    item->src_col_no);
				}
				++length;
			}
			if (length != src_length) {
				log_die(LOGL, "List '%s' (%s:%d:%d) has "
				    "dstlen=%"PRIz" != user srclen=%"PRIz"!",
				    keyword_get_string(item->name),
				    item->src_path, item->src_line_no,
				    item->src_col_no, length, src_length);
			}

			p32 = a_src;
			p16 = a_src;
			p8 = a_src;
			TAILQ_FOREACH(scalar, &item->scalar_list, next) {
				scalar->unit = a_unit;
				switch (a_src_elem_size) {
				case 1: scalar->value.i = *p8++; break;
				case 2: scalar->value.i = *p16++; break;
				case 4: scalar->value.i = *p32++; break;
				default:
					log_die(LOGL, "Funny integer size "
					    "%"PRIz".", a_src_elem_size);
				}
			}
			item->is_touched = 1;
			return;
		}
	}
	log_die(LOGL, "Could not find integer array config '%s'.",
	    keyword_get_string(a_name));
}

void
config_shutdown(void)
{
	while (!TAILQ_EMPTY(&g_auto_config_list)) {
		struct AutoConfig *auto_config;

		auto_config = TAILQ_FIRST(&g_auto_config_list);
		TAILQ_REMOVE(&g_auto_config_list, auto_config, next);
		FREE(auto_config);
	}
	item_list_clear(&g_item_list);
	while (!TAILQ_EMPTY(&g_path_list)) {
		struct Path *path;

		path = TAILQ_FIRST(&g_path_list);
		TAILQ_REMOVE(&g_path_list, path, next);
		FREE(path);
	}
	if (NULL != g_temp_block) {
		TAILQ_INIT(&g_temp_block->scalar_list);
		TAILQ_INIT(&g_temp_block->item_list);
		FREE(g_temp_block);
	}
	FREE(g_default_path);
}

void
config_snippet_free(struct ConfigBlock **a_block)
{
	struct Item *block;

	block = (void *)*a_block;
	if (NULL == block) {
		return;
	}
	item_list_clear(&block->item_list);
	FREE(*a_block);
}

struct ConfigBlock *
config_snippet_parse(char const *a_str)
{
	struct ConfigBlock *block;

	ASSERT(size_t, PRIz, 0, ==, g_block_builder_index);
	parser_prepare_block(KW_NONE, SNIPPET_SRC);
	block = (void *)g_temp_block;
	g_block_builder[g_block_builder_index++] = g_temp_block;
	parse_snippet(a_str);
	parser_pop_block();
	g_temp_block = NULL;
	config_dump(NULL);
	return block;
}

void
config_touched_assert(struct ConfigBlock *a_block, int a_do_recurse)
{
	struct ItemList *item_list;

	item_list = parent_get(a_block);
	if (!TAILQ_EMPTY(item_list)) {
		touched_traverse(KW_NONE, item_list, a_do_recurse);
	}
}

struct Item *
block_find_matching(struct ItemList *a_item_list, enum Keyword a_name, struct
    ScalarList const *a_scalar_list, char const *a_path, int a_line_no, int
    a_col_no)
{
	struct Item *item;

	/* Allow equal tag definitions. */
	if (KW_TAGS == a_name) {
		return NULL;
	}
	TAILQ_FOREACH(item, a_item_list, next) {
		struct Scalar const *p1;
		struct Scalar const *p2;

		if (CONFIG_BLOCK != item->type ||
		    a_name != item->name) {
			continue;
		}
		p1 = TAILQ_FIRST(a_scalar_list);
		p2 = TAILQ_FIRST(&item->scalar_list);
		for (;;) {
			int is_eq;

			if (TAILQ_END(a_scalar_list) == p1 ||
			    TAILQ_END(&item->scalar_list) == p2 ||
			    p1->type != p2->type) {
				break;
			}
			switch (p1->type) {
			case CONFIG_SCALAR_EMPTY:
				is_eq = 0;
				break;
			case CONFIG_SCALAR_DOUBLE:
				is_eq = p1->value.d == p2->value.d;
				break;
			case CONFIG_SCALAR_INTEGER:
				is_eq = p1->value.i == p2->value.i;
				break;
			case CONFIG_SCALAR_KEYWORD:
				is_eq = p1->value.k == p2->value.k;
				break;
			case CONFIG_SCALAR_RANGE:
				is_eq = p1->value.r.first ==
				    p2->value.r.first &&
				    p1->value.r.last == p2->value.r.last;
				break;
			case CONFIG_SCALAR_STRING:
				is_eq = 0 == strcmp(p1->value.s,
				    p2->value.s);
				break;
			default:
				log_die(LOGL, "This should not happen!");
			}
			if (!is_eq) {
				break;
			}
			p1 = TAILQ_NEXT(p1, next);
			p2 = TAILQ_NEXT(p2, next);
		}
		if (TAILQ_END(a_scalar_list) == p1 &&
		    TAILQ_END(&item->scalar_list) == p2) {
			assert_src_path(item, a_path, a_line_no, a_col_no);
			return item;
		}
	}
	return NULL;
}

void
item_free(struct Item **a_item)
{
	struct Item *item;

	item = *a_item;
	parser_scalar_list_clear(&item->scalar_list);
	item_list_clear(&item->item_list);
	FREE(item);
	*a_item = NULL;
}

void
item_list_clear(struct ItemList *a_item_list)
{
	while (!TAILQ_EMPTY(a_item_list)) {
		struct Item *item;

		item = TAILQ_FIRST(a_item_list);
		TAILQ_REMOVE(a_item_list, item, next);
		item_free(&item);
	}
}

struct ItemList *
item_list_get(void)
{
	return 0 == g_block_builder_index ? &g_item_list :
	    &g_block_builder[g_block_builder_index - 1]->item_list;
}

void
keyword_match(struct Item *a_item, size_t a_keyword_num, enum Keyword const
    *a_keyword_list, enum Keyword a_trial)
{
	size_t i;

	if (0 == a_keyword_num) {
		return;
	}
	for (i = 0; a_keyword_num > i; ++i) {
		if (a_keyword_list[i] == a_trial) {
			a_item->is_touched = 1;
			return;
		}
	}
	log_error(LOGL, "Keyword '%s' (%s:%d,%d) has invalid value '%s', "
	    "expected:", keyword_get_string(a_item->name), a_item->src_path,
	    a_item->src_line_no, a_item->src_col_no,
	    keyword_get_string(a_trial));
	for (i = 0; a_keyword_num > i; ++i) {
		log_error(LOGL, " Keyword[%u] = \"%s\".", a_keyword_list[i],
		    keyword_get_string(a_keyword_list[i]));
	}
	log_die(LOGL, "This is too much, I quit.");
}

#define PACK(list, n, item) do { \
	packer = packer_list_get(list, n); \
	if (NULL == packer) log_die(LOGL, "Could not pack %d bits.\n", n); \
	pack##n(packer, item); \
} while(0)

void
pack_item_list(struct PackerList *a_packer_list, struct ItemList const
    *a_item_list)
{
	struct Item const *item;
	struct Packer *packer;
	uint16_t num;

	num = 0;
	TAILQ_FOREACH(item, a_item_list, next) {
		++num;
	}
	PACK(a_packer_list, 16, num);
	TAILQ_FOREACH(item, a_item_list, next) {
		PACK(a_packer_list, 8, item->type | item->is_touched << 1);
		PACK(a_packer_list, 16, item->name);
		pack_scalar_list(a_packer_list, &item->scalar_list);
		if (CONFIG_BLOCK == item->type &&
		    KW_BARRIER != item->name &&
		    KW_TAGS != item->name) {
			pack_item_list(a_packer_list, &item->item_list);
		}
	}
}

void
pack_scalar(struct PackerList *a_list, struct Scalar const *a_scalar)
{
	union {
		double	d;
		uint64_t	u64;
	} u;
	struct Packer *packer;

	/* TODO: Are 8 bits enough for this? */
	PACK(a_list, 8, a_scalar->type);
	PACK(a_list, 16, a_scalar->vector_index);
	switch (a_scalar->type) {
	case CONFIG_SCALAR_EMPTY:
		break;
	case CONFIG_SCALAR_DOUBLE:
		/* Ugh... The beauty and bane of C. */
		u.d = a_scalar->value.d;
		PACK(a_list, 64, u.u64);
		PACK(a_list, 8, a_scalar->unit->idx);
		break;
	case CONFIG_SCALAR_INTEGER:
		PACK(a_list, 32, a_scalar->value.i);
		PACK(a_list, 8, a_scalar->unit->idx);
		break;
	case CONFIG_SCALAR_KEYWORD:
		PACK(a_list, 16, a_scalar->value.k);
		break;
	case CONFIG_SCALAR_RANGE:
		PACK(a_list, 8, a_scalar->value.r.first);
		PACK(a_list, 8, a_scalar->value.r.last);
		break;
	case CONFIG_SCALAR_STRING:
		/* meh, the macro does not work here... */
		packer = packer_list_get(a_list,
		    (strlen(a_scalar->value.s) + 1) * 8);
		pack_str(packer, a_scalar->value.s);
		break;
	}
}

/*
 * RLE:d scalar list packing (index,value):
 * (0,0) (1,1) (2,1) (3,2) (4,2) (5,2) (7,2) (8,4) (9,5) (10,6) ->
 *  0x00 (0,0) 0x80 (1,1) 0x81 (3,2) 0x00 (7,2) 0x02 (8,4) (9,5) (10,6)
 */
void
pack_scalar_list(struct PackerList *a_list, struct ScalarList const
    *a_scalar_list)
{
	struct Scalar const *scalar;
	struct Packer *packer;
	uint8_t *num;
	unsigned count_tot = 0;

	/*
	 * Need the packer state before actually packing, so the macro won't
	 * work here.
	 */
	packer = packer_list_get(a_list, 16);
	num = (uint8_t *)packer->data + packer->ofs;
	pack16(packer, 0);
	TAILQ_FOREACH(scalar, a_scalar_list, next) {
		struct Scalar const *rover;
		unsigned count;

		/* See if we have a clone run with incrementing indices. */
		/* Limit: 0x80 -> 2 clones, 0xff -> 129 clones. */
		count = 1;
		for (rover = scalar; count < 129; ++count) {
			struct Scalar const *next;

			next = TAILQ_NEXT(rover, next);
			if (TAILQ_END(a_scalar_list) == next ||
			    !scalar_are_equal(scalar, next) ||
			    rover->vector_index + 1 != next->vector_index) {
				break;
			}
			rover = next;
		}
		if (count > 1) {
			/* Store a dup run. */
			PACK(a_list, 8, 0x80 | (count - 2));
			pack_scalar(a_list, scalar);
			scalar = rover;
		} else {
			uint8_t *rle;

			/* Store a raw run. */
			packer = packer_list_get(a_list, 8);
			rle = (uint8_t *)packer->data + packer->ofs;
			pack8(packer, 0);
			/* Limit: 0x00 -> scalar, 0x7f -> 128 scalars. */
			count = 1;
			for (; count < 128; ++count) {
				struct Scalar const *next;

				pack_scalar(a_list, scalar);
				next = TAILQ_NEXT(scalar, next);
				if (TAILQ_END(a_scalar_list) == next ||
				    scalar_are_equal(scalar, next)) {
					break;
				}
				scalar = next;
			}
			*rle = count - 1;
		}
		count_tot += count;
	}
	num[0] = count_tot >> 8;
	num[1] = count_tot & 0xff;
}

/* A null parent means we're at the root. */
struct ItemList *
parent_get(struct ConfigBlock *a_parent)
{
	return NULL == a_parent ? &g_item_list :
	    &((struct Item *)a_parent)->item_list;
}

/* Checks global log level. */
void
parser_check_log_level(void)
{
	struct LogLevel const *log_level;
	struct Item *item;
	struct Scalar const *scalar;
	enum Keyword keyword;

	if (0 != g_block_builder_index) {
		return;
	}
	item = TAILQ_LAST(&g_item_list, ItemList);
	if (KW_LOG_LEVEL != item->name) {
		return;
	}
	scalar = TAILQ_FIRST(&item->scalar_list);
	if (CONFIG_SCALAR_KEYWORD != scalar->type) {
		log_error(LOGL, "Config: log_level requires a keyword "
		    "value.");
		return;
	}
	keyword = scalar->value.k;
	if (KW_INFO != keyword &&
	    KW_VERBOSE != keyword &&
	    KW_DEBUG != keyword &&
	    KW_SPAM != keyword) {
		log_error(LOGL, "Config: log_level must be 'info', "
		    "'verbose', 'debug', or 'spam' (is '%s').",
		    keyword_get_string(keyword));
		return;
	}
	item->is_touched = 1;
	log_level = log_level_get_from_keyword(keyword);
	log_level_push(log_level);
	LOGF(info)(LOGL, "Global log level=%s.", keyword_get_string(keyword));
}

void
parser_copy_scalar(struct ScalarList *a_scalar_list, unsigned a_src_index,
    unsigned a_num)
{
	struct Scalar const *src;
	unsigned i;

	src = scalar_get(a_scalar_list, a_src_index);
	for (i = 0; a_num > i; ++i) {
		struct Scalar *cur;

		/* TODO: This is needlessly _very_ slow... */
		cur = scalar_get(a_scalar_list, a_src_index + 1 + i);
		cur->type = src->type;
		memcpy(&cur->value, &src->value, sizeof cur->value);
		cur->unit = src->unit;
		cur->has_bitmask = src->has_bitmask;
		cur->bitmask = src->bitmask;
	}
}

/*
 * Creates a bitmask in the first scalar:
 *  - () -> 0x0
 *  - 0 -> 0x1
 *  - (0) -> 0x1
 *  - (0 1) -> 0x3
 *  - (0,2..3) -> 0xd
 */
void
parser_create_bitmask(struct ScalarList *a_scalar_list)
{
	struct Scalar *first, *scalar;

	first = TAILQ_FIRST(a_scalar_list);
	if (TAILQ_END(a_scalar_list) == first) {
		CALLOC(first, 1);
		first->type = CONFIG_SCALAR_EMPTY;
		first->has_bitmask = 1;
		TAILQ_INSERT_TAIL(a_scalar_list, first, next);
		return;
	}
	first->has_bitmask = 1;
	TAILQ_FOREACH(scalar, a_scalar_list, next) {
		if ((CONFIG_SCALAR_INTEGER == scalar->type &&
		     CONFIG_UNIT_NONE != scalar->unit) ||
		    (CONFIG_SCALAR_INTEGER != scalar->type &&
		     CONFIG_SCALAR_RANGE != scalar->type)) {
			first->has_bitmask = 0;
			break;
		}
	}
	if (!first->has_bitmask) {
		return;
	}
	first->bitmask = 0;
	TAILQ_FOREACH(scalar, a_scalar_list, next) {
		if (CONFIG_SCALAR_INTEGER == scalar->type) {
			if (0 <= scalar->value.i &&
			    32 > scalar->value.i) {
				first->bitmask |=
				    1UL << (uint32_t)scalar->value.i;
			}
		} else if (CONFIG_SCALAR_RANGE == scalar->type) {
			first->bitmask |= (0xffffffff <<
			    scalar->value.r.first) & (0xffffffff >> (31 -
				scalar->value.r.last));
		}
	}
}

/*
 * Tries first a default, then a user config file IF a_do_user_file is set.
 * The latter is needed for the global file for which we only want the
 * default, otherwise a user could make 'global.cfg' which would probably be
 * unintentionally included automatically.
 */
void
parser_include_file(char const *a_path, int a_do_user_file)
{
	char path_expanded[1024];
	char *previous_root_path;
	char const *src;
	char *dst;
	char *dir, *base, *path;
	size_t strsiz;
	int has_default, has_user;

	previous_root_path = g_current_root_path;

	/* Substitute variables in path. */
	dst = path_expanded;
	for (src = a_path; '\0' != *src;) {
		if (path_expanded + LENGTH(path_expanded) == dst) {
			log_die(LOGL, "%s: Enormous path, I die.", a_path);
		}
		if ('\\' == src[0] && '$' == src[1]) {
			*dst++ = '$';
			src += 2;
		} else if ('$' == src[0]) {
			char const *start;
			char const *end;
			char *name;
			size_t len;
			char const *value;

			++src;
			if ('{' == src[0]) {
				for (start = ++src; '}' != *src; ++src) {
					if ('\0' == *src) {
						log_die(LOGL, "%s: "
						    "Unterminated variable "
						    "name, I die.", a_path);
					}
				}
				end = src;
				++src;
			} else {
				for (start = src; '\0' != *src && '/' != *src;
				    ++src)
					;
				end = src;
			}
			len = end - start;
			name = malloc(len + 1);
			memcpy(name, start, len);
			name[len] = '\0';
			value = getenv(name);
			if (NULL == value) {
				log_die(LOGL, "%s: \"$%s\" not set, I die.",
				    a_path, name);
			}
			len = strlen(value);
			if (dst + len >=
			    path_expanded + LENGTH(path_expanded)) {
				log_die(LOGL, "%s: Expansion of \"$%s\" too"
				    " big, I die.", a_path, name);
			}
			memcpy(dst, value, len);
			dst += len;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';

	dir = dirname_dup(path_expanded);
	base = basename_dup(path_expanded);
	g_current_root_path = dir;

	/* Try default config. */
	strsiz = strlen(config_default_path_get()) + 1 + strlen(base) + 1;
	path = malloc(strsiz);
	snprintf(path, strsiz, "%s/%s", config_default_path_get(), base);
	LOGF(debug)(LOGL, "Trying default config file '%s'.", path);
	has_default = parse_file(path);
	FREE(path);

	/* Try user config. */
	if (a_do_user_file) {
		if ('/' == path_expanded[0]) {
			path = strdup(path_expanded);
		} else {
			strsiz = strlen(previous_root_path) + 1 +
			    strlen(path_expanded) + 1;
			path = malloc(strsiz);
			snprintf(path, strsiz, "%s/%s", previous_root_path,
			    path_expanded);
		}
		LOGF(debug)(LOGL, "Trying config file '%s'.", path);
		has_user = parse_file(path);
		FREE(path);
	} else {
		has_user = 0;
	}

	FREE(dir);
	FREE(base);
	g_current_root_path = previous_root_path;

	if (!has_default && !has_user) {
		log_die(LOGL, "Could not load default or user config '%s'.",
		    path_expanded);
	}
}

/* Remove block from stack. */
void
parser_pop_block(void)
{
	assert(0 < g_block_builder_index);
	--g_block_builder_index;
}

/* Setup temporary holder for a new block. */
struct ScalarList *
parser_prepare_block(enum Keyword a_name, char const *a_path, int a_line_no,
    int a_col_no)
{
	CALLOC(g_temp_block, 1);
	g_temp_block->type = CONFIG_BLOCK;
	g_temp_block->name = a_name;
	g_temp_block->src_path = path_get_instance(a_path);
	g_temp_block->src_line_no = a_line_no;
	g_temp_block->src_col_no = a_col_no;
	g_temp_block->is_touched = 0;
	TAILQ_INIT(&g_temp_block->scalar_list);
	TAILQ_INIT(&g_temp_block->item_list);
	return &g_temp_block->scalar_list;
}

/*
 * Match a block + params against already parsed blocks, in case different
 * files should overwrite configs, BUT, do not allow matching of configs in
 * one file, that's typically a copy-paste error.
 */
void
parser_push_block(void)
{
	struct ItemList *item_list;
	struct Item *item;
	struct AutoConfig *auto_config;

	item_list = item_list_get();
	item = block_find_matching(item_list, g_temp_block->name,
	    &g_temp_block->scalar_list, g_temp_block->src_path,
	    g_temp_block->src_line_no, g_temp_block->src_col_no);
	if (NULL != item) {
		/* Found duplicate (name and params), overwrite it. */
		item_free(&g_temp_block);
	} else {
		/* No match, use new. */
		item = g_temp_block;
		TAILQ_INSERT_TAIL(item_list, item, next);
		g_temp_block = NULL;
	}

	if (BLOCK_BUILDER_DEPTH == g_block_builder_index) {
		log_die(LOGL, "Blocks nested too deep (>%u).",
		    BLOCK_BUILDER_DEPTH);
	}
	g_block_builder[g_block_builder_index++] = item;
	TAILQ_FOREACH(auto_config, &g_auto_config_list, next) {
		if (item->name == auto_config->name) {
			parser_include_file(auto_config->path, 1);
			/*
			 * NOTE: Do not break early, e.g. log-level is set for
			 * all module types by a generic auto-block.
			 */
		}
	}
}

/* Like above but for a config. */
struct ScalarList *
parser_push_config(enum Keyword a_name, int a_vector_index, char const
    *a_path, int a_line_no, int a_col_no)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = item_list_get();
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type &&
		    a_name == item->name) {
			if (0 > a_vector_index) {
				/* Don't check dups for e.g. name[0] = ... */
				assert_src_path(item, a_path, a_line_no,
				    a_col_no);
			}
			item->src_path = path_get_instance(a_path);
			item->src_line_no = a_line_no;
			item->src_col_no = a_col_no;
			return &item->scalar_list;
		}
	}

	CALLOC(item, 1);
	item->type = CONFIG_CONFIG;
	item->name = a_name;
	item->src_path = path_get_instance(a_path);
	item->src_line_no = a_line_no;
	item->src_col_no = a_col_no;
	item->is_touched = 0;
	TAILQ_INIT(&item->scalar_list);
	TAILQ_INIT(&item->item_list);
	TAILQ_INSERT_TAIL(item_list, item, next);
	return &item->scalar_list;
}

void
parser_push_double(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    double a_d)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->type = CONFIG_SCALAR_DOUBLE;
	scalar->value.d = a_d;
	scalar->unit = CONFIG_UNIT_NONE;
}

void
parser_push_integer(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    int32_t a_i32)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->type = CONFIG_SCALAR_INTEGER;
	scalar->value.i = a_i32;
	scalar->unit = CONFIG_UNIT_NONE;
}

void
parser_push_keyword(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    enum Keyword a_keyword)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->type = CONFIG_SCALAR_KEYWORD;
	scalar->value.k = a_keyword;
	scalar->unit = CONFIG_UNIT_NONE;
}

void
parser_push_range(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    unsigned a_first, unsigned a_last)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->type = CONFIG_SCALAR_RANGE;
	scalar->value.r.first = a_first;
	scalar->value.r.last = a_last;
	scalar->unit = CONFIG_UNIT_NONE;
}

/*
 * Pushes a barrier/id_skip pseudo-block without any duplicate checks, because
 * these are all the same but need to be separate, all other non-unique blocks
 * are merged.
 */
void
parser_push_simple_block(void)
{
	struct ItemList *item_list;

	item_list = item_list_get();
	TAILQ_INSERT_TAIL(item_list, g_temp_block, next);
	g_temp_block = NULL;
}

void
parser_push_string(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    char const *a_s)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->type = CONFIG_SCALAR_STRING;
	scalar->value.s = strdup(a_s);
	scalar->unit = CONFIG_UNIT_NONE;
}

void
parser_push_unit(struct ScalarList *a_scalar_list, unsigned a_vector_index,
    struct ConfigUnit const *a_unit)
{
	struct Scalar *scalar;

	scalar = scalar_get(a_scalar_list, a_vector_index);
	scalar->unit = a_unit;
}

void
parser_scalar_list_clear(struct ScalarList *a_scalar_list)
{
	while (!TAILQ_EMPTY(a_scalar_list)) {
		struct Scalar *scalar;

		scalar = TAILQ_FIRST(a_scalar_list);
		TAILQ_REMOVE(a_scalar_list, scalar, next);
		if (CONFIG_SCALAR_STRING == scalar->type) {
			FREE(scalar->value.s);
		}
		FREE(scalar);
	}
}

/* Gives source information for config. */
void
parser_src_get(struct ConfigBlock *a_parent, enum Keyword a_name, char const
    **a_path, int *a_line_no, int *a_col_no)
{
	struct ItemList *item_list;
	struct Item *item;

	item_list = parent_get(a_parent);
	TAILQ_FOREACH(item, item_list, next) {
		if (CONFIG_CONFIG == item->type && a_name == item->name) {
			*a_path = item->src_path;
			*a_line_no = item->src_line_no;
			*a_col_no = item->src_col_no;
			return;
		}
	}
	log_die(LOGL, "Could not find config '%s'.",
	    keyword_get_string(a_name));
}

char const *
path_get_instance(char const *a_path)
{
	struct Path *path;
	size_t len;

	TAILQ_FOREACH(path, &g_path_list, next) {
		if (0 == strcmp(path->path, a_path)) {
			return path->path;
		}
	}
	/* Note that the terminating null is included in the [1] array! */
	len = strlen(a_path);
	path = malloc(sizeof *path + len);
	strlcpy(path->path, a_path, len + 1);
	TAILQ_INSERT_HEAD(&g_path_list, path, next);
	return path->path;
}

int
scalar_are_equal(struct Scalar const *a_left, struct Scalar const *a_right)
{
	if (a_left->type != a_right->type) {
		return 0;
	}
	switch (a_left->type) {
	case CONFIG_SCALAR_EMPTY:
		log_die(LOGL, "Double CONFIG_SCALAR_EMPTY!");
	case CONFIG_SCALAR_DOUBLE:
		return a_left->value.d == a_right->value.d &&
		    a_left->unit == a_right->unit;
	case CONFIG_SCALAR_INTEGER:
		return a_left->value.i == a_right->value.i &&
		    a_left->unit == a_right->unit;
	case CONFIG_SCALAR_KEYWORD:
		return a_left->value.k == a_right->value.k;
	case CONFIG_SCALAR_RANGE:
		return a_left->value.r.first == a_right->value.r.first &&
		    a_left->value.r.last == a_right->value.r.last;
	case CONFIG_SCALAR_STRING:
		return 0 == strcmp(a_left->value.s, a_right->value.s);
	}
	log_die(LOGL, "This should not happen!");
}

struct Scalar *
scalar_get(struct ScalarList *a_scalar_list, unsigned a_vector_index)
{
	struct Scalar *cur, *prev;

	prev = NULL;
	TAILQ_FOREACH(cur, a_scalar_list, next) {
		if (cur->vector_index == a_vector_index) {
			return cur;
		}
		if (cur->vector_index > a_vector_index) {
			break;
		}
		prev = cur;
	}
	CALLOC(cur, 1);
	cur->vector_index = a_vector_index;
	if (NULL == prev) {
		TAILQ_INSERT_HEAD(a_scalar_list, cur, next);
	} else {
		TAILQ_INSERT_AFTER(a_scalar_list, prev, cur, next);
	}
	return cur;
}

void
scalar_list_dump(struct ScalarList const *a_scalar_list)
{
	struct Scalar *scalar;

	TAILQ_FOREACH(scalar, a_scalar_list, next) {
		switch (scalar->type) {
		case CONFIG_SCALAR_EMPTY:
			LOGF(debug)(LOGL, "Empty vector");
			break;
		case CONFIG_SCALAR_DOUBLE:
			LOGF(debug)(LOGL, "Double=%g%s",
			    scalar->value.d, unit_dump(scalar->unit));
			break;
		case CONFIG_SCALAR_INTEGER:
			LOGF(debug)(LOGL, "Integer=%d%s",
			    scalar->value.i, unit_dump(scalar->unit));
			break;
		case CONFIG_SCALAR_KEYWORD:
			LOGF(debug)(LOGL, "Keyword=%s",
			    keyword_get_string(scalar->value.k));
			break;
		case CONFIG_SCALAR_RANGE:
			LOGF(debug)(LOGL, "Range=%d..%d",
			    scalar->value.r.first,
			    scalar->value.r.last);
			break;
		case CONFIG_SCALAR_STRING:
			LOGF(debug)(LOGL, "String=\"%s\"", scalar->value.s);
			break;
		}
	}
	scalar = TAILQ_FIRST(a_scalar_list);
	if (TAILQ_END(a_scalar_list) != scalar && scalar->has_bitmask) {
		LOGF(debug)(LOGL, ".Bitmask=0x%08x", scalar->bitmask);
	}
}

void
touched_traverse(enum Keyword a_name, struct ItemList const *a_item_list, int
    a_do_recurse)
{
	struct Item const *item;

	LOGF(debug)(LOGL, "touched_traverse=%s {",
	    keyword_get_string(a_name));
	TAILQ_FOREACH(item, a_item_list, next) {
		if (!item->is_touched &&
		    (a_do_recurse || CONFIG_CONFIG == item->type)) {
			log_die(LOGL, "Config=%s (%s:%d,%d) unused.",
			    keyword_get_string(item->name), item->src_path,
			    item->src_line_no, item->src_col_no);
		}
		if (a_do_recurse && !TAILQ_EMPTY(&item->item_list)) {
			touched_traverse(item->name, &item->item_list, 1);
		}
	}
	LOGF(debug)(LOGL, "touched_traverse }");
}

char const *
unit_dump(struct ConfigUnit const *a_unit)
{
	if (CONFIG_UNIT_NONE == a_unit) {
		return "no-unit";
	} else if (CONFIG_UNIT_NS == a_unit) {
		return "ns";
	} else if (CONFIG_UNIT_PS == a_unit) {
		return "ps";
	} else if (CONFIG_UNIT_US == a_unit) {
		return "us";
	} else if (CONFIG_UNIT_MS == a_unit) {
		return "ms";
	} else if (CONFIG_UNIT_S == a_unit) {
		return "s";
	} else if (CONFIG_UNIT_V == a_unit) {
		return "V";
	} else if (CONFIG_UNIT_MV == a_unit) {
		return "mV";
	} else if (CONFIG_UNIT_MHZ == a_unit) {
		return "MHz";
	} else if (CONFIG_UNIT_KHZ == a_unit) {
		return "kHz";
	} else if (CONFIG_UNIT_B == a_unit) {
		return "B";
	} else if (CONFIG_UNIT_KIB == a_unit) {
		return "KiB";
	} else if (CONFIG_UNIT_MIB == a_unit) {
		return "MIB";
	}
	log_die(LOGL, "Unknown unit.");
}

struct ConfigUnit const *
unit_from_id(unsigned a_id)
{
	switch (a_id) {
	case 0:
		return CONFIG_UNIT_NONE;
	case 1:
		return CONFIG_UNIT_MHZ;
	case 2:
		return CONFIG_UNIT_KHZ;
	case 3:
		return CONFIG_UNIT_NS;
	case 4:
		return CONFIG_UNIT_PS;
	case 5:
		return CONFIG_UNIT_US;
	case 6:
		return CONFIG_UNIT_MS;
	case 7:
		return CONFIG_UNIT_S;
	case 8:
		return CONFIG_UNIT_V;
	case 9:
		return CONFIG_UNIT_MV;
	case 10:
		return CONFIG_UNIT_B;
	case 11:
		return CONFIG_UNIT_KIB;
	case 12:
		return CONFIG_UNIT_MIB;
	}
	return NULL;
}

/* Conversion factors. */
double
unit_get_factor(struct ConfigUnit const *a_unit)
{
	if (CONFIG_UNIT_PS == a_unit) {
		return 1e-12;
	}
	if (CONFIG_UNIT_NS == a_unit) {
		return 1e-9;
	}
	if (CONFIG_UNIT_US == a_unit) {
		return 1e-6;
	}
	if (CONFIG_UNIT_MS == a_unit) {
		return 1e-3;
	}
	if (CONFIG_UNIT_MHZ == a_unit) {
		return 1e6;
	}
	if (CONFIG_UNIT_KHZ == a_unit) {
		return 1e3;
	}
	if (CONFIG_UNIT_MV == a_unit) {
		return 1e-3;
	}
	if (CONFIG_UNIT_B == a_unit) {
		return 1;
	}
	if (CONFIG_UNIT_KIB == a_unit) {
		return 1024;
	}
	if (CONFIG_UNIT_MIB == a_unit) {
		return 1024 * 1024;
	}
	return 1.0;
}

/* Conversion ratios. */
double
unit_get_mult(struct ConfigUnit const *a_from, struct ConfigUnit const *a_to)
{
	if (!unit_is_compatible(a_from, a_to)) {
		log_die(LOGL, "Trying to convert between incompatible units "
		    "(config=%u=%s, request=%u=%s).",
		    a_from->idx, unit_dump(unit_from_id(a_from->idx)),
		    a_to->idx, unit_dump(unit_from_id(a_to->idx)));
	}
	return unit_get_factor(a_from) / unit_get_factor(a_to);
}

int
unit_is_compatible(struct ConfigUnit const *a_u1, struct ConfigUnit const
    *a_u2)
{
	if (CONFIG_UNIT_NONE == a_u1) {
		return CONFIG_UNIT_NONE == a_u2;
	}
	if (CONFIG_UNIT_MHZ == a_u1 ||
	    CONFIG_UNIT_KHZ == a_u1) {
		return CONFIG_UNIT_MHZ == a_u2 ||
		    CONFIG_UNIT_KHZ == a_u2;
	}
	if (CONFIG_UNIT_NS == a_u1 ||
	    CONFIG_UNIT_PS == a_u1 ||
	    CONFIG_UNIT_US == a_u1 ||
	    CONFIG_UNIT_MS == a_u1 ||
	    CONFIG_UNIT_S == a_u1) {
		return CONFIG_UNIT_NS == a_u2 ||
		    CONFIG_UNIT_PS == a_u2 ||
		    CONFIG_UNIT_US == a_u2 ||
		    CONFIG_UNIT_MS == a_u2 ||
		    CONFIG_UNIT_S == a_u2;
	}
	if (CONFIG_UNIT_V == a_u1 ||
	    CONFIG_UNIT_MV == a_u1) {
		return CONFIG_UNIT_V == a_u2 ||
		    CONFIG_UNIT_MV == a_u2;
	}
	if (CONFIG_UNIT_B == a_u1 ||
	    CONFIG_UNIT_KIB == a_u1 ||
	    CONFIG_UNIT_MIB == a_u1) {
		return CONFIG_UNIT_B == a_u2 ||
		    CONFIG_UNIT_KIB == a_u2 ||
		    CONFIG_UNIT_MIB == a_u2;
	}
	assert(0 && "Non-existent unit.");
	return 0;
}

int
unpack_snippet(struct ConfigBlock *a_block, struct Packer *a_packer, int
    a_do_write)
{
	struct ScalarList scalar_list;
	struct ItemList *item_list;
	uint8_t item_num, item_i;

	item_list = parent_get(a_block);
	if (!unpack8(a_packer, &item_num)) {
		log_error(LOGL, "Could not unpack item_num.");
		return 0;
	}
	LOGF(verbose)(LOGL, "item_num=%d", item_num);

	TAILQ_INIT(&scalar_list);
	for (item_i = 0; item_i < item_num; ++item_i) {
		struct Item *item;
		struct Scalar *scalar;
		uint16_t name;
		uint8_t item_type;
		uint8_t scalar_num, scalar_i;

		if (!unpack8(a_packer, &item_type) ||
		    (CONFIG_BLOCK != item_type &&
		     CONFIG_CONFIG != item_type)) {
			log_error(LOGL, "Could not unpack item_type.");
			log_error(LOGL, "item_type=%s.",
			    (item_type == CONFIG_CONFIG)
			    ? "config" : "block");
			return 0;
		}
		LOGF(verbose)(LOGL, "item_type=%s.",
		    (item_type == CONFIG_CONFIG) ? "config" : "block");
		if (!unpack16(a_packer, &name) ||
		    (KW_NONE >= name || KW_THIS_IS_ALWAYS_LAST <= name)) {
			log_error(LOGL, "Could not unpack name.");
			log_error(LOGL, "name=%s.",
			    keyword_get_string(name));
			return 0;
		}
		LOGF(verbose)(LOGL, "name=%s.", keyword_get_string(name));
		if (!unpack8(a_packer, &scalar_num)) {
			log_error(LOGL, "Could not unpack scalar_num.");
			return 0;
		}
		LOGF(verbose)(LOGL, "scalar_num=%d.", scalar_num);

		item = TAILQ_END(item_list);
		scalar = NULL;
		if (CONFIG_CONFIG == item_type) {
			/* Find config, it must exist. */
			TAILQ_FOREACH(item, item_list, next) {
				if (item->name == name) {
					break;
				}
			}
			if (TAILQ_END(item_list) == item) {
				log_error(LOGL,
				    "Could not find config item %s",
				    keyword_get_string(name));
				log_error(LOGL, "Allowed config items: {");
				TAILQ_FOREACH(item, item_list, next) {
					log_error(LOGL, "%s",
					    keyword_get_string(item->name));
				}
				log_error(LOGL, "}");
				return 0;
			}
			scalar = TAILQ_FIRST(&item->scalar_list);
		}
		/* Save all scalars temporarily. */
		for (scalar_i = 0; scalar_i < scalar_num;) {
			struct Scalar *src;
			uint8_t rle_type;

			if (!unpack8(a_packer, &rle_type)) {
				log_error(LOGL, "rle_type for scalar %d.",
				    scalar_i);
				goto unpack_snippet_fail;
			}
			if (0x80 & rle_type) {
				unsigned count, vector_index;

				count = (0x7f & rle_type) + 2;
				scalar_i += count;
				if (scalar_i > scalar_num) {
					log_error(LOGL,
					    "scalar_i(%d) > scalar_num(%d).",
					    scalar_i, scalar_num);
					goto unpack_snippet_fail;
				}
				if (!unpack_snippet_scalar(a_packer,
				    &scalar_list)) {
					log_error(LOGL,
					    "unpack_snippet_scalar %d failed.",
					    scalar_i);
					goto unpack_snippet_fail;
				}
				src = TAILQ_LAST(&scalar_list, ScalarList);
				vector_index = src->vector_index + 1;
				for (--count; 0 != count; --count) {
					struct Scalar *next;

					if (NULL != scalar) {
						if (scalar->type != src->type)
						{
							log_error(LOGL,
					    "scalar type != src type "
					    "(vector_index=%d,scalar_i=%d)",
					    vector_index, scalar_i);
							goto unpack_snippet_fail;
						}
						scalar = TAILQ_NEXT(scalar,
						    next);
					}
					CALLOC(next, 1);
					memcpy(next, src, sizeof *next);
					next->vector_index = vector_index++;
					/* Gotta dup allocated stuff! */
					if (CONFIG_SCALAR_STRING ==
					    next->type) {
						next->value.s =
						    strdup(next->value.s);
					}
					TAILQ_INSERT_TAIL(&scalar_list, next,
					    next);
				}
			} else {
				unsigned count;

				count = rle_type + 1;
				scalar_i += count;
				if (scalar_i > scalar_num) {
					log_error(LOGL,
					    "scalar_i(%d) > scalar_num(%d).",
					    scalar_i, scalar_num);
					goto unpack_snippet_fail;
				}
				for (; 0 != count; --count) {
					if (!unpack_snippet_scalar(a_packer,
					    &scalar_list)) {
						log_error(LOGL,
					    "unpack_snippet_scalar %d failed. "
					    "(count=%d)",
						    scalar_i, count);
						goto unpack_snippet_fail;
					}
					src = TAILQ_LAST(&scalar_list,
					    ScalarList);
					if (NULL != src) {
						if (scalar->type != src->type)
						{
							log_error(LOGL,
						    "scalar type != src type "
						    "(count=%d,scalar_i=%d)",
						    count, scalar_i);
							goto unpack_snippet_fail;
						}
						scalar = TAILQ_NEXT(scalar,
						    next);
					}
				}
			}
		}
		if (CONFIG_BLOCK == item_type) {
			item = block_find_matching(item_list, name,
			    &scalar_list, SNIPPET_SRC);
			if (NULL == item) {
				log_error(LOGL,
				    "block_find_matching failed. "
				    "(name=%s)", keyword_get_string(name));
				goto unpack_snippet_fail;
			}
			if (!unpack_snippet((void *)item, a_packer,
			    a_do_write)) {
				log_error(LOGL, "unpack_snippet failed.");
				goto unpack_snippet_fail;
			}
		} else if (a_do_write) {
			struct Scalar *dst;

			/* Move new values on top of the old ones. */
			if (TAILQ_EMPTY(&scalar_list)) {
				parser_scalar_list_clear(&item->scalar_list);
			} else {
				dst = TAILQ_FIRST(&item->scalar_list);
				while (!TAILQ_EMPTY(&scalar_list)) {
					struct Scalar *src;

					src = TAILQ_FIRST(&scalar_list);
					for (;; dst = TAILQ_NEXT(dst, next)) {
						if (TAILQ_END(
						    &item->scalar_list) == dst
						    ||
						    src->vector_index <
						    dst->vector_index) {
			log_error(LOGL,
			    "TAILQ_END(&item->scalar_list)=%p,"
			    "dst=%p,"
			    "src->vector_index=%d,"
			    "dst->vector_index=%d",
			    (void *)TAILQ_END(&item->scalar_list),
			    (void *)dst,
			    src->vector_index,
			    dst->vector_index);
							goto unpack_snippet_fail;
						}
						if (src->vector_index ==
						    dst->vector_index) {
							break;
						}
					}
					TAILQ_REMOVE(&scalar_list, src, next);
					TAILQ_REPLACE(&item->scalar_list, dst,
					    src, next);
					if (CONFIG_SCALAR_STRING == dst->type)
					{
						free(dst->value.s);
					}
					free(dst);
				}
			}
			parser_create_bitmask(&item->scalar_list);
		} else {
			parser_scalar_list_clear(&scalar_list);
		}
	}
	return 1;
unpack_snippet_fail:
	parser_scalar_list_clear(&scalar_list);
	return 0;
}

int
unpack_snippet_scalar(struct Packer *a_packer, struct ScalarList
    *a_scalar_list)
{
	struct Scalar *scalar;
	uint16_t vector_index;
	uint8_t scalar_type;

	if (!unpack8(a_packer, &scalar_type)) {
		return 0;
	}
	if (!unpack16(a_packer, &vector_index)) {
		return 0;
	}
	CALLOC(scalar, 1);
	scalar->type = scalar_type;
	scalar->vector_index = vector_index;
	switch (scalar_type) {
	case CONFIG_SCALAR_EMPTY:
		FREE(scalar);
		break;
	case CONFIG_SCALAR_DOUBLE:
		{
			union {
				double	d;
				uint64_t	u64;
			} u;
			uint8_t unit_i;

			if (!unpack64(a_packer, &u.u64) ||
			    !unpack8(a_packer, &unit_i)) {
				FREE(scalar);
				return 0;
			}
			scalar->value.d = u.d;
			scalar->unit = unit_from_id(unit_i);
		}
		break;
	case CONFIG_SCALAR_INTEGER:
		{
			uint32_t u32;
			uint8_t unit_i;

			if (!unpack32(a_packer, &u32) ||
			    !unpack8(a_packer, &unit_i)) {
				FREE(scalar);
				return 0;
			}
			scalar->value.i = u32;
			scalar->unit = unit_from_id(unit_i);
		}
		break;
	case CONFIG_SCALAR_KEYWORD:
		{
			uint16_t keyword;

			if (!unpack16(a_packer, &keyword)) {
				FREE(scalar);
				return 0;
			}
			scalar->value.k = keyword;
		}
		break;
	case CONFIG_SCALAR_RANGE:
		{
			uint8_t first, last;

			if (!unpack8(a_packer, &first) ||
			    !unpack8(a_packer, &last)) {
				FREE(scalar);
				return 0;
			}
			scalar->value.r.first = first;
			scalar->value.r.last = last;
		}
		break;
	case CONFIG_SCALAR_STRING:
		{
			char *str;

			str = unpack_strdup(a_packer);
			if (NULL == str) {
				FREE(scalar);
				return 0;
			}
			scalar->value.s = str;
		}
		break;
	}
	if (NULL != scalar) {
		TAILQ_INSERT_TAIL(a_scalar_list, scalar, next);
	}
	return 1;
}
