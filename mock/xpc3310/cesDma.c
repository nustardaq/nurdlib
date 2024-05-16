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

#include <stdint.h>
#include <stdlib.h>
#include <ces/cesBus.h>
#include <ces/cesDma.h>

struct CesDmaChain *cesDmaNewChain(void) { return NULL; }
void cesDmaChainDelete(struct CesDmaChain *a_a) {}
struct CesDmaChainEl *cesDmaChainAdd64(struct CesDmaChain *a_a, CesUint64 a_b,
    struct CesBus *a_c, uintptr_t a_d, struct CesBus *a_e, size_t a_f, int
    a_g) { return NULL; }
CesError cesDmaChainElErr(struct CesDmaChainEl *a_a) { return 0; }
void cesDmaChainStart(struct CesDmaChain *a_a) {}
CesError cesDmaChainWait(struct CesDmaChain *a_a) { return 0; }
void cesDmaChainReset(struct CesDmaChain *a_a) {}
