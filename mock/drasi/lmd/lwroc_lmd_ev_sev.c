/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024
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

#include <stdint.h>
#include <stdlib.h>
#include <lwroc_lmd_ev_sev.h>

lmd_stream_handle *lwroc_get_lmd_stream(const char *a_a) { return NULL; }

void lwroc_reserve_event_buffer(lmd_stream_handle *a_a, uint32_t a_b, size_t
    a_c, size_t a_d, size_t a_e) {}

void lwroc_new_event(lmd_stream_handle *a_a, lmd_event_10_1_host **a_b, int
    a_c) {}

char *lwroc_new_subevent(lmd_stream_handle *a_a, int a_b,
    lmd_subevent_10_1_host **a_c, struct lwroc_lmd_subevent_info *a_d)
{ return NULL;}

void lwroc_finalise_subevent(lmd_stream_handle *a_a, int a_b, void *a_c) {}

void lwroc_finalise_event_buffer(lmd_stream_handle *a_a) {}
