/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2021, 2024
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
#include <nurdlib/base.h>

NTEST(MemoryCheck)
{
	struct EventBuffer eb;
	char buf100[102];
	char buf1[3];
	char *ptr;
	int i;

	eb.bytes = 1;
	eb.ptr = &buf1[1];
	ptr = eb.ptr;

	--ptr;
	NTRY_BOOL(!MEMORY_CHECK(eb, ptr));
	++ptr;
	NTRY_BOOL( MEMORY_CHECK(eb, ptr));
	++ptr;
	NTRY_BOOL(!MEMORY_CHECK(eb, ptr));

	eb.bytes = 100;
	eb.ptr = &buf100[1];
	ptr = eb.ptr;

	--ptr;
	NTRY_BOOL(!MEMORY_CHECK(eb, ptr));
	++ptr;
	for (i = 0; i < 100; ++i) {
		NTRY_BOOL( MEMORY_CHECK(eb, ptr));
		++ptr;
	}
	NTRY_BOOL(!MEMORY_CHECK(eb, ptr));
}

NTEST(EventBufferAdvance)
{
	struct EventBuffer eb;
	char buf[12];
	char *ptr;

	eb.bytes = 10;
	eb.ptr = &buf[1];

	ptr = &buf[0];
	NTRY_SIGNAL(EVENT_BUFFER_ADVANCE(eb, ptr));
	++ptr;
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (10, ==, eb.bytes);
	++ptr;
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (9, ==, eb.bytes);
	/* Don't allow going backwards! */
	--ptr;
	NTRY_SIGNAL(EVENT_BUFFER_ADVANCE(eb, ptr));
	NTRY_U     (9, ==, eb.bytes);
	++ptr;
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (9, ==, eb.bytes);

	ptr = &buf[1 + 8];
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (2, ==, eb.bytes);
	++ptr;
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (1, ==, eb.bytes);
	++ptr;
	EVENT_BUFFER_ADVANCE(eb, ptr);
	NTRY_U     (0, ==, eb.bytes);
	++ptr;
	NTRY_SIGNAL(EVENT_BUFFER_ADVANCE(eb, ptr));
	NTRY_U     (0, ==, eb.bytes);
}

NTEST(EventBufferInvariant)
{
	struct EventBuffer eb;
	struct EventBuffer eb_orig;

	eb.bytes = 10;
	eb.ptr = malloc(eb.bytes);
	COPY(eb_orig, eb);
	EVENT_BUFFER_INVARIANT(eb, eb_orig);

	++eb.bytes;
	NTRY_SIGNAL(EVENT_BUFFER_INVARIANT(eb, eb_orig));

	COPY(eb, eb_orig);
	--eb.bytes;
	NTRY_SIGNAL(EVENT_BUFFER_INVARIANT(eb, eb_orig));

	COPY(eb, eb_orig);
	eb.ptr = (uint8_t *)eb.ptr + 1;
	NTRY_SIGNAL(EVENT_BUFFER_INVARIANT(eb, eb_orig));

	COPY(eb, eb_orig);
	eb.ptr = (uint8_t *)eb.ptr - 1;
	NTRY_SIGNAL(EVENT_BUFFER_INVARIANT(eb, eb_orig));

	COPY(eb, eb_orig);
	EVENT_BUFFER_ADVANCE(eb, (uint8_t *)eb.ptr + 1);
	EVENT_BUFFER_INVARIANT(eb, eb_orig);

	free(eb_orig.ptr);
}

NTEST(AlignCheck)
{
	ALIGN_CHECK(0, 4);
	NTRY_SIGNAL(ALIGN_CHECK(1, 4));
	NTRY_SIGNAL(ALIGN_CHECK(2, 4));
	NTRY_SIGNAL(ALIGN_CHECK(3, 4));
	ALIGN_CHECK(4, 4);
}

NTEST_SUITE(Base)
{
	NTEST_ADD(MemoryCheck);
	NTEST_ADD(EventBufferAdvance);
	NTEST_ADD(EventBufferInvariant);
	NTEST_ADD(AlignCheck);
}
