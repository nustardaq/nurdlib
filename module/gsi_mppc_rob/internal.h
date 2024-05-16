/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2022, 2024
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

#ifndef MODULE_GSI_MPPC_ROB_INTERNAL_H
#define MODULE_GSI_MPPC_ROB_INTERNAL_H

#include <module/gsi_ctdc_proto/internal.h>

struct GsiMppcRobModule {
	struct	GsiCTDCProtoModule ctdcp;
};
struct GsiMppcRobCrate {
	struct	GsiMppcRobModule *sfp[4];
};

void			gsi_mppc_rob_crate_add(struct GsiMppcRobCrate *,
    struct Module *);
void			gsi_mppc_rob_crate_create(struct GsiMppcRobCrate *);
int			gsi_mppc_rob_crate_init_slow(struct GsiMppcRobCrate *)
	FUNC_RETURNS;

#endif
