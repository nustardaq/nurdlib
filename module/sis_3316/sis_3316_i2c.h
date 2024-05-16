/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2017-2018, 2024
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

#ifndef MODULE_SIS_3316_SIS_3316_I2C_H
#define MODULE_SIS_3316_SIS_3316_I2C_H

#include <module/sis_3316/internal.h>

/* SPI interface to si5325 clock multiplier */
struct Sis3316ExternalClockMultiplierConfig
{
	uint32_t n1_output_divider_clock_1;
	uint32_t n1_output_divider_clock_2;
	uint32_t n2_feedback_divider;
	uint32_t n3_input_divider;
	uint8_t bandwidth_select;
	uint8_t n1_high_speed_divider;
};

enum Sis3316ExternalClockInputFrequency
{
	IN_10_MHZ = 10,
	IN_12_MHZ = 12,
	IN_20_MHZ = 20,
	IN_50_MHZ= 50
};

enum Sis3316ExternalClockOutputFrequency
{
	OUT_125_MHZ = 125,
	OUT_250_MHZ = 250
};

void sis_3316_set_frequency(struct Sis3316Module* m,
    int osc, unsigned char preset[6]);
void sis_3316_change_frequency(struct Sis3316Module* m, int osc,
    unsigned int a_hs, unsigned int a_n1);
struct Sis3316ExternalClockMultiplierConfig sis_3316_make_ext_clk_mul_config(
    enum Sis3316ExternalClockInputFrequency,
    enum Sis3316ExternalClockOutputFrequency);
void sis_3316_si5325_clk_multiplier_set(struct Sis3316Module *, struct
    Sis3316ExternalClockMultiplierConfig);
void sis_3316_si5325_clk_multiplier_bypass(struct Sis3316Module *);

#endif
