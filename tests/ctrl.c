/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2025
 * Bastian Löher
 * Håkan T Johansson
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

#include <ntest/ntest.h>
#include <util/stdint.h>
#include <crate/internal.h>
#include <ctrl/ctrl.h>
#include <module/module.h>
#include <nurdlib.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/endian.h>
#include <util/pack.h>

NTEST(OnlineStatus)
{
	struct CtrlClient *client;
	struct CtrlServer *server;

	config_load("tests/ctrl_yes.cfg");

	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);
	NTRY_BOOL(!ctrl_client_is_online(client));

	server = ctrl_server_create();
	NTRY_BOOL(ctrl_client_is_online(client));

	ctrl_server_free(&server);
	NTRY_PTR(NULL, ==, server);
	NTRY_BOOL(!ctrl_client_is_online(client));

	ctrl_client_free(&client);
	NTRY_PTR(NULL, ==, client);

	config_shutdown();
}

NTEST(EmptyCrate)
{
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct CtrlCrateArray crate_array;
	struct Crate *crate;
	struct CtrlCrate const *ctrl_crate;
	int ret;

	/* Load one empty crate. */
	crate_setup();
	module_setup();
	config_load("tests/crate_empty.cfg");
	crate = crate_create();

	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);
	NTRY_PTR(NULL, !=, server);
	NTRY_PTR(NULL, !=, client);

	/* There should be one crate with specific data. */
	ret = ctrl_client_crate_array_get(client, &crate_array);
	NTRY_I(1, ==, ret);
	NTRY_I(1, ==, crate_array.num);
	ctrl_crate = &crate_array.array[0];
	NTRY_STR("AyeBeEmpty", ==, ctrl_crate->name);

	ctrl_client_crate_array_free(&crate_array);

	ctrl_client_free(&client);
	ctrl_server_free(&server);

	crate_free(&crate);
	config_shutdown();
}

NTEST(SimpleCrate)
{
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct CtrlCrateArray crate_array;
	struct Crate *crate;
	struct CtrlCrate const *ctrl_crate;
	struct CtrlModule const *ctrl_module;

	/* Load one crate with a single module. */
	crate_setup();
	module_setup();
	config_load("tests/crate_simple.cfg");
	crate = crate_create();

	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);

	/* There should be one crate with specific data. */
	ctrl_client_crate_array_get(client, &crate_array);
	NTRY_I(1, ==, crate_array.num);
	ctrl_crate = &crate_array.array[0];
	NTRY_STR("Simple", ==, ctrl_crate->name);

	/* And two specific modules with a barrier in between. */
	NTRY_I(3, ==, ctrl_crate->module_num);
	ctrl_module = ctrl_crate->module_array;
	NTRY_I(KW_CAEN_V775, ==, ctrl_module->type);
	++ctrl_module;
	NTRY_I(KW_BARRIER, ==, ctrl_module->type);
	++ctrl_module;
	NTRY_I(KW_MESYTEC_MADC32, ==, ctrl_module->type);

	ctrl_client_crate_array_free(&crate_array);

	ctrl_client_free(&client);
	ctrl_server_free(&server);

	crate_free(&crate);
	config_shutdown();
}

NTEST(UnknownCrate)
{
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct Crate *crate;
	struct CtrlCrateInfo crate_info;

	crate_setup();
	module_setup();
	config_load("tests/crate_simple.cfg");
	crate = crate_create();

	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);

	/* Ask for ghost crate. */
	ZERO(crate_info);
	ctrl_client_crate_info_get(client, &crate_info, 1);
	NTRY_I(0, ==, crate_info.event_max_override);
	NTRY_I(0, ==, crate_info.dt_release);
	NTRY_I(0, ==, crate_info.acvt);
	NTRY_I(0, ==, crate_info.shadow.buf_bytes);
	NTRY_I(0, ==, crate_info.shadow.max_bytes);

	ctrl_client_free(&client);
	ctrl_server_free(&server);

	crate_free(&crate);
	config_shutdown();
}

NTEST(UnknownModule)
{
	struct CtrlRegisterArray reg_array;
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct Crate *crate;

	crate_setup();
	module_setup();
	config_load("tests/crate_simple.cfg");
	crate = crate_create();

	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);

	/* Ask for ghost module. */
	NTRY_BOOL(!ctrl_client_register_array_get(client, &reg_array, 0, 3,
	    -1));

	ctrl_client_free(&client);
	ctrl_server_free(&server);

	crate_free(&crate);
	config_shutdown();
}

