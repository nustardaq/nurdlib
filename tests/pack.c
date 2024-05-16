/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2018, 2021, 2024
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

#include <ntest/ntest.h>
#include <assert.h>
#include <nurdlib/base.h>
#include <util/pack.h>
#include <util/endian.h>
#include <util/string.h>

NTEST(PackUnpack8)
{
	struct Packer packer;
	char buf[1];
	uint8_t u8;

	memset(buf, 0xff, sizeof buf);
	PACKER_CREATE_STATIC(packer, buf);
	pack8(&packer, 0x12);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 1);
	NTRY_I(packer.ofs, ==, 1);
	NTRY_I(0x12, ==, buf[0]);

	PACKER_CREATE_STATIC(packer, buf);
	NTRY_I(0, !=, unpack8(&packer, &u8));
	NTRY_I(0x12, ==, u8);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 1);
	NTRY_I(packer.ofs, ==, 1);
}

NTEST(PackUnpack16)
{
	struct Packer packer;
	char buf[2];
	uint16_t u16;

	memset(buf, 0xff, sizeof buf);
	PACKER_CREATE_STATIC(packer, buf);
	pack16(&packer, 0x1234);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 2);
	NTRY_I(packer.ofs, ==, 2);
	NTRY_I(0x12, ==, buf[0]);
	NTRY_I(0x34, ==, buf[1]);

	PACKER_CREATE_STATIC(packer, buf);
	NTRY_I(0, !=, unpack16(&packer, &u16));
	NTRY_I(0x1234, ==, u16);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 2);
	NTRY_I(packer.ofs, ==, 2);
}

NTEST(PackUnpack32)
{
	struct Packer packer;
	char buf[4];
	uint32_t u32;

	memset(buf, 0xff, sizeof buf);
	PACKER_CREATE_STATIC(packer, buf);
	pack32(&packer, 0x12345678);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 4);
	NTRY_I(packer.ofs, ==, 4);
	NTRY_I(0x12, ==, buf[0]);
	NTRY_I(0x34, ==, buf[1]);
	NTRY_I(0x56, ==, buf[2]);
	NTRY_I(0x78, ==, buf[3]);

	PACKER_CREATE_STATIC(packer, buf);
	NTRY_I(0, !=, unpack32(&packer, &u32));
	NTRY_I(0x12345678, ==, u32);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 4);
	NTRY_I(packer.ofs, ==, 4);
}

NTEST(PackUnpack64)
{
	struct Packer packer;
	char buf[8];
	uint64_t u64;

	memset(buf, 0xff, sizeof buf);
	PACKER_CREATE_STATIC(packer, buf);
	pack64(&packer, 1);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 8);
	NTRY_I(packer.ofs, ==, 8);
	NTRY_C(0x00, ==, buf[0]);
	NTRY_C(0x01, ==, buf[7]);

	PACKER_CREATE_STATIC(packer, buf);
	pack64(&packer, ((uint64_t)0x12345678 << 32) | (uint64_t)(0x9abcdef0));
	NTRY_C(0x12, ==, buf[0]);
	NTRY_C(0x34, ==, buf[1]);
	NTRY_C(0x56, ==, buf[2]);
	NTRY_C(0x78, ==, buf[3]);
	NTRY_UC(0x9a, ==, buf[4]);
	NTRY_UC(0xbc, ==, buf[5]);
	NTRY_UC(0xde, ==, buf[6]);
	NTRY_UC(0xf0, ==, buf[7]);

	PACKER_CREATE_STATIC(packer, buf);
	NTRY_I(0, !=, unpack64(&packer, &u64));
	NTRY_ULL(((uint64_t)0x12345678 << 32) | (uint64_t)(0x9abcdef0), ==,
	    u64);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 8);
	NTRY_I(packer.ofs, ==, 8);
}

NTEST(PackUnpackString)
{
	struct Packer packer;
	char buf[4];
	char *p;

	memset(buf, 0xff, sizeof buf);
	PACKER_CREATE_STATIC(packer, buf);
	pack_str(&packer, "123");
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 4);
	NTRY_I(packer.ofs, ==, 4);
	NTRY_I('1', ==, buf[0]);
	NTRY_I('2', ==, buf[1]);
	NTRY_I('3', ==, buf[2]);
	NTRY_I('\0', ==, buf[3]);

	PACKER_CREATE_STATIC(packer, buf);
	p = unpack_strdup(&packer);
	NTRY_STR("123", ==, p);
	FREE(p);
	NTRY_PTR(packer.data, ==, buf);
	NTRY_I(packer.bytes, ==, 4);
	NTRY_I(packer.ofs, ==, 4);
}

