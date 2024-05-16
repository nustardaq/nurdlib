/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019, 2023-2024
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

#ifndef MODULE_MESYTEC_MDPP16QDC_INTERNAL_H
#define MODULE_MESYTEC_MDPP16QDC_INTERNAL_H

#include <module/mesytec_mdpp/internal.h>

#define MDPP16QDC_CH_N 16
#define MDPP16QDC_PR_N (MDPP16QDC_CH_N / 2)

struct MesytecMdpp16qdcConfig {
	int	signal_width[MDPP16QDC_PR_N];
	int	amplitude[MDPP16QDC_PR_N];
	double	jumper_range[MDPP16QDC_PR_N];
	int	qdc_jumper[MDPP16QDC_PR_N];
	int	integration_long[MDPP16QDC_PR_N];
	int	integration_short[MDPP16QDC_PR_N];
	double	correction_long_gain[MDPP16QDC_PR_N];
	double	correction_tf_gain[MDPP16QDC_PR_N];
	double	correction_short_gain[MDPP16QDC_PR_N];
};

struct MesytecMdpp16qdcModule {
	struct	MesytecMdppModule mdpp;
	struct	MesytecMdpp16qdcConfig config;
};

#endif
