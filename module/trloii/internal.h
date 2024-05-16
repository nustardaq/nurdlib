/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Michael Munch
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

#ifndef MODULE_TRLOII_INTERNAL_H
#define MODULE_TRLOII_INTERNAL_H

#include <module/module.h>
#include <util/funcattr.h>

enum TRLOIIModuleType {
	TRLOII_TRIDI,
	TRLOII_VULOM,
	TRLOII_RFX1
};

struct TRLOIIModule {
	struct	Module module;
	uint32_t	address;
	int	has_timestamp;
	char	const *multi_event_tag_name;
	int	acvt_has;
	/*
	 * The TRLO II library checks some hw things internally with its own
	 * methods, so we don't use nurdlib mapping.
	 */
	void	volatile *hwmap;
	void	*unmapinfo;
	void	*ctrl;
	uint32_t	ts_status;
	uint32_t	data_counter;
	uint32_t	u32_low;
	size_t	scaler_n;
	uint8_t	scaler[64];
};

void		trloii_create(struct Crate *, struct TRLOIIModule *, struct
    ConfigBlock *);
void		trloii_scaler_parse(struct Crate *, struct ConfigBlock *, char
    const *, struct TRLOIIModule *);

#endif
