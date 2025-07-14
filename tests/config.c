/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2025
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

#include <math.h>
#include <ntest/ntest.h>
#include <nurdlib/base.h>
#include <nurdlib/config.h>
#include <config/parser.h>
#include <nurdlib/log.h>

NTEST(EmptyBlockOk)
{
	struct ConfigBlock *block;

	parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_block();
	parser_pop_block();

	block = config_get_block(NULL, KW_CRATE);
	NTRY_PTR(NULL, !=, block);
	NTRY_I(KW_CRATE, ==, config_get_block_name(block));
}

NTEST(NestedBlockHierarchy)
{
	struct ConfigBlock *outer, *inner;

	parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_block();
		parser_prepare_block(KW_GATE, __FILE__, __LINE__, 1);
		parser_push_block();
		parser_pop_block();
	parser_pop_block();

	outer = config_get_block(NULL, KW_CRATE);
	NTRY_PTR(NULL, ==, config_get_block(NULL, KW_GATE));

	NTRY_PTR(NULL, ==, config_get_block(outer, KW_CRATE));

	inner = config_get_block(outer, KW_GATE);
	NTRY_PTR(NULL, !=, inner);

	NTRY_PTR(NULL, ==, config_get_block(inner, KW_CRATE));
	NTRY_PTR(NULL, ==, config_get_block(inner, KW_GATE));
}

NTEST(ConfigNameCollisionOverwrites)
{
	struct ScalarList *list;

	/* Filenames cannot be identical, make them unique. */
	list = parser_push_config(KW_WIDTH, -1, "a"__FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 0);

	list = parser_push_config(KW_WIDTH, -1, "b"__FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);

	NTRY_I(1, ==, config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 2));
}

NTEST(BlockTraversalInOrder)
{
	struct ScalarList *list;
	struct ConfigBlock *block;

	list = parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_block();
	parser_pop_block();

	list = parser_prepare_block(KW_GATE, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_block();
	parser_pop_block();

	block = config_get_block(NULL, KW_NONE);
	NTRY_I(KW_CRATE, ==, config_get_block_name(block));
	NTRY_I(1, ==, config_get_block_param_int32(block, 0));

	block = config_get_block_next(block, KW_NONE);
	NTRY_I(KW_GATE, ==, config_get_block_name(block));
	NTRY_I(2, ==, config_get_block_param_int32(block, 0));
}

NTEST(BlockNameCollisionParamDiffInOrder)
{
	const enum Keyword c_lemo[] = {KW_LEMO};
	struct ScalarList *list;
	struct ConfigBlock *block;

	list = parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_block();
	parser_pop_block();

	list = parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_keyword(list, 0, KW_LEMO);
	parser_push_block();
	parser_pop_block();

	list = parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_string(list, 0, "FlyingPianos");
	parser_push_block();
	parser_pop_block();

	block = config_get_block(NULL, KW_NONE);
	NTRY_I(1, ==, config_get_block_param_int32(block, 0));
	block = config_get_block_next(block, KW_NONE);
	NTRY_I(KW_LEMO, ==, CONFIG_GET_BLOCK_PARAM_KEYWORD(block, 0, c_lemo));
	block = config_get_block_next(block, KW_NONE);
	NTRY_STR("FlyingPianos", ==, config_get_block_param_string(block,
	    0));
	block = config_get_block_next(block, KW_CRATE);
	NTRY_PTR(NULL, ==, block);
}

NTEST(BlockNameAndParamCollisionOverwrites)
{
	struct ScalarList *list;
	struct ConfigBlock *crate;

	/* File-names cannot be identical, make them unique. */
	list = parser_prepare_block(KW_CRATE, "a"__FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_block();
		list = parser_push_config(KW_WIDTH, -1, "a"__FILE__, __LINE__,
		    1);
		parser_push_int32(list, 0, 1);
	parser_pop_block();

	list = parser_prepare_block(KW_CRATE, "b"__FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_block();
		list = parser_push_config(KW_WIDTH, -1, "b"__FILE__, __LINE__,
		    1);
		parser_push_int32(list, 0, 2);
	parser_pop_block();

	crate = config_get_block(NULL, KW_NONE);
	NTRY_I(1, ==, config_get_block_param_int32(crate, 0));
	NTRY_I(2, ==, config_get_int32(crate, KW_WIDTH, CONFIG_UNIT_NONE,
	    1, 3));
	NTRY_PTR(NULL, ==, config_get_block_next(crate, KW_NONE));
}

NTEST(BlockNameFilter)
{
	struct ScalarList *list;
	struct ConfigBlock *block;

	list = parser_prepare_block(KW_CRATE, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_block();
	parser_pop_block();

	list = parser_prepare_block(KW_GATE, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_block();
	parser_pop_block();

	block = config_get_block(NULL, KW_CRATE);
	NTRY_I(1, ==, config_get_block_param_int32(block, 0));

	block = config_get_block(NULL, KW_GATE);
	NTRY_I(2, ==, config_get_block_param_int32(block, 0));
}

NTEST(BlockParamOutOfRange)
{
	struct ScalarList *list;
	struct ConfigBlock *crate;
	int32_t dum;

	(void)dum;

	list = parser_prepare_block(KW_CRATE, "froot.cfg", 321, 654);
	parser_push_int32(list, 0, 1);
	parser_push_block();
	parser_pop_block();

	crate = config_get_block(NULL, KW_NONE);
	NTRY_I(1, ==, config_get_block_param_int32(crate, 0));
	NTRY_SIGNAL(dum = config_get_block_param_int32(crate, -1));
	NTRY_SIGNAL(dum = config_get_block_param_int32(crate, 1));
}

NTEST(PersistedDouble)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_double(list, 0, 1.0);
	NTRY_DBL(1.0, ==, config_get_double(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    -1e20, 1e20));
}

NTEST(PersistedInteger)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	NTRY_I(1, ==, config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 2));
}

NTEST(PersistedKeyword)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_keyword(list, 0, KW_LEMO);
	{
		enum Keyword kw[] = {KW_LEMO};
		NTRY_I(KW_LEMO, ==, CONFIG_GET_KEYWORD(NULL, KW_WIDTH, kw));
	}
}

NTEST(PersistedMixedBitmask)
{
	struct ScalarList *list;
	uint32_t bitmask;

	/* Simulate "(2..3, 5, 7..9) = 0x3ac". */
	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_range(list, 0, 2, 3);
	parser_push_int32(list, 1, 5);
	parser_push_range(list, 2, 7, 9);
	parser_create_bitmask(list);
	bitmask = config_get_bitmask(NULL, KW_WIDTH, 0, 31);
	NTRY_U(0x3ac, ==, bitmask);
}

NTEST(PersistedString)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_string(list, 0, "Peanutz");
	NTRY_STR("Peanutz", ==, config_get_string(NULL, KW_WIDTH));
}

