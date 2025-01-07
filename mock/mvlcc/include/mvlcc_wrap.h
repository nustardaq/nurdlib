/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2024-2025
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

#ifndef MVLCC_WRAP_H
#define MVLCC_WRAP_H

struct mvlcc_s {
	int	dummy;
};
struct MvlccBlockReadParams {
	uint8_t	amod;
	int	fifo;
	int	swap;
};
typedef struct mvlcc_s *mvlcc_t;

int mvlcc_connect(mvlcc_t);
void mvlcc_disconnect(mvlcc_t);
char const *mvlcc_strerror(int);
void mvlcc_free_mvlc(mvlcc_t);
int mvlcc_is_mvlc_valid(mvlcc_t);
mvlcc_t mvlcc_make_mvlc(void const *);
void mvlcc_set_global_log_level(const char *);
int mvlcc_single_vme_read(mvlcc_t, unsigned, uint32_t *, unsigned, unsigned);
int mvlcc_single_vme_write(mvlcc_t, unsigned, uint32_t, unsigned, unsigned);
int mvlcc_vme_block_read(mvlcc_t, uint32_t, uint32_t *, size_t, size_t *,
    struct MvlccBlockReadParams);

#endif
