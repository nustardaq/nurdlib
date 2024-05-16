/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2016-2021, 2024
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

#ifndef NURDLIB_BASE_H
#define NURDLIB_BASE_H

#include <string.h>
#include <nurdlib/log.h>
#include <util/fmtmod.h>
#include <util/stdint.h>

/* Common silly macros. */
#define CALLOC(ptr, num) ptr = calloc(num, sizeof *ptr)
#define FREE(ptr) do {\
		free(ptr);\
		(ptr) = NULL;\
	} while (0)
#define LENGTH(x) (sizeof x / sizeof *x)
#define MALLOC(ptr, num) ptr = malloc(num * sizeof *ptr)
#ifndef MAX
#	define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#	define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#define CLAMP(x, a, b) ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define IS_POW2(x) (0 == (((x) - 1) & (x)))
#define OFFSETOF(s, m) ((uintptr_t)&(s).m - (uintptr_t)&(s))
#define COPY(dst, src) do {\
		int copy_assert_size_[sizeof dst == sizeof src];\
		(void)copy_assert_size_;\
		memmove(&dst, &src, sizeof dst);\
	} while (0)
#define ZERO(x) memset(&x, 0, sizeof x)
#define ALIGN_CHECK(num, size) do { \
	size_t num_ = (size_t)(num); \
	size_t size_ = (size_t)(size); \
	if (0 != num_ % size_) { \
		log_die(LOGL, \
		    "'"#num"'=%"PRIz" not aligned to '"#size"'=%"PRIz".", \
		    num_, size_); \
	} \
} while (0)

/* Advance the data pointer and reduce the # of bytes. */
#define EVENT_BUFFER_ADVANCE(a_eb, a_ptr) do {\
		void const *ptr_ = (void const *)(a_ptr);\
		size_t diff_;\
		if ((uintptr_t)ptr_ < (uintptr_t)(a_eb).ptr) {\
			log_die(LOGL, "Invalid pointer to advance event "\
			    "buffer.");\
		}\
		diff_ = (uintptr_t)ptr_ - (uintptr_t)(a_eb).ptr;\
		if (diff_ > (a_eb).bytes) {\
			log_die(LOGL, "Tried to advance outside event "\
			    "buffer.");\
		}\
		(a_eb).ptr = a_ptr;\
		(a_eb).bytes -= diff_;\
	} while (0)

#define EVENT_BUFFER_INVARIANT(a_eb1, a_eb2) do {\
		uintptr_t end1_ = (uintptr_t)(a_eb1).ptr + (a_eb1).bytes;\
		uintptr_t end2_ = (uintptr_t)(a_eb2).ptr + (a_eb2).bytes;\
		if (end1_ != end2_) {\
			log_die(LOGL, "Event-buffer inconsistent "\
			    "(%p:%"PRIz" != %p:%"PRIz").",\
			    (a_eb1).ptr, (a_eb1).bytes,\
			    (a_eb2).ptr, (a_eb2).bytes);\
		}\
	} while (0)

/*
 * An offset helps with hardware counters/scalers that should not be reset
 * willy-nilly because they don't have to be, e.g.:
 *  master = master_counter
 *  slave = slave_counter
 *  add = slave - master
 *  ... let time ravage the heedless ...
 *  COUNTER_DIFF(master, slave, add) == 0 -> a-ok!
 */
#define COUNTER_DIFF(c_l, c_r, add)\
    (((c_l).value - (c_r).value + add) & (c_l).mask & (c_r).mask)
/*
 * Difference between counter with mask and a raw counter.
 */
#define COUNTER_DIFF_RAW(c, raw)\
    (((c).value - (raw)) & (c).mask)

/*
 * I = inclusive, e = exclusive.
 * XY: X = opening, Y = closing.
 */
#define IN_RANGE_II(x, min, max) ((min) <= (x) && (x) <= (max))
#define IN_RANGE_IE(x, min, max) ((min) <= (x) && (x) <  (max))
#define IN_RANGE_EI(x, min, max) ((min) <  (x) && (x) <= (max))
#define IN_RANGE_EE(x, min, max) ((min) <  (x) && (x) <  (max))

/*
 * Note on pointing in nurdlib:
 * According to the C standard, it is undefined behaviour to even form a
 * pointer past allocated memory. Any function in nurdlib which takes a
 * pointer uses a size_t for the number of available bytes until the end of
 * the valid memory area, rather than an end pointer.
 */

/* Checks that ptr_ is inside event-buffer eb_. */
#define MEMORY_CHECK(eb_, ptr_) \
    (((uintptr_t)(ptr_) >= (uintptr_t)(eb_).ptr) &&\
     ((uintptr_t)(ptr_) + sizeof *(ptr_) <= (uintptr_t)(eb_).ptr +\
      (eb_).bytes))

struct EventConstBuffer {
	/* # bytes still available. */
	size_t	bytes;
	/* Pointer to next empty entry. */
	void	const *ptr;
};
struct EventBuffer {
	size_t	bytes;
	void	*ptr;
};
struct Counter {
	uint32_t	value;
	uint32_t	mask;
};

#endif
