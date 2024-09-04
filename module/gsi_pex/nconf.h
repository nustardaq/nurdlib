/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021, 2023-2024
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

#ifndef MODULE_GSI_PEX_NCONF_H
#define MODULE_GSI_PEX_NCONF_H

#include <nconf/module/gsi_pex/nconf.h>

#if NCONF_mGSI_PEX_bYES
#	define _BSD_SOURCE 1
#	include <sys/mman.h>
#	include <errno.h>
#	include <fcntl.h>
#	include <stdlib.h>
#	include <string.h>
#	include <unistd.h>
#	include <nurdlib/base.h>
#	include <util/stdint.h>
#	ifndef PEX_DEV_PREFIX
#		define PEX_DEV_PREFIX
#	endif
#	define PEX_DEV STRINGIFY_VALUE(PEX_DEV_PREFIX) "/dev/pexor"
#	define PCI_BAR0_SIZE     0x00800000
#	if NCONFING_mGSI_PEX
#		define NCONF_TEST nconf_test_()
static int nconf_test_(void) {
	uint8_t *bar0;
	int fd;
	fd = open(PEX_DEV, O_RDWR);
	if (-1 == fd) {
		fprintf(stderr, "open(%s): %s.\n", PEX_DEV, strerror(errno));
		return 0;
	}
	bar0 = mmap(NULL, PCI_BAR0_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED |
	    MAP_LOCKED, fd, 0);
	if (MAP_FAILED == bar0) {
		fprintf(stderr, "mmap: %s.\n", strerror(errno));
		return 0;
	}
	munmap(NULL, PCI_BAR0_SIZE);
	close(fd);
	return 1;
}
#	endif
#elif NCONF_mGSI_PEX_bNO
/* NCONF_NOLINK */
#endif

#endif