NTEST(Int32Sign)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, -1);
	NTRY_I(-1, ==, config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    -2, 2));
}

NTEST(Uint32Sign)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, -1);
	NTRY_I(0xffffffff, ==, config_get_uint32(NULL, KW_WIDTH,
	    CONFIG_UNIT_NONE, 0, 0xffffffff));
}

NTEST(KeywordFilter)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, "abblez.cfg", 123, 456);
	parser_push_keyword(list, 0, KW_LEMO);
	{
		enum Keyword const c_kw[] = {KW_TRUE};
		int dum;

		(void)dum;
		NTRY_SIGNAL(dum = CONFIG_GET_KEYWORD(NULL, KW_WIDTH, c_kw));
	}
}

NTEST(Boolean)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, "abblez.cfg", 123, 456);
	parser_push_keyword(list, 0, KW_TRUE);
	list = parser_push_config(KW_LEMO, -1, "abblez.cfg", 124, 567);
	parser_push_keyword(list, 0, KW_FALSE);
	NTRY_BOOL(config_get_boolean(NULL, KW_WIDTH));
	NTRY_BOOL(!config_get_boolean(NULL, KW_LEMO));
}

NTEST(Bitmasks)
{
	struct ScalarList *list;
	uint32_t bitmask;

	list = parser_push_config(KW_CLOCK_INPUT, -1, __FILE__, __LINE__, 1);
	parser_create_bitmask(list);
	bitmask = config_get_bitmask(NULL, KW_CLOCK_INPUT, 0, 31);
	NTRY_U(0, ==, bitmask);

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_range(list, 0, 1, 30);
	parser_create_bitmask(list);
	bitmask = config_get_bitmask(NULL, KW_WIDTH, 0, 31);
	NTRY_U(0x7ffffffe, ==, bitmask);

	list = parser_push_config(KW_LOG_LEVEL, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	parser_push_range(list, 1, 3, 4);
	parser_create_bitmask(list);
	bitmask = config_get_bitmask(NULL, KW_LOG_LEVEL, 0, 31);
	NTRY_U(0x1a, ==, bitmask);
}

NTEST(DoubleFromIntegerOk)
{
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	NTRY_DBL(1, ==, config_get_double(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 2));
}

NTEST(IntegerFromDoubleNotOk)
{
	struct ScalarList *list;
	int dum;

	(void)dum;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_double(list, 0, 1);
	NTRY_SIGNAL(dum = config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 2));
}

