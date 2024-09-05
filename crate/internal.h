/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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

#ifndef CRATE_INTERNAL_H
#define CRATE_INTERNAL_H

#include <util/funcattr.h>

struct ConfigBlock;
struct CrateTag;
struct Packer;
struct PackerList;

int	crate_config_write(int, int, int, struct Packer *) FUNC_RETURNS;
void	crate_gsi_pex_goc_read(uint8_t, uint8_t, uint16_t, uint32_t, uint16_t,
    uint32_t *);
void	crate_gsi_pex_goc_write(uint8_t, uint8_t, uint16_t, uint32_t,
    uint16_t, uint32_t);
void	crate_info_pack(struct Packer *, int);
void	crate_pack(struct PackerList *);
void	crate_pack_free(struct PackerList *);
void	crate_register_array_pack(struct PackerList *, int, int, int);
int	crate_tag_gsi_pex_is_needed(struct CrateTag const *) FUNC_RETURNS;

#endif
