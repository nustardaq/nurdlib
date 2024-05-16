/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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

#include <stdio.h>
#include <ntest/ntest.h>
#include <module/dummy/internal.h>
#include <module/dummy/offsets.h>
#include <module/map/map.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <tests/log_intercept.h>
#include <util/math.h>

struct ModuleEntry {
	struct	Module *module;
	TAILQ_ENTRY(ModuleEntry)	next;
};
TAILQ_HEAD(ModuleEntryList, ModuleEntry);

static void	init_callback(void);
static void	destroy(struct Module *);

static int g_is_destroyed;

void
init_callback(void)
{
	LOGF(info)(LOGL, "TestMania");
	LOGF(verbose)(LOGL, "TestMania");
	LOGF(debug)(LOGL, "TestMania");
	LOGF(spam)(LOGL, "TestMania");
}

void
destroy(struct Module *a_module)
{
	(void)a_module;
	g_is_destroyed = 1;
}

NTEST(BaseCreateFree)
{
	struct ModuleProps props;
	struct Module *module;

	g_is_destroyed = 0;
	props.destroy = destroy;
	module = module_create_base(sizeof *module, &props);
	NTRY_PTR(NULL, !=, module);
	module_free(&module);
	NTRY_PTR(NULL, ==, module);
	NTRY_BOOL(g_is_destroyed);
}

NTEST(Pedestals)
{
	struct Pedestal pedestal;
	size_t i;

	memset(&pedestal, 0, sizeof pedestal);
	for (i = 0; PEDESTAL_BUF_LEN > i; ++i) {
		if (0 == (0x100 & rand())) {
			module_pedestal_add(&pedestal, 0);
		} else {
			module_pedestal_add(&pedestal, 10 +
			    (3 & rand()));
		}
	}
	NTRY_BOOL(module_pedestal_calculate(&pedestal));
	NTRY_I(10, <, pedestal.threshold);
}

NTEST(LogLevel)
{
	char mem[MAP_SIZE + 5];
	enum Keyword const c_set_info[] = {
		KW_INFO,
		KW_INFO,
		KW_INFO, KW_VERBOSE,
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM
	};
	enum Keyword const c_set_verbose[] = {
		KW_INFO, KW_VERBOSE,
		KW_INFO, KW_VERBOSE,
		KW_INFO, KW_VERBOSE,
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM
	};
	enum Keyword const c_set_debug[] = {
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM
	};
	enum Keyword const c_set_spam[] = {
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM,
		KW_INFO, KW_VERBOSE, KW_DEBUG, KW_SPAM
	};
	struct Crate *crate;
	size_t i, test_mania_num;

	module_setup();
	map_setup();
	map_user_add(0x0, mem, sizeof mem);
	crate_setup();
	config_load("tests/module_logleveled.cfg");
	crate = crate_create();
	crate_init(crate);

	for (i = 0; 5 > i; ++i) {
		struct Module *module;

		module = crate_module_find(crate, KW_DUMMY, i);
		dummy_init_set_callback(module, init_callback);
	}

	log_callback_set(log_intercept);

	/* Info. */
	log_intercept_clear();
	crate_deinit(crate);
	crate_init(crate);
	test_mania_num = 0;
	for (i = 0; log_intercept_get_num() > i; ++i) {
		char const *str;

		str = log_intercept_get_str(i);
		if (NULL != strstr(str, "TestMania")) {
			NTRY_I(LENGTH(c_set_info), >, test_mania_num);
			NTRY_I(log_intercept_get_level(i), ==,
			    c_set_info[test_mania_num]);
			++test_mania_num;
		}
	}
	NTRY_I(LENGTH(c_set_info), ==, test_mania_num);

	/* Verbose. */
	log_level_push(g_log_level_verbose_);
	log_intercept_clear();
	crate_deinit(crate);
	crate_init(crate);
	test_mania_num = 0;
	for (i = 0; log_intercept_get_num() > i; ++i) {
		char const *str;

		str = log_intercept_get_str(i);
		if (NULL != strstr(str, "TestMania")) {
			NTRY_I(LENGTH(c_set_verbose), >, test_mania_num);
			NTRY_I(log_intercept_get_level(i), ==,
			    c_set_verbose[test_mania_num]);
			++test_mania_num;
		}
	}
	NTRY_I(LENGTH(c_set_verbose), ==, test_mania_num);
	log_level_pop();

	/* Debug. */
	log_level_push(g_log_level_debug_);
	log_intercept_clear();
	crate_deinit(crate);
	crate_init(crate);
	test_mania_num = 0;
	for (i = 0; log_intercept_get_num() > i; ++i) {
		char const *str;

		str = log_intercept_get_str(i);
		if (NULL != strstr(str, "TestMania")) {
			NTRY_I(LENGTH(c_set_debug), >, test_mania_num);
			NTRY_I(log_intercept_get_level(i), ==,
			    c_set_debug[test_mania_num]);
			++test_mania_num;
		}
	}
	NTRY_I(LENGTH(c_set_debug), ==, test_mania_num);
	log_level_pop();

	/* Spam. */
	log_level_push(g_log_level_spam_);
	log_intercept_clear();
	crate_deinit(crate);
	crate_init(crate);
	test_mania_num = 0;
	for (i = 0; log_intercept_get_num() > i; ++i) {
		char const *str;

		str = log_intercept_get_str(i);
		if (NULL != strstr(str, "TestMania")) {
			NTRY_I(LENGTH(c_set_spam), >, test_mania_num);
			NTRY_I(log_intercept_get_level(i), ==,
			    c_set_spam[test_mania_num]);
			++test_mania_num;
		}
	}
	NTRY_I(LENGTH(c_set_spam), ==, test_mania_num);
	log_level_pop();

	log_callback_set(NULL);

	crate_free(&crate);
	map_user_clear();
	map_shutdown();
	config_shutdown();
}

NTEST_SUITE(Module)
{
	NTEST_ADD(BaseCreateFree);
	NTEST_ADD(Pedestals);
	NTEST_ADD(LogLevel);
}