NTEST(UndefinedConfigsShouldSignalButBlockNot)
{
	enum Keyword const c_list[] = {KW_WIDTH};
	int dum;

	(void)dum;

	NTRY_SIGNAL(dum = config_get_bitmask(NULL, KW_WIDTH, 0, 31));
	NTRY_SIGNAL(dum = config_get_double(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0.0, 0.0));
	NTRY_SIGNAL(dum = config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 0));
	NTRY_SIGNAL(dum = config_get_uint32(NULL, KW_WIDTH, CONFIG_UNIT_NONE,
	    0, 0));
	NTRY_SIGNAL(dum = CONFIG_GET_KEYWORD(NULL, KW_WIDTH, c_list));
	NTRY_SIGNAL(dum = (intptr_t)config_get_string(NULL, KW_WIDTH));

	NTRY_PTR(NULL, ==, config_get_block(NULL, KW_CRATE));
}

NTEST(DoubleRange)
{
	struct Limits {
		double	value;
		double	min;
		double	max;
		double	result;
	};
	struct Limits limits[] = {
		{-3.0, -2.0,  0.0, -2.0},
		{-1.0, -2.0,  0.0, -1.0},
		{+1.0, -2.0,  0.0,  0.0},
		{-1.0,  0.0, +2.0,  0.0},
		{+1.0,  0.0, +2.0, +1.0},
		{+3.0,  0.0, +2.0, +2.0}
	};
	struct ScalarList *list;
	/* Identical file-names are not allowed, make them unique. */
	char name[] = "a";
	size_t i;

	for (i = 0; i < LENGTH(limits); ++i) {
		struct Limits *l;

		l = &limits[i];
		list = parser_push_config(KW_WIDTH, -1, name, __LINE__, 1);
		parser_push_double(list, 0, l->value);
		NTRY_DBL(l->result, ==, config_get_double(NULL, KW_WIDTH,
		    CONFIG_UNIT_NONE, l->min, l->max));
		++name[0];
	}
}

NTEST(Int32Range)
{
	struct Limits {
		int32_t	value;
		int32_t	min;
		int32_t	max;
		int32_t	result;
	};
	struct Limits limits[] = {
		{INT32_MIN,   INT32_MIN,          -1,   INT32_MIN},
		{INT32_MIN, INT32_MIN/2,          -1, INT32_MIN/2},
		{        0,   INT32_MIN,          -1,          -1},
		{INT32_MAX,   INT32_MIN,          -1,          -1},
		{INT32_MIN,           1,   INT32_MAX,           1},
		{        0,           1,   INT32_MAX,           1},
		{INT32_MAX,           1, INT32_MAX/2, INT32_MAX/2},
		{INT32_MAX,           1,   INT32_MAX,   INT32_MAX}
	};
	struct ScalarList *list;
	char name[] = "a";
	size_t i;

	for (i = 0; i < LENGTH(limits); ++i) {
		struct Limits *l;

		l = &limits[i];
		list = parser_push_config(KW_WIDTH, -1, name, __LINE__, 1);
		parser_push_int32(list, 0, l->value);
		NTRY_I(l->result, ==, config_get_int32(NULL, KW_WIDTH,
		    CONFIG_UNIT_NONE, l->min, l->max));
		++name[0];
	}
}

