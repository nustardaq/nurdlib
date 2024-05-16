/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2024
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
#include <module/module.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_tacquila/internal.h>
#include <module/pnpi_cros3/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/log.h>

NTEST(DefaultConfig)
{
	struct Crate *crate;
	struct CrateTag *tag;

	config_load("tests/crate_empty.cfg");
	crate = crate_create();
	NTRY_STR("AyeBeEmpty", ==, crate_get_name(crate));
	NTRY_BOOL(!crate_acvt_has(crate));
	NTRY_BOOL(!crate_get_do_shadow(crate));
	tag = crate_get_tag_by_name(crate, NULL);
	NTRY_PTR(NULL, !=, tag);
	NTRY_STR("Default", ==, crate_tag_get_name(tag));
	NTRY_I(0, ==, crate_module_get_num(crate));
	crate_free(&crate);
}

NTEST(BarriersAndTags)
{
	struct Crate *crate;
	struct CrateTag *t1, *t2;

	config_load("tests/crate_tags.cfg");
	crate = crate_create();
	NTRY_STR("BarriersAndTags", ==, crate_get_name(crate));
	NTRY_I(7, ==, crate_module_get_num(crate));
	t1 = crate_get_tag_by_name(crate, NULL);
	t2 = crate_get_tag_by_name(crate, "Default");
	NTRY_PTR(t1, ==, t2);
	t1 = crate_get_tag_by_name(crate, "Scalers1");
	NTRY_I(4, ==, crate_tag_get_module_num(t1));
	t1 = crate_get_tag_by_name(crate, "Scalers2");
	NTRY_I(3, ==, crate_tag_get_module_num(t1));
	crate_free(&crate);
}

NTEST(Scaler)
{
	struct Crate *crate;

	config_load("tests/crate_scaler.cfg");
	crate = crate_create();
	NTRY_STR("Scaler", ==, crate_get_name(crate));
	/* TODO: Test more? */
	crate_free(&crate);
}

NTEST(Tacquila)
{
	struct Crate *crate;

	config_load("tests/crate_tacquila.cfg");
	crate = crate_create();
	NTRY_BOOL(!TAILQ_EMPTY(&crate_get_tacquila_crate(crate)->list));
	NTRY_BOOL(TAILQ_EMPTY(&crate_get_cros3_crate(crate)->list));
	crate_free(&crate);
}

NTEST(Cros3)
{
	struct Crate *crate;

	config_load("tests/crate_cros3.cfg");
	crate = crate_create();
	NTRY_BOOL(TAILQ_EMPTY(&crate_get_tacquila_crate(crate)->list));
	NTRY_BOOL(!TAILQ_EMPTY(&crate_get_cros3_crate(crate)->list));
	crate_free(&crate);
}

#if NCONF_mGSI_PEX_bYES
NTEST(Pex)
{
	struct Crate *crate;
	struct CrateTag *t1, *t2;

	config_load("tests/crate_pex.cfg");
	crate = crate_create();
	NTRY_STR("Pex", ==, crate_get_name(crate));
	t1 = crate_get_tag_by_name(crate, "1");
	t2 = crate_get_tag_by_name(crate, "2");
	NTRY_BOOL(crate_tag_gsi_pex_is_needed(t1));
	NTRY_BOOL(crate_tag_gsi_pex_is_needed(t2));
	crate_free(&crate);
}
#endif

NTEST_SUITE(Crate)
{
	crate_setup();
	module_setup();

	NTEST_ADD(DefaultConfig);
	NTEST_ADD(BarriersAndTags);
	NTEST_ADD(Scaler);
	NTEST_ADD(Tacquila);
	NTEST_ADD(Cros3);
#if NCONF_mGSI_PEX_bYES
	NTEST_ADD(Pex);
#endif

	config_shutdown();
}