NTEST(UnsupportedModule)
{
	struct CtrlRegisterArray reg_array;
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct Crate *crate;

	crate_setup();
	module_setup();
	config_load("tests/crate_sam.cfg");
	crate = crate_create();

	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);

	NTRY_BOOL(!ctrl_client_register_array_get(client, &reg_array, 0, 0,
	    -1));

	ctrl_client_free(&client);
	ctrl_server_free(&server);

	crate_free(&crate);
	config_shutdown();
}

NTEST(Confed)
{
	struct CtrlClient *client;
	struct CtrlServer *server;
	int i;

	/* Try a few times to be sure... */
	for (i = 0; 2 > i; ++i) {
		config_load("tests/ctrl_yes.cfg");
		server = ctrl_server_create();
		client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT +
		    1);
		NTRY_BOOL(ctrl_client_is_online(client));
		ctrl_client_free(&client);
		ctrl_server_free(&server);
		config_shutdown();

		config_load("tests/ctrl_no.cfg");
		server = ctrl_server_create();
		client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT +
		    1);
		NTRY_BOOL(!ctrl_client_is_online(client));
		ctrl_client_free(&client);
		ctrl_server_free(&server);
		config_shutdown();
	}
}

NTEST(CustomPort)
{
	struct CtrlClient *client;
	struct CtrlServer *server;

	config_load("tests/ctrl_customport.cfg");
	server = ctrl_server_create();

	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);
	NTRY_BOOL(!ctrl_client_is_online(client));
	ctrl_client_free(&client);

	client = ctrl_client_create("127.0.0.1", 50000);
	NTRY_BOOL(ctrl_client_is_online(client));
	ctrl_client_free(&client);

	ctrl_server_free(&server);
	config_shutdown();
}