NTEST(Uint32Range)
{
	struct Limits {
		uint32_t	value;
		uint32_t	min;
		uint32_t	max;
		uint32_t	result;
	};
	struct Limits limits[] = {
		{         0, 0,   UINT32_MAX,            0},
		{UINT32_MAX, 0,   UINT32_MAX,   UINT32_MAX},
		{UINT32_MAX, 0, UINT32_MAX/2, UINT32_MAX/2}
	};
	struct ScalarList *list;
	char name[] = "a";
	size_t i;

	for (i = 0; i < LENGTH(limits); ++i) {
		struct Limits *l;

		l = &limits[i];
		list = parser_push_config(KW_WIDTH, -1, name, __LINE__, 1);
		parser_push_int32(list, 0, l->value);
		NTRY_I(l->result, ==, config_get_uint32(NULL, KW_WIDTH,
		    CONFIG_UNIT_NONE, l->min, l->max));
		++name[0];
	}
}

NTEST(BitmaskRange)
{
	struct ScalarList *list;
	uint32_t bitmask;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_range(list, 0, 0, 31);
	parser_create_bitmask(list);
	bitmask = config_get_bitmask(NULL, KW_WIDTH, 7, 18);
	NTRY_U(0x0007ff80, ==, bitmask);
	bitmask = config_get_bitmask(NULL, KW_WIDTH, 1, 30);
	NTRY_U(0x7ffffffe, ==, bitmask);
}

NTEST(UnitConversion)
{
	struct ScalarList *list;
	int dum;

	(void)dum;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 1);
	NTRY_SIGNAL(dum = config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NS, 0,
	    1001));
	parser_push_unit(list, 0, CONFIG_UNIT_US);
	NTRY_I(1000, ==, config_get_int32(NULL, KW_WIDTH, CONFIG_UNIT_NS,
	    0, 1001));
}

NTEST(ArraySize)
{
	int array1[1];
	int array2[2];
	int array3[3];
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_int32(list, 1, 1);

	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array1, NULL, KW_WIDTH,
	    CONFIG_UNIT_NONE, 0, 3));
	CONFIG_GET_INT_ARRAY(array2, NULL, KW_WIDTH, CONFIG_UNIT_NONE, 0, 2);
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array3, NULL, KW_WIDTH,
	    CONFIG_UNIT_NONE, 0, 3));
	NTRY_I(2, ==, array2[0]);
	NTRY_I(1, ==, array2[1]);
}

NTEST(ArrayTypes)
{
	double darr[2];
	int iarr[2];
	struct ScalarList *list;

	list = parser_push_config(KW_LEMO, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_int32(list, 1, 1);

	CONFIG_GET_INT_ARRAY(iarr, NULL, KW_LEMO, CONFIG_UNIT_NONE, 0, 3);
	NTRY_I(iarr[0], ==, 2);
	NTRY_I(iarr[1], ==, 1);
	CONFIG_GET_DOUBLE_ARRAY(darr, NULL, KW_LEMO, CONFIG_UNIT_NONE, 0, 3);
	NTRY_DBL(darr[0], ==, 2);
	NTRY_DBL(darr[1], ==, 1);

	list = parser_push_config(KW_NIM, -1, __FILE__, __LINE__, 1);
	parser_push_double(list, 0, 0.1);
	parser_push_double(list, 1, 0.2);

	CONFIG_GET_INT_ARRAY(iarr, NULL, KW_NIM, CONFIG_UNIT_NONE, 0, 3);
	NTRY_I(iarr[0], ==, 0);
	NTRY_I(iarr[1], ==, 0);
	CONFIG_GET_DOUBLE_ARRAY(darr, NULL, KW_NIM, CONFIG_UNIT_NONE, 0, 3);
	NTRY_DBL(darr[0], ==, 0.1);
	NTRY_DBL(darr[1], ==, 0.2);

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_double(list, 1, 1.1);

	CONFIG_GET_INT_ARRAY(iarr, NULL, KW_WIDTH, CONFIG_UNIT_NONE, 0, 3);
	NTRY_I(iarr[0], ==, 2);
	NTRY_I(iarr[1], ==, 1);
	CONFIG_GET_DOUBLE_ARRAY(darr, NULL, KW_WIDTH, CONFIG_UNIT_NONE, 0, 3);
	NTRY_DBL(darr[0], ==, 2);
	NTRY_DBL(darr[1], ==, 1.1);
}

NTEST(ArrayUnits)
{
	int array[2];
	struct ScalarList *list;

	list = parser_push_config(KW_WIDTH, -1, __FILE__, __LINE__, 1);
	parser_push_int32(list, 0, 2);
	parser_push_unit(list, 0, CONFIG_UNIT_US);
	parser_push_int32(list, 1, 1);
	parser_push_unit(list, 1, CONFIG_UNIT_NS);

	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_NONE, 0, 3));
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_MHZ, 0, 3));
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_KHZ, 0, 3));
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_V, 0, 3));
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_MV, 0, 3));
	NTRY_SIGNAL(CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH,
	    CONFIG_UNIT_FC, 0, 3));
	CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH, CONFIG_UNIT_US, 0, 3);
	NTRY_I(2, ==, array[0]);
	NTRY_I(0, ==, array[1]);
	CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH, CONFIG_UNIT_NS, 0, 3e3);
	NTRY_I(2e3, ==, array[0]);
	NTRY_I(1, ==, array[1]);
	CONFIG_GET_INT_ARRAY(array, NULL, KW_WIDTH, CONFIG_UNIT_PS, 0, 3e6);
	NTRY_I(2e6, ==, array[0]);
	NTRY_I(1e3, ==, array[1]);
}

