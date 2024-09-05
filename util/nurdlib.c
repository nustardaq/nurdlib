/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
 * Bastian Löher
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

#include <nurdlib.h>
#include <string.h>
#include <ctrl/ctrl.h>
#include <module/map/map.h>
#include <module/module.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <util/err.h>
#include <util/sigbus.h>

static struct CtrlServer *g_server = NULL;
static int g_is_setup;

struct Crate *
nurdlib_setup(LogCallback a_log_callback, char const *a_config_path)
{
	struct Crate *crate;

	if (g_is_setup) {
		log_die(LOGL, "nurdlib_setup called twice in a row!");
	}
	log_callback_set(a_log_callback);
	LOGF(info)(LOGL, "Using config '%s'.", a_config_path);
	err_set_printer(log_printerv);
	sigbus_setup();
	crate_setup();
	module_setup();
	map_setup();
	config_load(a_config_path);
	crate = crate_create();
	crate_init(crate);
	g_server = ctrl_server_create();
	g_is_setup = 1;
	return crate;
}

void
nurdlib_shutdown(struct Crate **a_crate)
{
	if (!g_is_setup) {
		log_die(LOGL, "nurdlib_shutdown called twice in a row!");
	}
	crate_free(a_crate);
	ctrl_server_free(&g_server);
	map_shutdown();
	config_shutdown();
	log_level_clear();
	g_is_setup = 0;
}
