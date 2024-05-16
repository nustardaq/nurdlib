/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2024
 * Håkan T Johansson
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

#ifndef NTEST_NTEST_H
#define NTEST_NTEST_H

#include <sys/wait.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NTEST_BLUE_ "\033[1;34m"
#define NTEST_GREEN_ "\033[1;32m"
#define NTEST_RED_ "\033[1;31m"
#define NTEST_RESET_ "\033[0m"

#define NTEST_SUITE_PROTO_(name) \
	void ntest_suite_##name##_(void)
#define NTEST_SUITE(name) \
	NTEST_SUITE_PROTO_(name); \
	void ntest_suite_##name##_(void)
#define NTEST(name) static void ntest_##name##_(void)
#define NTEST_ADD(name) do { \
		if (g_ntest_i_ == g_ntest_ctr_) { \
			unsigned fails_ = g_ntest_try_fail_; \
			printf("%s[%s:%d: %s]%s\n", g_ntest_blue_, __FILE__, \
			    __LINE__, #name, g_ntest_reset_); \
			ntest_##name##_(); \
			++g_ntest_i_; \
			++g_ntest_test_num_; \
			if (fails_ != g_ntest_try_fail_) { \
				++g_ntest_test_fail_; \
			} \
		} \
		g_ntest_ctr_ += 2; \
	} while (0)
extern char const *g_ntest_blue_;
extern char const *g_ntest_green_;
extern char const *g_ntest_red_;
extern char const *g_ntest_reset_;
extern int g_ntest_bail_;
extern unsigned g_ntest_i_;
extern unsigned g_ntest_ctr_;
extern unsigned g_ntest_test_num_;
extern unsigned g_ntest_test_fail_;

#define NTRY_ADD_ ++g_ntest_try_num_
#define NTRY_FAIL_HEADER_ do { \
		printf(" %s%s:%d: Failed: %s", g_ntest_red_, __FILE__, \
		    __LINE__, g_ntest_reset_); \
	} while (0)
#define NTRY_FAIL_FOOTER_ do { \
		printf("\n"); \
		++g_ntest_try_fail_; \
		if (g_ntest_bail_) { \
			printf("NTEST_BAIL set, I abort.\n"); \
			fflush(stdout); \
			fflush(stderr); \
			abort(); \
		} \
	} while (0)
extern unsigned g_ntest_try_num_;
extern unsigned g_ntest_try_fail_;

#define NTRY(Type, fmt, l, op, r) do { \
		Type const l_ = l; \
		Type const r_ = r; \
		NTRY_ADD_; \
		if (!(l_ op r_)) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=%" #fmt " " #op " '" #r "'=%" #fmt, \
			    l_, r_); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)
#define NTRY64(Type, fmt, l, op, r) do { \
		Type const l_ = l; \
		Type const r_ = r; \
		NTRY_ADD_; \
		if (!(l_ op r_)) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=0x%" #fmt ":%" #fmt " " #op " '" \
			    #r "'=0x%" #fmt ":%" #fmt, \
			    (uint32_t)(l_ >> 32), \
			    (uint32_t)(l_ & 0xffffffff), \
			    (uint32_t)(r_ >> 32), \
			    (uint32_t)(r_ & 0xffffffff)); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)

#define NTRY_BOOL(expr) do { \
		NTRY_ADD_; \
		if (!(expr)) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #expr "'"); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)
#define NTRY_C(l, op, r) NTRY(char, d, l, op, r)
#define NTRY_DBL(l, op, r) NTRY(double, g, l, op, r)
#define NTRY_I(l, op, r) NTRY(int, d, l, op, r)
#define NTRY_U(l, op, r) NTRY(unsigned, u, l, op, r)
#define NTRY_UC(l, op, r) NTRY(unsigned char, u, l, op, r)
#if UINT_MAX >= ULONG_MAX
#define NTRY_UL(a, op, b) NTRY(unsigned long, lx, a, op, b);
#else
#define NTRY_UL(a, op, b) NTRY64(unsigned long, x, a, op, b);
#endif
#define NTRY_ULL(a, op, b) NTRY64(uint64_t, x, a, op, b)
#define NTRY_PTR(l, op, r) NTRY(void const *, p, l, op, r)

