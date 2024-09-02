/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2019, 2021, 2024
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

#include <nconf/util/pack.c>

#if NCONF_mPACK_bARPA_INET_H
#	include <arpa/inet.h>
#elif NCONF_mPACK_bNETINET_IN_H
#	include <sys/types.h>
#	include <netinet/in.h>
#elif NCONF_mPACK_bBSD_IN_H
#	include <bsd/in.h>
#endif
#if NCONFING_mPACK
#	define NCONF_TEST 0 == htonl(0)
#else

#include <util/pack.h>
#include <string.h>
#include <nurdlib/base.h>
#include <nurdlib/log.h>
#include <util/endian.h>
#include <util/md5sum.h>
#include <util/memcpy.h>

int
pack_is_empty(struct Packer const *a_packer)
{
	return a_packer->ofs >= a_packer->bytes;
}

void
pack_str(struct Packer *a_packer, char const *a_str)
{
	int len;

	len = strlen(a_str);
	if (a_packer->ofs + len >= a_packer->bytes) {
		log_die(LOGL, "Not enough space to pack string.");
	}
	memmove((uint8_t *)a_packer->data + a_packer->ofs, a_str, len + 1);
	a_packer->ofs += len + 1;
}

void
pack8(struct Packer *a_packer, uint8_t a_u8)
{
	if (a_packer->bytes < a_packer->ofs + 1) {
		log_die(LOGL, "Packing beyond limits, debug caller!");
	}
	*((uint8_t *)a_packer->data + a_packer->ofs) = a_u8;
	++a_packer->ofs;
}

void
pack16(struct Packer *a_packer, uint16_t a_u16)
{
	if (a_packer->bytes < a_packer->ofs + 2) {
		log_die(LOGL, "Packing beyond limits, debug caller!");
	}
	*(uint16_t *)((uint8_t *)a_packer->data + a_packer->ofs) =
	    htons(a_u16);
	a_packer->ofs += 2;
}

void
pack32(struct Packer *a_packer, uint32_t a_u32)
{
	if (a_packer->bytes < a_packer->ofs + 4) {
		log_die(LOGL, "Packing beyond limits, debug caller!");
	}
	*(uint32_t *)((uint8_t *)a_packer->data + a_packer->ofs) =
	    htonl(a_u32);
	a_packer->ofs += 4;
}

void
pack64(struct Packer *a_packer, uint64_t a_u64)
{
	if (a_packer->bytes < a_packer->ofs + 8) {
		log_die(LOGL, "Packing beyond limits, debug caller!");
	}

#if defined(NURDLIB_LITTLE_ENDIAN)
	*(uint64_t *)((uint8_t *)a_packer->data + a_packer->ofs) =
	    (uint64_t)htonl((uint32_t)(a_u64 & 0xffffffff)) << 32
	    | (uint64_t)htonl((uint32_t)(a_u64 >> 32));
#else
	*(uint64_t *)((uint8_t*)a_packer->data + a_packer->ofs) = a_u64;
#endif

	a_packer->ofs += 8;
}

void
packer_list_free(struct PackerList *a_list)
{
	while (!TAILQ_EMPTY(a_list)) {
		struct PackerNode *node;

		node = TAILQ_FIRST(a_list);
		TAILQ_REMOVE(a_list, node, next);
		FREE(node);
	}
}

struct Packer *
packer_list_get(struct PackerList *a_list, unsigned a_bit_num)
{
	struct PackerNode *node;
	struct Packer *packer;

	node = TAILQ_LAST(a_list, PackerList);
	if (TAILQ_END(a_list) != node &&
	    node->packer.bytes >= node->packer.ofs + a_bit_num / 8) {
		/* Use last packer if there's enough space. */
		return &node->packer;
	}
	/* No space, gotta make some. */
	CALLOC(node, 1);
	TAILQ_INSERT_TAIL(a_list, node, next);
	packer = &node->packer;
	PACKER_CREATE_STATIC(*packer, node->dgram.buf);
	pack32(packer, NURDLIB_MD5);
	/* Reserve space for num & id. */
	pack8(packer, 0);
	pack8(packer, 0);
	return packer;
}

char *
unpack_strdup(struct Packer *a_packer)
{
	char *str;
	uint8_t const *p;
	size_t i;

	p = (uint8_t const *)a_packer->data + a_packer->ofs;
	for (i = 0;; ++i) {
		if (a_packer->ofs + i >= a_packer->bytes) {
			return NULL;
		}
		if ('\0' == p[i]) {
			break;
		}
	}
	++i;
	str = malloc(i);
	memcpy_(str, p, i);
	a_packer->ofs += i;
	return str;
}

int
unpack8(struct Packer *a_packer, uint8_t *a_out)
{
	if (a_packer->bytes < a_packer->ofs + 1) {
		return 0;
	}
	*a_out = *((uint8_t *)a_packer->data + a_packer->ofs);
	++a_packer->ofs;
	return 1;
}

int
unpack16(struct Packer *a_packer, uint16_t *a_out)
{
	if (a_packer->bytes < a_packer->ofs + 2) {
		return 0;
	}
	*a_out = ntohs(*(uint16_t *)((uint8_t *)a_packer->data +
	    a_packer->ofs));
	a_packer->ofs += 2;
	return 1;
}

int
unpack32(struct Packer *a_packer, uint32_t *a_out)
{
	if (a_packer->bytes < a_packer->ofs + 4) {
		return 0;
	}
	*a_out = ntohl(*(uint32_t *)((uint8_t *)a_packer->data +
	    a_packer->ofs));
	a_packer->ofs += 4;
	return 1;
}

int
unpack64(struct Packer *a_packer, uint64_t *a_out)
{
	uint64_t v = 0;

	if (a_packer->bytes < a_packer->ofs + 8) {
		return 0;
	}

#if defined(NURDLIB_LITTLE_ENDIAN)
	v = *(uint64_t *)((uint8_t *)a_packer->data + a_packer->ofs);
	*a_out = (uint64_t)ntohl((uint32_t)(v & 0xffffffff)) << 32
	    | (uint64_t)ntohl((uint32_t)(v >> 32));
#else
	(void)v;
	*a_out = *(uint64_t *)((uint8_t *)a_packer->data + a_packer->ofs);
#endif
	a_packer->ofs += 8;
	return 1;
}

#endif
