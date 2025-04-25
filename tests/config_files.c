/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2021-2022, 2024
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

#include <errno.h>
#include <ntest/ntest.h>
#include <config/parser.h>
#include <nurdlib/base.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/string.h>

NTEST(AbsolutePaths)
{
	char cwd[1024];
	FILE *file;
	struct ConfigBlock *gate;
	char *path;
	size_t strsiz;
	int ret;

	path = getcwd(cwd, sizeof cwd);
	NTRY_PTR(NULL, !=, path);
	if (NULL == path) {
		log_error(LOGL, "getcwd: %s.", strerror(errno));
		return;
	}
	strsiz = strlen(cwd) + 1 + 12 + 1;
	path = malloc(strsiz);
	snprintf_(path, strsiz, "%s/absolute.cfg", cwd);
	file = fopen(path, "wb");
	NTRY_PTR(NULL, !=, file);
	if (NULL == file) {
		log_error(LOGL, "fopen: %s.", strerror(errno));
		free(path);
		return;
	}
	fprintf(file, "include \"%s/tests/single.cfg\"\n", cwd);
	fclose(file);

	config_load(path);

	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 2));
	gate = config_get_block(NULL, KW_GATE);
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    2));

	config_shutdown();
	ret = remove(path);
	free(path);
	NTRY_I(0, ==, ret);
	if (0 != ret) {
		log_error(LOGL, "remove: %s.", strerror(errno));
	}
}

NTEST(IncludeSingle)
{
	struct ConfigBlock *gate;

	config_load("tests/single.cfg");

	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 2));
	gate = config_get_block(NULL, KW_GATE);
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    2));
}

NTEST(IncludeMissingShouldSignal)
{
	NTRY_SIGNAL(config_load("tests/missing.cfg"));
}

NTEST(IncludeNested)
{
	struct ConfigBlock *gate;

	config_load("tests/nested.cfg");

	NTRY_I(1, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 2));
	gate = config_get_block(NULL, KW_GATE);
	NTRY_I(2, ==, config_get_int32(gate, KW_LEMO, CONFIG_UNIT_NONE, 0,
	    3));
	NTRY_I(2, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    3));
}

NTEST(IncludeDefault)
{
	struct ConfigBlock *gate;
	char *old;
	char const *src_path;
	int src_line_no, src_col_no;

	old = strdup_(config_default_path_get());
	config_default_path_set("tests/def");
	config_load("tests/default.cfg");
	config_default_path_set(old);
	FREE(old);

	NTRY_I(2, ==, config_get_int32(NULL, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 3));
	NTRY_I(1, ==, config_get_int32(NULL, KW_TRIGGER_INPUT,
	    CONFIG_UNIT_NONE, 0, 2));
	gate = config_get_block(NULL, KW_GATE);
	NTRY_I(2, ==, config_get_int32(gate, KW_LEMO, CONFIG_UNIT_NONE, 0,
	    3));
	NTRY_I(1, ==, config_get_int32(gate, KW_WIDTH, CONFIG_UNIT_NONE, 0,
	    2));

	parser_src_get(gate, KW_LEMO, &src_path, &src_line_no, &src_col_no);
	NTRY_STR("./tests/default.cfg", ==, src_path);
	NTRY_I(24, ==, src_line_no);
	NTRY_I(2, ==, src_col_no);
}

NTEST(IncludeAvailableUnits)
{
	config_load("tests/units.cfg");
}