NTEST(SourceLocation)
{
	struct ScalarList *list;
	char const *src_path;
	int src_line_no, src_col_no;

	list = parser_push_config(KW_WIDTH, -1, "oranje", 10, 20);
	parser_push_int32(list, 0, 1);
	parser_src_get(NULL, KW_WIDTH, &src_path, &src_line_no, &src_col_no);
	NTRY_STR("oranje", ==, src_path);
	NTRY_I(10, ==, src_line_no);
	NTRY_I(20, ==, src_col_no);
}

NTEST(UnitIndex)
{
	int idx;

	idx = 0;
#define UNIT_CHECK(name)\
	NTRY_I(CONFIG_UNIT_##name->idx, ==, idx);\
	++idx
	UNIT_CHECK(NONE);
	UNIT_CHECK(MHZ);
	UNIT_CHECK(KHZ);
	UNIT_CHECK(NS);
	UNIT_CHECK(PS);
	UNIT_CHECK(US);
	UNIT_CHECK(MS);
	UNIT_CHECK(S);
	UNIT_CHECK(V);
	UNIT_CHECK(MV);
}

NTEST(DuplicateAutoBad)
{
	config_auto_register(KW_GSI_VFTX2, "gsi_vftx2_mock.cfg");
	NTRY_SIGNAL(config_auto_register(KW_GSI_VFTX2, "gsi_vftx2_mock.cfg"));
}

NTEST(MergeConfig)
{
	char buf[100];
	struct Packer packer;
	struct ConfigBlock *gate;

	config_load("tests/single.cfg");
	gate = config_get_block(NULL, KW_GATE);

	/* TODO: Test RLE more. */

LOGF(info)(LOGL, "%d", __LINE__);
	/* Empty packer should be ok. */
	PACKER_CREATE_STATIC(packer, buf);
	packer.bytes = 0;
	NTRY_BOOL(config_merge(gate, &packer));
	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 2));
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    2));
	NTRY_I(2, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* Setting a non-existing config (adding) is not ok! */
	PACKER_CREATE_STATIC(packer, buf);
	/* The packed format is implemented in config/config.c. */
	NTRY_BOOL(pack16(&packer, 1));                    /* Num configs. */
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));         /* Type = config. */
	NTRY_BOOL(pack16(&packer, KW_CLOCK_INPUT));       /* Name. */
	NTRY_BOOL(pack16(&packer, 1));                    /* Num scalars. */
	NTRY_BOOL(pack8(&packer, 0));                     /* Raw RLE. */
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER)); /* Scalar type. */
	NTRY_BOOL(pack16(&packer, 0));                    /* Vector index. */
	NTRY_BOOL(pack32(&packer, 2));                    /* Value. */
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx)); /* Unit. */
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(!config_merge(gate, &packer));
	/* Merging should be atomic, a bad merge should leave no traces. */
	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(2, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* Corrupt packer data is also not ok! */
	PACKER_CREATE_STATIC(packer, buf);
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));
	NTRY_BOOL(pack16(&packer, KW_WIDTH));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 2));
	/* Leave out the unit. */
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(!config_merge(gate, &packer));
	/* Again, no traces. */
	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(2, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* The torture continues, double config (copy-paste?) also not ok! */
	PACKER_CREATE_STATIC(packer, buf);
	NTRY_BOOL(pack16(&packer, 2));
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));
	NTRY_BOOL(pack16(&packer, KW_CLOCK_INPUT));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 2));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));
	NTRY_BOOL(pack16(&packer, KW_CLOCK_INPUT));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 3));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(!config_merge(NULL, &packer));
	/* Merging should be atomic, a bad merge should leave no traces. */
	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(2, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* Finally, one nice-guy merge. */
	PACKER_CREATE_STATIC(packer, buf);
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));
	NTRY_BOOL(pack16(&packer, KW_WIDTH));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 2));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(config_merge(gate, &packer));
	/* This time we want a trace... */
	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(2, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(4, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* And another one, global. */
	PACKER_CREATE_STATIC(packer, buf);
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG));
	NTRY_BOOL(pack16(&packer, KW_CLOCK_INPUT));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 2));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(config_merge(NULL, &packer));
	NTRY_I(2, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(2, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(4, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));

LOGF(info)(LOGL, "%d", __LINE__);
	/* And one with a block. */
	PACKER_CREATE_STATIC(packer, buf);
	NTRY_BOOL(pack16(&packer, 2)); /* clock_input + GATE = 2 items. */
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG)); /* 0 = clock_input. */
	NTRY_BOOL(pack16(&packer, KW_CLOCK_INPUT));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 3));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	NTRY_BOOL(pack8(&packer, CONFIG_BLOCK)); /* 1 = GATE. */
	NTRY_BOOL(pack16(&packer, KW_GATE));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack16(&packer, 1)); /* width = 1 item. */
	NTRY_BOOL(pack8(&packer, CONFIG_CONFIG)); /* 0 = width. */
	NTRY_BOOL(pack16(&packer, KW_WIDTH));
	NTRY_BOOL(pack16(&packer, 1));
	NTRY_BOOL(pack8(&packer, 0));
	NTRY_BOOL(pack8(&packer, CONFIG_SCALAR_INTEGER));
	NTRY_BOOL(pack16(&packer, 0));
	NTRY_BOOL(pack32(&packer, 3));
	NTRY_BOOL(pack8(&packer, CONFIG_UNIT_NONE->idx));
	packer.bytes = packer.ofs;
	packer.ofs = 0;
	NTRY_BOOL(config_merge(NULL, &packer));
	NTRY_I(3, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 4));
	NTRY_I(3, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    4));
	NTRY_I(8, ==, config_get_bitmask(gate, KW_WIDTH, 0, 31));
}

