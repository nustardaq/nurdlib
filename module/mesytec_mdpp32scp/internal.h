/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2024
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

#ifndef MODULE_MESYTEC_MDPP32SCP_INTERNAL_H
#define MODULE_MESYTEC_MDPP32SCP_INTERNAL_H

#include <module/mesytec_mdpp/internal.h>

#define MDPP32SCP_CH_N 32
#define MDPP32SCP_PR_N 8

enum ResolutionType {RT_ADC, RT_TDC};

struct MesytecMdpp32scpConfig {
	int	resolution[2];
	int	tf[MDPP32SCP_PR_N];
	int	gain[MDPP32SCP_PR_N];
	int	shaping[MDPP32SCP_PR_N];
	int	blr[MDPP32SCP_PR_N];
	int	rise_time[MDPP32SCP_PR_N];
	int32_t	pz[MDPP32SCP_CH_N];
	int     samples_pre[MDPP32SCP_PR_N];
	int     samples_tot[MDPP32SCP_PR_N];
};

struct MesytecMdpp32scpModule {
	struct	MesytecMdppModule mdpp;
	struct	MesytecMdpp32scpConfig config;
};

#endif
