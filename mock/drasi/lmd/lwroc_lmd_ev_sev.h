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

#ifndef LWROC_LMD_EV_SEV_H
#define LWROC_LMD_EV_SEV_H

enum {
	LWROC_LMD_SEV_NORMAL
};

typedef void *lmd_stream_handle;

struct lwroc_lmd_subevent_info {
	int	type;
	int	subtype;
	int	procid;
	int	control;
	int	subcrate;
};
typedef struct {
	int	dummy;
} lmd_event_10_1_host;
typedef struct {
	int	dummy;
} lmd_subevent_10_1_host;

lmd_stream_handle *lwroc_get_lmd_stream(const char *);
void lwroc_reserve_event_buffer(lmd_stream_handle *, uint32_t, size_t, size_t,
    size_t);
void lwroc_new_event(lmd_stream_handle *, lmd_event_10_1_host **, int);
char *lwroc_new_subevent(lmd_stream_handle *, int, lmd_subevent_10_1_host **,
    struct lwroc_lmd_subevent_info *);
void lwroc_finalise_subevent(lmd_stream_handle *, int, void *);
void lwroc_finalise_event_buffer(lmd_stream_handle *);

#endif