NTEST_SUITE(Config)
{
	NTEST_ADD(EmptyBlockOk);
	NTEST_ADD(NestedBlockHierarchy);
	NTEST_ADD(ConfigNameCollisionOverwrites);
	NTEST_ADD(BlockTraversalInOrder);
	NTEST_ADD(BlockNameCollisionParamDiffInOrder);
	NTEST_ADD(BlockNameAndParamCollisionOverwrites);
	NTEST_ADD(BlockNameFilter);
	NTEST_ADD(BlockParamOutOfRange);
	NTEST_ADD(PersistedDouble);
	NTEST_ADD(PersistedInteger);
	NTEST_ADD(PersistedKeyword);
	NTEST_ADD(PersistedMixedBitmask);
	NTEST_ADD(PersistedString);
	NTEST_ADD(Int32Sign);
	NTEST_ADD(Uint32Sign);
	NTEST_ADD(KeywordFilter);
	NTEST_ADD(Boolean);
	NTEST_ADD(Bitmasks);
	NTEST_ADD(DoubleFromIntegerOk);
	NTEST_ADD(IntegerFromDoubleNotOk);
	NTEST_ADD(UndefinedConfigsShouldSignalButBlockNot);
	NTEST_ADD(DoubleRange);
	NTEST_ADD(Int32Range);
	NTEST_ADD(Uint32Range);
	NTEST_ADD(BitmaskRange);
	NTEST_ADD(UnitConversion);
	NTEST_ADD(ArraySize);
	NTEST_ADD(ArrayTypes);
	NTEST_ADD(ArrayUnits);
	NTEST_ADD(SourceLocation);
	NTEST_ADD(UnitIndex);
	NTEST_ADD(DuplicateAutoBad);
	NTEST_ADD(MergeConfig);

	config_shutdown();
}