NTEST(ConfigDump)
{
	struct CtrlClient *client;
	struct CtrlServer *server;
	struct CtrlConfigList list;
	struct CtrlConfig *control_port;
	struct CtrlConfig *crate_;
	struct CtrlConfigScalar *scalar;
	struct CtrlConfig *barrier;
	struct CtrlConfig *v785;
	struct CtrlConfig *threshold;
	size_t i;
	int ret;

	/* TODO: Do we really need exactly this cfg? */
	config_load("tests/barriers.cfg");
	server = ctrl_server_create();
	client = ctrl_client_create("127.0.0.1", CTRL_DEFAULT_PORT + 1);
	TAILQ_INIT(&list);
	ret = ctrl_client_config_dump(client, &list);
	NTRY_BOOL(ret);

	control_port = TAILQ_FIRST(&list);
	NTRY_I(CONFIG_CONFIG, ==, control_port->type);
	NTRY_I(KW_CONTROL_PORT, ==, control_port->name);

	scalar = TAILQ_FIRST(&control_port->scalar_list);
	NTRY_I(CONFIG_SCALAR_INTEGER, ==, scalar->type);
	NTRY_I(23547, ==, scalar->value.i32.i);
	NTRY_I(CONFIG_UNIT_NONE->idx, ==, scalar->value.i32.unit);
	scalar = TAILQ_NEXT(scalar, next);
	NTRY_PTR(TAILQ_END(&control_port->scalar_list), ==, scalar);

	crate_ = TAILQ_NEXT(control_port, next);
	NTRY_I(CONFIG_BLOCK, ==, crate_->type);
	NTRY_I(KW_CRATE, ==, crate_->name);

	/* Check CRATE and its arguments. */
	scalar = TAILQ_FIRST(&crate_->scalar_list);
	NTRY_I(CONFIG_SCALAR_STRING, ==, scalar->type);
	NTRY_STR("TESTING", ==, scalar->value.str);
	scalar = TAILQ_NEXT(scalar, next);
	NTRY_PTR(TAILQ_END(&crate_->scalar_list), ==, scalar);

	/* Barrier 1/3. */
	barrier = TAILQ_FIRST(&crate_->child_list);
	NTRY_I(CONFIG_BLOCK, ==, barrier->type);
	NTRY_I(KW_BARRIER, ==, barrier->name);
	NTRY_PTR(TAILQ_END(&barrier->scalar_list), ==,
	    TAILQ_FIRST(&barrier->scalar_list));
	NTRY_PTR(TAILQ_END(&barrier->child_list), ==,
	    TAILQ_FIRST(&barrier->child_list));

	/* V785 1/2. */
	v785 = TAILQ_NEXT(barrier, next);
	NTRY_I(CONFIG_BLOCK, ==, v785->type);
	NTRY_I(KW_CAEN_V785, ==, v785->name);
	scalar = TAILQ_FIRST(&v785->scalar_list);
	NTRY_I(CONFIG_SCALAR_INTEGER, ==, scalar->type);
	NTRY_I(0x200000, ==, scalar->value.i32.i);
	NTRY_I(CONFIG_UNIT_NONE->idx, ==, scalar->value.i32.unit);
	scalar = TAILQ_NEXT(scalar, next);
	NTRY_PTR(TAILQ_END(&v785->value.block.arg_list), ==, scalar);

	/* Threshold array 1/2. */
	threshold = TAILQ_FIRST(&v785->child_list);
	NTRY_I(CONFIG_CONFIG, ==, threshold->type);
	NTRY_I(KW_THRESHOLD, ==, threshold->name);
	i = 0;
	TAILQ_FOREACH(scalar, &threshold->scalar_list, next) {
		NTRY_I(CONFIG_SCALAR_INTEGER, ==, scalar->type);
		NTRY_I(200, ==, scalar->value.i32.i);
		NTRY_I(CONFIG_UNIT_NONE->idx, ==, scalar->value.i32.unit);
		++i;
	}
	NTRY_I(32, ==, i);

	/* Barrier 2/3. */
	barrier = TAILQ_NEXT(v785, next);
	NTRY_I(CONFIG_BLOCK, ==, barrier->type);
	NTRY_I(KW_BARRIER, ==, barrier->name);
	NTRY_PTR(TAILQ_END(&barrier->scalar_list), ==,
	    TAILQ_FIRST(&barrier->scalar_list));
	NTRY_PTR(TAILQ_END(&barrier->child_list), ==,
	    TAILQ_FIRST(&barrier->child_list));

	/* V785 2/2. */
	v785 = TAILQ_NEXT(barrier, next);
	NTRY_I(CONFIG_BLOCK, ==, v785->type);
	NTRY_I(KW_CAEN_V785, ==, v785->name);
	scalar = TAILQ_FIRST(&v785->scalar_list);
	NTRY_I(CONFIG_SCALAR_INTEGER, ==, scalar->type);
	NTRY_I(0x210000, ==, scalar->value.i32.i);
	NTRY_I(CONFIG_UNIT_NONE->idx, ==, scalar->value.i32.unit);
	scalar = TAILQ_NEXT(scalar, next);
	NTRY_PTR(TAILQ_END(&v785->value.block.arg_list), ==, scalar);

	/* Threshold array 2/2. */
	threshold = TAILQ_FIRST(&v785->child_list);
	NTRY_I(CONFIG_CONFIG, ==, threshold->type);
	NTRY_I(KW_THRESHOLD, ==, threshold->name);
	i = 0;
	TAILQ_FOREACH(scalar, &threshold->scalar_list, next) {
		NTRY_I(CONFIG_SCALAR_INTEGER, ==, scalar->type);
		NTRY_I(200, ==, scalar->value.i32.i);
		NTRY_I(CONFIG_UNIT_NONE->idx, ==, scalar->value.i32.unit);
		++i;
	}
	NTRY_I(32, ==, i);

	/* Barrier 3/3. */
	barrier = TAILQ_NEXT(v785, next);
	NTRY_I(CONFIG_BLOCK, ==, barrier->type);
	NTRY_I(KW_BARRIER, ==, barrier->name);
	NTRY_PTR(TAILQ_END(&barrier->scalar_list), ==,
	    TAILQ_FIRST(&barrier->scalar_list));
	NTRY_PTR(TAILQ_END(&barrier->child_list), ==,
	    TAILQ_FIRST(&barrier->child_list));

	NTRY_PTR(TAILQ_END(crate_->child_list), ==,
	    TAILQ_NEXT(barrier, next));

	ctrl_config_dump_free(&list);
	ctrl_client_free(&client);
	ctrl_server_free(&server);
	config_shutdown();
}

NTEST_SUITE(Ctrl)
{
	NTEST_ADD(OnlineStatus);
	NTEST_ADD(EmptyCrate);
	NTEST_ADD(SimpleCrate);
	NTEST_ADD(UnknownCrate);
	NTEST_ADD(UnknownModule);
	NTEST_ADD(UnsupportedModule);
	NTEST_ADD(Confed);
	NTEST_ADD(CustomPort);
	NTEST_ADD(ConfigDump);
}
