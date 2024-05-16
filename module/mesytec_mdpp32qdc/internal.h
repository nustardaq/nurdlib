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

#ifndef MODULE_MESYTEC_MDPP32QDC_INTERNAL_H
#define MODULE_MESYTEC_MDPP32QDC_INTERNAL_H

#include <module/mesytec_mdpp/internal.h>

#define MDPP32QDC_CH_N 32
#define MDPP32QDC_QD_N 8

struct MesytecMdpp32qdcConfig {
	int	adc_resolution;
	int	sig_width[MDPP32QDC_QD_N];
	double	in_amp[MDPP32QDC_QD_N];
	double	jump_range[MDPP32QDC_QD_N];
	uint8_t	qdc_jump;
        double	int_long[MDPP32QDC_QD_N];
	double	int_short[MDPP32QDC_QD_N];
	double	long_gain_corr[MDPP32QDC_QD_N];
	double	short_gain_corr[MDPP32QDC_QD_N];
};

struct MesytecMdpp32qdcModule {
	struct	MesytecMdppModule mdpp;
	struct	MesytecMdpp32qdcConfig config;
};

#endif