NTEST(PackUnpackTuttiFrutti)
{
#define N 10000
	struct {
		uint32_t	value;
		char	bit_num;
	} cmd_array[N];
	struct Packer packer;
	char *buf;
	unsigned i;

	CALLOC(buf, sizeof(uint32_t) * N);
	PACKER_CREATE(packer, buf, sizeof(uint32_t) * N);

	for (i = 0; i < N; ++i) {
		uint32_t u32;
		float f;

		u32 = rand() + (rand() << 16);
		f = (float)((double)rand() / RAND_MAX);
		if (f < 0.25) {
			u32 &= 0xff;
			cmd_array[i].bit_num = 8;
			pack8(&packer, u32);
		} else if (f < 0.5) {
			u32 &= 0xffff;
			cmd_array[i].bit_num = 16;
			pack16(&packer, u32);
		} else if (f < 0.75) {
			cmd_array[i].bit_num = 32;
			pack32(&packer, u32);
		} else {
			char str[16];

			cmd_array[i].bit_num = 0;
			u32 %= 1000;
			snprintf(str, sizeof str, "%d", u32);
			pack_str(&packer, str);
		}
		cmd_array[i].value = u32;
	}

	/* Check that we can recover all entries. */
	PACKER_CREATE(packer, buf, sizeof(uint32_t) * N);
	for (i = 0; i < N; ++i) {
		switch (cmd_array[i].bit_num) {
		case 0:
			{
				char str[16];
				char *p;

				snprintf(str, sizeof str, "%d",
				    cmd_array[i].value);
				p = unpack_strdup(&packer);
				NTRY_STR(str, ==, p);
				FREE(p);
			}
			break;
		case 8:
			{
				uint8_t u8;

				NTRY_I(0, !=, unpack8(&packer, &u8));
				NTRY_I(cmd_array[i].value, ==, u8);
			}
			break;
		case 16:
			{
				uint16_t u16;

				NTRY_I(0, !=, unpack16(&packer, &u16));
				NTRY_I(cmd_array[i].value, ==, u16);
			}
			break;
		case 32:
			{
				uint32_t u32;

				NTRY_I(0, !=, unpack32(&packer, &u32));
				NTRY_I(cmd_array[i].value, ==, u32);
			}
			break;
		default:
			assert(0 && "This should not happen.");
		}
	}
	FREE(buf);
}

NTEST(PackLimit)
{
	struct Packer packer;
	char buf[10];

	PACKER_CREATE(packer, buf, 0); NTRY_SIGNAL(pack8(&packer, 0));
	PACKER_CREATE(packer, buf, 0); NTRY_SIGNAL(pack16(&packer, 0));
	PACKER_CREATE(packer, buf, 0); NTRY_SIGNAL(pack32(&packer, 0));
	PACKER_CREATE(packer, buf, 0); NTRY_SIGNAL(pack_str(&packer, "a"));

	PACKER_CREATE(packer, buf, 1); pack8(&packer, 0);
	PACKER_CREATE(packer, buf, 1); NTRY_SIGNAL(pack16(&packer, 0));
	PACKER_CREATE(packer, buf, 1); NTRY_SIGNAL(pack32(&packer, 0));
	PACKER_CREATE(packer, buf, 1); NTRY_SIGNAL(pack_str(&packer, "a"));

	PACKER_CREATE(packer, buf, 2); pack8(&packer, 0);
	PACKER_CREATE(packer, buf, 2); pack16(&packer, 0);
	PACKER_CREATE(packer, buf, 2); NTRY_SIGNAL(pack32(&packer, 0));
	PACKER_CREATE(packer, buf, 2); pack_str(&packer, "a");
	PACKER_CREATE(packer, buf, 2); NTRY_SIGNAL(pack_str(&packer, "ab"));

	PACKER_CREATE(packer, buf, 3); pack8(&packer, 0);
	PACKER_CREATE(packer, buf, 3); pack16(&packer, 0);
	PACKER_CREATE(packer, buf, 3); NTRY_SIGNAL(pack32(&packer, 0));
	PACKER_CREATE(packer, buf, 3); pack_str(&packer, "ab");
	PACKER_CREATE(packer, buf, 3); NTRY_SIGNAL(pack_str(&packer, "abc"));

	PACKER_CREATE(packer, buf, 4); pack8(&packer, 0);
	PACKER_CREATE(packer, buf, 4); pack16(&packer, 0);
	PACKER_CREATE(packer, buf, 4); pack32(&packer, 0);
	PACKER_CREATE(packer, buf, 4); pack_str(&packer, "abc");
	PACKER_CREATE(packer, buf, 4); NTRY_SIGNAL(pack_str(&packer, "abcd"));
}

