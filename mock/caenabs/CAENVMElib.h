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

#ifndef MOCK_CAEN_CAENVMELIB_H
#define MOCK_CAEN_CAENVMELIB_H

typedef enum {
	cvV1718,
        cvV2718,
        cvA2818,
        cvA2719,
        cvA3818,

        cvUSB_A4818_V2718_LOCAL,
        cvUSB_A4818_V2718,
        cvUSB_A4818_LOCAL,
        cvUSB_A4818_V3718_LOCAL,
        cvUSB_A4818_V3718,
        cvUSB_A4818_V4718_LOCAL,
        cvUSB_A4818_V4718,
        cvUSB_A4818,
        cvUSB_A4818_A2719_LOCAL,

	cvUSB_V3718_LOCAL,
        cvPCI_A2818_V3718_LOCAL,
        cvPCIE_A3818_V3718_LOCAL,

        cvUSB_V3718,
        cvPCI_A2818_V3718,
        cvPCIE_A3818_V3718,

        cvUSB_V4718_LOCAL,
        cvPCI_A2818_V4718_LOCAL,
        cvPCIE_A3818_V4718_LOCAL,
        cvETH_V4718_LOCAL,

        cvUSB_V4718,
        cvPCI_A2818_V4718,
        cvPCIE_A3818_V4718,
        cvETH_V4718,

	cvInvalid
} CVBoardTypes;

typedef enum {
	cvSuccess,
	cvBusError
} CVErrorCodes;

typedef enum {
	cvA32_U_DATA,
	cvA32_U_BLT,
	cvA32_U_MBLT
} CVAddressModifier;

typedef enum {
	cvD16,
	cvD32,
	cvD64
} CVDataWidth;

char const *CAENVME_DecodeError(CVErrorCodes);

CVErrorCodes CAENVME_Init(CVBoardTypes, short, short, int32_t *);
CVErrorCodes CAENVME_Init2(CVBoardTypes, void const *, short, int32_t *);
CVErrorCodes CAENVME_End(int32_t);

CVErrorCodes CAENVME_ReadCycle(int32_t, uint32_t, void *, int, int);
CVErrorCodes CAENVME_WriteCycle(int32_t, uint32_t, void const *, int, int);

CVErrorCodes CAENVME_BLTReadCycle(int32_t, uint32_t, void *, int, int, int,
    int *);

#endif // C++ comment... Yuk.
