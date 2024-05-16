/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019-2021, 2023-2024
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
#include <nurdlib/log.h>
#include <tests/log_intercept.h>
#include <util/endian.h>
#include <util/stdint.h>
#include <util/string.h>

static void	push100(void);

void
push100(void)
{
	int i;

	for (i = 0; 100 > i; ++i) {
		log_level_push(g_log_level_info_);
	}
}

NTEST(GetLevel)
{
	NTRY_PTR(g_log_level_info_, ==, log_level_get_from_keyword(KW_INFO));
	NTRY_PTR(g_log_level_verbose_, ==,
	    log_level_get_from_keyword(KW_VERBOSE));
	NTRY_PTR(g_log_level_debug_, ==,
	    log_level_get_from_keyword(KW_DEBUG));
	NTRY_PTR(g_log_level_spam_, ==, log_level_get_from_keyword(KW_SPAM));
}

NTEST(StackPopEmptyShouldSignal)
{
	NTRY_SIGNAL(log_level_pop());
	log_level_push(g_log_level_info_);
	log_level_push(g_log_level_info_);
	log_level_pop();
	log_level_pop();
	NTRY_SIGNAL(log_level_pop());
}

NTEST(StackPushExcessiveShouldSignal)
{
	NTRY_SIGNAL(push100());
}

NTEST(LevelClear)
{
	log_level_push(g_log_level_info_);
	log_level_push(g_log_level_info_);
	log_level_clear();
	NTRY_SIGNAL(log_level_pop());
}

NTEST(DumpSmall)
{
	uint8_t const c_start[] = {
		0x12, 0x34, 0x56, 0x78,
		0xba, 0xff, 0xba, 0xff,
		0x01, 0x10, 0x01, 0x10,
		0x02, 0x20, 0x02, 0x20,
		0x03, 0x30, 0x03, 0x30,
		0x04, 0x40, 0x04, 0x40,
		0x05, 0x50, 0x05, 0x50,
		0x06, 0x60, 0x06, 0x60,
		0x07, 0x70, 0x07, 0x70
	};

	log_callback_set(log_intercept);
	log_level_push(g_log_level_verbose_);

	log_intercept_clear();
	log_dump(LOGL, c_start, 1);
	NTRY_I(4, ==, log_intercept_get_num());
	NTRY_I(log_intercept_get_level(0), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(0), ==, "---[ Dump begin ]---");
	/* TODO: Second line contains a pointer. */
	NTRY_I(log_intercept_get_level(2), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 12");
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(3), ==, "---[  Dump end  ]---");

	log_intercept_clear();
	log_dump(LOGL, c_start, 5);
	NTRY_I(log_intercept_get_level(2), ==, KW_INFO);
#if NURDLIB_BIG_ENDIAN
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 12345678 ba");
#elif NURDLIB_LITTLE_ENDIAN
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 78563412 ba");
#else
#	error "No endianness?"
#endif

	log_intercept_clear();
	log_dump(LOGL, c_start, 33);
	NTRY_I(log_intercept_get_level(0), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(0), ==, "---[ Dump begin ]---");
	NTRY_I(log_intercept_get_level(2), ==, KW_INFO);
#if NURDLIB_BIG_ENDIAN
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 12345678 baffbaff "
	    "01100110 02200220 03300330 04400440 05500550 06600660");
#elif NURDLIB_LITTLE_ENDIAN
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 78563412 ffbaffba "
	    "10011001 20022002 30033003 40044004 50055005 60066006");
#else
#	error "No endianness?"
#endif
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 07");
	NTRY_I(log_intercept_get_level(4), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(4), ==, "---[  Dump end  ]---");

	log_intercept_clear();
	log_dump(LOGL, c_start, 34);
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
#if NURDLIB_BIG_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 0770");
#elif NURDLIB_LITTLE_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 7007");
#else
#	error "No endianness?"
#endif

	log_intercept_clear();
	log_dump(LOGL, c_start, 35);
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
#if NURDLIB_BIG_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 0770 07");
#elif NURDLIB_LITTLE_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 7007 07");
#else
#	error "No endianness?"
#endif

	log_intercept_clear();
	log_dump(LOGL, c_start, 36);
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
#if NURDLIB_BIG_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 07700770");
#elif NURDLIB_LITTLE_ENDIAN
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 70077007");
#else
#	error "No endianness?"
#endif

	log_callback_set(NULL);
}

NTEST(DumpBig)
{
	uint32_t const c_start[] = {
		0x12345678,
		0x23456789,
		0x3456789a,
		0x456789ab,
		0x56789abc,
		0x6789abcd,
		0x789abcde,
		0x89abcdef,
		0x9abcdef0
	};

	log_intercept_clear();
	log_callback_set(log_intercept);
	log_level_push(g_log_level_verbose_);
	log_dump(LOGL, c_start, sizeof c_start);
	NTRY_I(5, ==, log_intercept_get_num());
	NTRY_I(log_intercept_get_level(0), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(0), ==, "---[ Dump begin ]---");
	/* TODO: Second line contains pointers. */
	NTRY_I(log_intercept_get_level(2), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(2), ==, "    0: 12345678 23456789 "
	    "3456789a 456789ab 56789abc 6789abcd 789abcde 89abcdef");
	NTRY_I(log_intercept_get_level(3), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(3), ==, "   20: 9abcdef0");
	NTRY_I(log_intercept_get_level(4), ==, KW_INFO);
	NTRY_STR(log_intercept_get_str(4), ==, "---[  Dump end  ]---");
	log_callback_set(NULL);
}

NTEST_SUITE(UtilLog)
{
	NTEST_ADD(GetLevel);
	NTEST_ADD(StackPopEmptyShouldSignal);
	NTEST_ADD(StackPushExcessiveShouldSignal);
	NTEST_ADD(LevelClear);
	NTEST_ADD(DumpSmall);
	NTEST_ADD(DumpBig);

	log_level_clear();
}
