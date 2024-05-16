/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2019, 2022-2024
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

#ifndef MODULE_MESYTEC_VMMR8_INTERNAL_H
#define MODULE_MESYTEC_VMMR8_INTERNAL_H

#include <module/mesytec_mxdc32/internal.h> /*is it OK to use mxdc32 for vmmr8*/

#define VMMR8_N_BUSES 8
#define VMMR8_N_NIM 4
#define VMMR8_N_ECL 4


struct MesytecVmmr8Config {
  int active_buses[VMMR8_N_BUSES];
  int timing_resolution;
  uint16_t gate_offset;
  uint16_t gate_width;
  int use_ext_trg0;
  int use_ext_trg1;
  int use_int_trg;  /* any group of input channels*/
  int trigger_output_src;
  int trigger_output;
  int nim[VMMR8_N_NIM];
  int ecl[VMMR8_N_ECL];
  int mmr64_thrs0[VMMR8_N_BUSES];
  int mmr64_thrs1[VMMR8_N_BUSES];
  int data_thrs[VMMR8_N_BUSES];
  int pulser_enabled;
  int pulser_amplitude;
};

struct MesytecVmmr8Module {
  struct MesytecMxdc32Module mxdc32;
  struct MesytecVmmr8Config config;
};

#endif
