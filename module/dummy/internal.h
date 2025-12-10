/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2017, 2019, 2023-2024
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

#ifndef MODULE_DUMMY_INTERNAL_H
#define MODULE_DUMMY_INTERNAL_H

#include <module/module.h>

#define N_CHANNELS 32

/* Module structure. */
struct DummyModule {
	/* Parent Module object. */
	struct	Module module;
	/*
	 * Mapped "hardware" for single-cycle accesses, we will actually be
	 * talking to some computer memory.
	 */
	struct	Map *sicy_map;
	/* Virtual memory for module registers. */
	void	*memory;
	/* Event counter diff. */
	uint32_t	event_diff;
	/* Callback in 'init' for testing. */
	void	(*init_callback)(void);

	/* Configuration. */
	uint32_t	address;
	uint32_t	thresholds[N_CHANNELS];
	uint32_t	channel_enable;
	uint32_t	offset;
	float	range;
	uint32_t	gate_delay;
	uint32_t	gate_width;
	enum	Keyword mode;
};

void	dummy_init_set_callback(struct Module *, void (*)(void));
/* This is a software trigger that preferably happens in hardware! */
void	dummy_counter_increase(struct Module *, unsigned);

#endif