NTEST(IncludeBig)
{
	enum Keyword const c_log_info[] = {KW_INFO};
	enum Keyword const c_ecl[] = {KW_ECL};
	struct ConfigBlock *crate;
	struct ConfigBlock *gate;
	int32_t clock_input[2];
	int32_t threshold[8];
	int dum;

	(void)dum;

	config_load("tests/big.cfg");

	NTRY_I(KW_INFO, ==, CONFIG_GET_KEYWORD(NULL, KW_LOG_LEVEL,
	    c_log_info));

	NTRY_SIGNAL(dum = config_get_double(NULL, KW_TIME_AFTER_TRIGGER,
	    CONFIG_UNIT_US, -1e20, 1e20));

	crate = config_get_block(NULL, KW_CRATE);
	NTRY_STR("KrazyIvan", ==, config_get_block_param_string(crate, 0));
	NTRY_I(KW_ECL, ==, CONFIG_GET_BLOCK_PARAM_KEYWORD(crate, 1, c_ecl));
	NTRY_I(-1, ==, config_get_block_param_int32(crate, 2));

	NTRY_DBL(1e-3, ==, config_get_double(crate, KW_TIME_AFTER_TRIGGER,
	    CONFIG_UNIT_US, -1e20, 1e20));
	NTRY_DBL(100, ==, config_get_double(crate, KW_WIDTH, CONFIG_UNIT_NS,
	    1e-20, 1e20));

	CONFIG_GET_INT_ARRAY(clock_input, crate, KW_CLOCK_INPUT,
	    CONFIG_UNIT_NONE, -1, 2);
	NTRY_I(2, ==, clock_input[0]);
	NTRY_I(2, ==, clock_input[1]);

	CONFIG_GET_INT_ARRAY(threshold, crate, KW_THRESHOLD, CONFIG_UNIT_NONE,
	    -10, 10);
	NTRY_I(1, ==, threshold[0]);
	NTRY_I(1, ==, threshold[1]);
	NTRY_I(1, ==, threshold[2]);
	NTRY_I(-2, ==, threshold[3]);
	NTRY_I(10, ==, threshold[4]);
	NTRY_I(-2, ==, threshold[5]);
	NTRY_I(-2, ==, threshold[6]);
	NTRY_I(0, ==, threshold[7]);

	NTRY_I(0, ==, config_get_bitmask(crate, KW_EXTERNAL, 0, 31));

	gate = config_get_block(crate, KW_GATE);
	NTRY_I(0, ==, config_get_block_param_int32(gate, 0));

	NTRY_SIGNAL(dum = config_get_int32(NULL, KW_CLOCK_INPUT,
	    CONFIG_UNIT_NONE, 0, 1));
	NTRY_SIGNAL(dum = config_get_int32(crate, KW_LOG_LEVEL,
	    CONFIG_UNIT_NONE, 0, 1));
	NTRY_I(1, ==, config_get_int32(gate, KW_CLOCK_INPUT, CONFIG_UNIT_NONE,
	    0, 2));

	NTRY_I(-1, ==, config_get_int32(crate, KW_CHANNEL0_ENABLE,
	    CONFIG_UNIT_NONE, -1, 1));
	NTRY_I(-2, ==, config_get_int32(crate, KW_CHANNEL0_ENABLE,
	    CONFIG_UNIT_NONE, -3, 3));
	NTRY_U(0x1000, ==, config_get_uint32(crate, KW_CHANNEL0_ENABLE,
	    CONFIG_UNIT_NONE, 0, 0x1000));
	NTRY_U((uint32_t)-2, ==, config_get_uint32(crate, KW_CHANNEL0_ENABLE,
	    CONFIG_UNIT_NONE, 0, UINT32_MAX));

	NTRY_I(INT32_MIN, ==, config_get_int32(crate, KW_CHANNEL1_ENABLE,
	    CONFIG_UNIT_NONE, INT32_MIN, INT32_MAX));
	NTRY_I(-1, ==, config_get_int32(crate, KW_CHANNEL1_ENABLE,
	    CONFIG_UNIT_NONE, -1, 1));
	NTRY_U(0x1000, ==, config_get_uint32(crate, KW_CHANNEL1_ENABLE,
	    CONFIG_UNIT_NONE, 0, 0x1000));
	NTRY_U(0x80000000, ==, config_get_uint32(crate, KW_CHANNEL1_ENABLE,
	    CONFIG_UNIT_NONE, 0, UINT32_MAX));
}

NTEST(IncludeAuto)
{
	struct ConfigBlock *block;

	config_auto_register(KW_GSI_VFTX2, "gsi_vftx2_mock.cfg");
	config_load("tests/module.cfg");

	block = config_get_block(NULL, KW_GSI_VFTX2);
	NTRY_I(1, ==, config_get_int32(block, KW_LEMO, CONFIG_UNIT_NONE, 0,
	    2));
	NTRY_I(2, ==, config_get_int32(block, KW_WIDTH, CONFIG_UNIT_NONE, 1,
	    3));
	NTRY_I(0, ==, config_get_bitmask(block, KW_CHANNEL_ENABLE, 0, 31));
}

NTEST(IncludeAutoRequiresFile)
{
	config_auto_register(KW_GSI_VFTX2, "rubbish.cfg");
	NTRY_SIGNAL(config_load("tests/module.cfg"));
}