#define NTRY_STR_PRE_(l, op, r) \
		char const *l_ = l; \
		char const *r_ = r; \
		NTRY_ADD_; \
		if (NULL == l_ && NULL == r_) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=NULL " #op " '" #r "'=NULL"); \
			NTRY_FAIL_FOOTER_; \
		} else if (NULL == l_) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=NULL " #op " '" #r "'=\"%s\"", r_); \
			NTRY_FAIL_FOOTER_; \
		} else if (NULL == r_) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=\"%s\" " #op " '" #r "'=NULL", l_); \
			NTRY_FAIL_FOOTER_; \
		}
#define NTRY_STRN(l, op, r, siz) do { \
		size_t const siz_ = siz; \
		NTRY_STR_PRE_(l, op, r) \
		else if (!(strncmp(l_, r_, siz_) op 0)) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=\"%s\" "#op" '" #r \
			    "'=\"%s\" n='" #siz "'=\"%u\"", l_, r_, \
			    (unsigned)siz_); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)
#define NTRY_STR(l, op, r) do { \
		NTRY_STR_PRE_(l, op, r) \
		else if (!(strcmp(l_, r_) op 0)) { \
			NTRY_FAIL_HEADER_; \
			printf("'" #l "'=\"%s\" "#op" '" #r \
			    "'=\"%s\"", l_, r_); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)

#define NTRY_SIGNAL(expr) do { \
		pid_t pid_; \
		int status_ = 0; \
		NTRY_ADD_; \
		fflush(stdout); \
		fflush(stderr); \
		pid_ = fork(); \
		if (-1 == pid_) { \
			perror("fork"); \
			exit(EXIT_FAILURE); \
		} else if (0 == pid_) { \
			expr; \
			exit(EXIT_SUCCESS); \
		} \
		waitpid(pid_, &status_, 0); \
		if (WIFEXITED(status_) && \
		    EXIT_SUCCESS == WEXITSTATUS(status_)) { \
			NTRY_FAIL_HEADER_; \
			printf("Missing signal."); \
			NTRY_FAIL_FOOTER_; \
		} \
	} while (0)

#define NTEST_GLOBAL_ \
	static int g_log_, g_stdout_, g_stderr_; \
	char const *g_ntest_blue_ = NTEST_BLUE_; \
	char const *g_ntest_green_ = NTEST_GREEN_; \
	char const *g_ntest_red_ = NTEST_RED_; \
	char const *g_ntest_reset_ = NTEST_RESET_; \
	int g_ntest_bail_; \
	unsigned g_ntest_i_; \
	unsigned g_ntest_ctr_; \
	unsigned g_ntest_test_num_; \
	unsigned g_ntest_test_fail_; \
	unsigned g_ntest_try_num_; \
	unsigned g_ntest_try_fail_
#define NTEST_HEADER_ do { \
		char const *e_ = getenv("NTEST_COLOR"); \
		if (e_ && 0 == strcmp(e_, "0")) { \
			g_ntest_blue_ = ""; \
			g_ntest_green_ = ""; \
			g_ntest_red_ = ""; \
			g_ntest_reset_ = ""; \
		} \
		e_ = getenv("NTEST_BAIL"); \
		if (e_ && 0 != strcmp(e_, "0")) { \
			g_ntest_bail_ = 1; \
		} \
	} while (0)
#define NTEST_FOOTER_ \
	if (0 == g_ntest_try_fail_) { \
		printf("%s", g_ntest_green_); \
	} \
	printf("Tests: %u/%u = %.2f%%\n", \
	    g_ntest_test_num_ - g_ntest_test_fail_, g_ntest_test_num_, \
	    100 - 100.f * g_ntest_test_fail_ / g_ntest_test_num_); \
	printf("Tries: %u/%u = %.2f%%%s\n", \
	    g_ntest_try_num_ - g_ntest_try_fail_, g_ntest_try_num_, \
	    100.f * (g_ntest_try_num_ - g_ntest_try_fail_) / \
	    g_ntest_try_num_, g_ntest_reset_)
#define NTEST_SUITE_CALL_(name) do { \
		unsigned old_; \
		for (g_ntest_i_ = 0;; ++g_ntest_i_) { \
			old_ = g_ntest_i_; \
			g_ntest_ctr_ = 0; \
			ntest_suite_##name##_(); \
			if (g_ntest_i_ == old_) { \
				break; \
			} \
		} \
	} while (0)

#endif