NTEST(UnpackLimit)
{
	struct Packer packer;
	char buf[10];
	char *s;
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;

	strlcpy(buf, "abc", sizeof buf);

	PACKER_CREATE(packer, buf, 0); NTRY_BOOL(!unpack8(&packer, &u8));
	PACKER_CREATE(packer, buf, 0); NTRY_BOOL(!unpack16(&packer, &u16));
	PACKER_CREATE(packer, buf, 0); NTRY_BOOL(!unpack32(&packer, &u32));
	PACKER_CREATE(packer, buf, 0);
	NTRY_PTR(NULL, ==, unpack_strdup(&packer));

	PACKER_CREATE(packer, buf, 1); NTRY_BOOL(unpack8(&packer, &u8));
	PACKER_CREATE(packer, buf, 1); NTRY_BOOL(!unpack16(&packer, &u16));
	PACKER_CREATE(packer, buf, 1); NTRY_BOOL(!unpack32(&packer, &u32));
	PACKER_CREATE(packer, buf, 1);
	NTRY_PTR(NULL, ==, unpack_strdup(&packer));

	PACKER_CREATE(packer, buf, 2); NTRY_BOOL(unpack8(&packer, &u8));
	PACKER_CREATE(packer, buf, 2); NTRY_BOOL(unpack16(&packer, &u16));
	PACKER_CREATE(packer, buf, 2); NTRY_BOOL(!unpack32(&packer, &u32));
	PACKER_CREATE(packer, buf, 2);
	NTRY_PTR(NULL, ==, unpack_strdup(&packer));

	PACKER_CREATE(packer, buf, 3); NTRY_BOOL(unpack8(&packer, &u8));
	PACKER_CREATE(packer, buf, 3); NTRY_BOOL(unpack16(&packer, &u16));
	PACKER_CREATE(packer, buf, 3); NTRY_BOOL(!unpack32(&packer, &u32));
	PACKER_CREATE(packer, buf, 3);
	NTRY_PTR(NULL, ==, unpack_strdup(&packer));

	PACKER_CREATE(packer, buf, 4); NTRY_BOOL(unpack8(&packer, &u8));
	PACKER_CREATE(packer, buf, 4); NTRY_BOOL(unpack16(&packer, &u16));
	PACKER_CREATE(packer, buf, 4); NTRY_BOOL(unpack32(&packer, &u32));
	PACKER_CREATE(packer, buf, 4);
	NTRY_PTR(NULL, !=, s = unpack_strdup(&packer));
	FREE(s);
}

NTEST(UnpackNontermedString)
{
	struct Packer packer;
	char buf[2];
	char *p;

	PACKER_CREATE_STATIC(packer, buf);
	packer.bytes = 0;
	p = unpack_strdup(&packer);
	NTRY_PTR(NULL, ==, p);

	PACKER_CREATE_STATIC(packer, buf);
	p = packer.data;
	p[0] = 1;
	p[1] = 1;
	p = unpack_strdup(&packer);
	NTRY_PTR(NULL, ==, p);
}

NTEST_SUITE(Pack)
{
	NTEST_ADD(PackUnpack8);
	NTEST_ADD(PackUnpack16);
	NTEST_ADD(PackUnpack32);
	NTEST_ADD(PackUnpack64);
	NTEST_ADD(PackUnpackString);
	NTEST_ADD(PackUnpackTuttiFrutti);
	NTEST_ADD(PackLimit);
	NTEST_ADD(UnpackLimit);
	NTEST_ADD(UnpackNontermedString);
}