NTEST(IncludeAutoCleanedOnShutdown)
{
	struct ConfigBlock *block;
	int dum;

	(void)dum;

	config_auto_register(KW_GSI_VFTX2, "gsi_vftx2_mock.cfg");
	config_shutdown();
	config_load("tests/module.cfg");

	block = config_get_block(NULL, KW_GSI_VFTX2);
	NTRY_SIGNAL(dum = config_get_int32(block, KW_LEMO,
	    CONFIG_UNIT_NONE, 0, 2));
	NTRY_I(2, ==, config_get_int32(block, KW_WIDTH, CONFIG_UNIT_NONE, 1,
	    3));
	NTRY_I(0, ==, config_get_bitmask(block, KW_CHANNEL_ENABLE, 0, 31));
}

NTEST(Snippets)
{
	char const c_gate_lemo[] = "GATE(0) { lemo = 0 }#";
	struct ConfigBlock *block;
	struct ConfigBlock *gate;
	char const *src_path;
	int src_line_no, src_col_no;

	/* Try combinations of existing and missing EOLs. */
	block = config_snippet_parse("control_port = 123\n");
	NTRY_PTR(NULL, !=, block);
	NTRY_I(123, ==, config_get_int32(block, KW_CONTROL_PORT,
	    CONFIG_UNIT_NONE, 0, 124));
	config_snippet_free(&block);

	block = config_snippet_parse("width = 2#\n");
	NTRY_I(2, ==, config_get_int32(block, KW_WIDTH, CONFIG_UNIT_NONE, 1,
	    3));
	config_snippet_free(&block);

	block = config_snippet_parse("clock_input = 1");
	NTRY_I(1, ==, config_get_int32(block, KW_CLOCK_INPUT,
	    CONFIG_UNIT_NONE, 0, 3));
	config_snippet_free(&block);

	block = config_snippet_parse(c_gate_lemo);
	gate = config_get_block(block, KW_GATE);
	NTRY_I(0, ==, config_get_int32(gate, KW_LEMO, CONFIG_UNIT_NONE, -1,
	    1));
	parser_src_get(gate, KW_LEMO, &src_path, &src_line_no, &src_col_no);
	NTRY_STR("<snippet>", ==, src_path);
	NTRY_I(1, ==, src_line_no);
	NTRY_I(1 + strstr(c_gate_lemo, "lemo") - c_gate_lemo, ==, src_col_no);
	config_snippet_free(&block);
}

NTEST(TouchedAssertion)
{
	int32_t array[3];
	enum Keyword const c_true[] = {KW_TRUE};
	struct ConfigBlock *crate;
	int dum;

	(void)dum;
	config_load("tests/type_concoction.cfg");

	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	NTRY_I(-1, ==, config_get_int32(NULL, KW_CONTROL_PORT,
	    CONFIG_UNIT_NONE, -2, 2));
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	crate = config_get_block(NULL, KW_CRATE);
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	NTRY_I(0x6, ==, config_get_bitmask(crate, KW_CLOCK_INPUT, 0, 15));
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	dum = config_get_double(crate, KW_WIDTH, CONFIG_UNIT_NONE, -2.0, 2.0);
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	NTRY_I(0xab, ==, config_get_int32(crate, KW_RANGE, CONFIG_UNIT_NONE,
	    -1, 0xff));
	NTRY_I(0, ==, config_get_bitmask(crate, KW_RANGE, 0, 31));
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	CONFIG_GET_INT_ARRAY(array, crate, KW_THRESHOLD, CONFIG_UNIT_NONE,
	    -2, 2);
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	dum = CONFIG_GET_KEYWORD(crate, KW_SUPPRESS_INVALID, c_true);
	NTRY_SIGNAL(config_touched_assert(NULL, 1));
	NTRY_STR("Bananas", ==, config_get_string(crate, KW_EXTERNAL));
	config_touched_assert(NULL, 1);
}

NTEST(CopyPaste)
{
	NTRY_SIGNAL(config_load("tests/copypaste.cfg"));
	NTRY_SIGNAL(config_load("tests/copypaste2.cfg"));
}

NTEST_SUITE(ConfigFiles)
{
	NTEST_ADD(AbsolutePaths);
	NTEST_ADD(IncludeMissingShouldSignal);
	NTEST_ADD(IncludeSingle);
	NTEST_ADD(IncludeNested);
	NTEST_ADD(IncludeDefault);
	NTEST_ADD(IncludeAvailableUnits);
	NTEST_ADD(IncludeBig);
	NTEST_ADD(IncludeAuto);
	NTEST_ADD(IncludeAutoRequiresFile);
	NTEST_ADD(IncludeAutoCleanedOnShutdown);
	NTEST_ADD(Snippets);
	NTEST_ADD(TouchedAssertion);
	NTEST_ADD(CopyPaste);

	config_shutdown();
}
