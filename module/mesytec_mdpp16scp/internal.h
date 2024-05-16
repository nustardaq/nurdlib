/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2019, 2023-2024
 * Bastian Löher
 * Oliver Papst
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

#ifndef MODULE_MESYTEC_MDPP16SCP_INTERNAL_H
#define MODULE_MESYTEC_MDPP16SCP_INTERNAL_H

#include <module/mesytec_mdpp/internal.h>

#define MDPP16SCP_CH_N 16
#define MDPP16SCP_PR_N (MDPP16SCP_CH_N / 2)

enum ResolutionType {RT_ADC, RT_TDC};

struct MesytecMdpp16scpConfig {
	int	resolution[2];
	int	tf[MDPP16SCP_PR_N];
	int	gain[MDPP16SCP_PR_N];
	int	shaping[MDPP16SCP_PR_N];
	int	blr[MDPP16SCP_PR_N];
	int	rise_time[MDPP16SCP_PR_N];
	int32_t	pz[MDPP16SCP_CH_N];
};

struct MesytecMdpp16scpModule {
	struct	MesytecMdppModule mdpp;
	struct	MesytecMdpp16scpConfig config;
};

#endif
