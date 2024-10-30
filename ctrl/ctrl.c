/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
 * Michael Munch
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

#include <ctrl/ctrl.h>
#include <crate/internal.h>
#include <module/module.h>
#include <module/reggen/registerlist.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/md5sum.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/memcpy.h>
#include <util/pack.h>
#include <util/queue.h>
#include <util/string.h>
#include <util/thread.h>
#include <util/time.h>
#include <util/udp.h>

/*
 * TODO: More client side error messages.
 */

struct CtrlClient {
	struct	UDPClient *client;
};
struct CtrlServer {
	struct	UDPServer *server;
	struct	Thread thread;
};

static int	client_recv(struct CtrlClient const *, struct UDPDatagram *,
    struct Packer *) FUNC_RETURNS;
static int	client_send_recv(struct CtrlClient const *, struct UDPDatagram
    *, struct Packer *) FUNC_RETURNS;
static int	client_send_recv_seq(struct CtrlClient const *, struct
    UDPDatagram *, struct Packer *, struct DatagramArray *) FUNC_RETURNS;
static void	config_indent(unsigned);
static void	config_scalar_list_print(struct CtrlConfigScalarList const *,
    int);
static int	packer_lookup(struct DatagramArray *, size_t *, struct Packer
    *) FUNC_RETURNS;
static void	scalar_list_free(struct CtrlConfigScalarList *);
static void	send_config_dump(struct UDPServer *, struct UDPAddress const
    *);
static void	send_crate_array(struct UDPServer *, struct UDPAddress const
    *);
static void	send_crate_info(struct UDPServer *, struct UDPAddress const *,
    int);
static void	send_goc_read(struct UDPServer *, struct UDPAddress const *,
    uint8_t, uint8_t, uint16_t, uint32_t, uint16_t, uint32_t);
static void	send_module_access(struct UDPServer *, struct UDPAddress const
    *, uint8_t, uint8_t, int, struct Packer *);
static void	send_online(struct UDPServer *, struct UDPAddress const *);
static void	send_packer(struct UDPServer *, struct UDPAddress const *,
    struct UDPDatagram *, struct Packer const *);
static void	send_packer_list(struct UDPServer *, struct UDPAddress const
    *, struct PackerList const *);
static void	send_register_array(struct UDPServer *, struct UDPAddress
    const *, int, int, int);
static void	server_run(void *);
static int	unpack_config_list(struct DatagramArray *, size_t *, struct
    Packer *, struct CtrlConfigList *) FUNC_RETURNS;
static void	unpack_loc(struct Packer *);
static int	unpack_module_access(struct CtrlModuleAccess *, size_t, struct
    DatagramArray *, size_t *) FUNC_RETURNS;
static int	unpack_scalar(struct DatagramArray *, size_t *, struct Packer
    *, struct CtrlConfigScalar *) FUNC_RETURNS;
static int	unpack_scalar_list(struct DatagramArray *, size_t *, struct
    Packer *, struct CtrlConfigScalarList *) FUNC_RETURNS;

/* Looks at MD5 sum. */
int
client_recv(struct CtrlClient const *a_client, struct UDPDatagram *a_dgram,
    struct Packer *a_packer)
{
	uint32_t md5;
	unsigned trial;

	for (trial = 0; 3 > trial; ++trial) {
		a_dgram->bytes = 0;
		if (!udp_client_receive(a_client->client, a_dgram, 1.0)) {
			break;
		}
		if (0 != a_dgram->bytes) {
			break;
		}
	}
	if (3 == trial) {
		log_error(LOGL, "No server reply, check host and MD5 sum.");
		PACKER_CREATE(*a_packer, NULL, 0);
		return 0;
	}
	PACKER_CREATE(*a_packer, a_dgram->buf, a_dgram->bytes);
	md5 = 0;
	if (!unpack32(a_packer, &md5) || NURDLIB_MD5 != md5) {
		if (0 == md5) {
			log_error(LOGL, "Empty server reply.");
		} else {
			log_error(LOGL, "MD5 mismatch between server (0x%08x)"
			    " and client (0x%08x)!", md5, NURDLIB_MD5);
			log_error(LOGL, "The DAQ and the client must be built"
			    " with the same nurdlib version!");
		}
		return 0;
	}
	return 1;
}

/* Sends, receives, and checks one-datagram reply. */
int
client_send_recv(struct CtrlClient const *a_client, struct UDPDatagram
    *a_dgram, struct Packer *a_packer)
{
	ASSERT(void *, "p", a_dgram->buf, ==, a_packer->data);
	a_dgram->bytes = a_packer->ofs;
	if (!udp_client_send(a_client->client, a_dgram)) {
		PACKER_CREATE(*a_packer, NULL, 0);
		return 0;
	}
	a_dgram->bytes = 0;
	return client_recv(a_client, a_dgram, a_packer);
}

/* Same as above but for sequential datagrams. */
int
client_send_recv_seq(struct CtrlClient const *a_client, struct UDPDatagram
    *a_dgram, struct Packer *a_packer, struct DatagramArray *a_array)
{
	uint32_t node_mask, node_full_mask;

	a_array->num = 0;
	a_array->array = NULL;
	a_dgram->bytes = a_packer->ofs;
	if (!udp_client_send(a_client->client, a_dgram)) {
		PACKER_CREATE(*a_packer, NULL, 0);
		return 0;
	}
	node_mask = 0;
	node_full_mask = 0;
	do {
		uint32_t bit;
		uint8_t cur_dgram_num;
		uint8_t cur_dgram_id;

		a_dgram->bytes = 0;
		if (!client_recv(a_client, a_dgram, a_packer)) {
			goto client_send_recv_seq_error;
		}
		if (!unpack8(a_packer, &cur_dgram_num) ||
		    !unpack8(a_packer, &cur_dgram_id)) {
			log_error(LOGL, "Server reply missing seq node "
			    "info.");
			goto client_send_recv_seq_error;
		}
		if (1 > cur_dgram_num ||
		    32 < cur_dgram_num ||
		    cur_dgram_num <= cur_dgram_id) {
			log_error(LOGL, "Server reply seq node info invalid,"
			    " num=%d id=%d.", cur_dgram_num, cur_dgram_id);
			goto client_send_recv_seq_error;
		}
		if (a_array->num != cur_dgram_num) {
			if (0 != a_array->num) {
				log_error(LOGL, "Server replies have "
				    "different node num, congestion?");
				goto client_send_recv_seq_error;
			}
			a_array->num = cur_dgram_num;
			CALLOC(a_array->array, a_array->num);
			node_full_mask = BITS_MASK_TOP(a_array->num - 1);
		}
		bit = 1 << cur_dgram_id;
		if (0 != (bit & node_mask)) {
			log_error(LOGL, "Server reply duplicated node ID.");
			goto client_send_recv_seq_error;
		}
		node_mask |= bit;
		COPY(a_array->array[cur_dgram_id], *a_dgram);
	} while (node_full_mask != node_mask);
	return 1;
client_send_recv_seq_error:
	a_array->num = 0;
	FREE(a_array->array);
	return 0;
}

void
config_indent(unsigned a_level)
{
	while (0 < a_level--) {
		fputc('\t', stdout);
	}
}

void
config_scalar_list_print(struct CtrlConfigScalarList const *a_list, int
    a_do_parens)
{
	struct CtrlConfigScalar *scalar;
	int is_first, num;

	num = 0;
	TAILQ_FOREACH(scalar, a_list, next) {
		++num;
	}
	if (1 < num || a_do_parens) {
		fputc('(', stdout);
	}
	is_first = 1;
	TAILQ_FOREACH(scalar, a_list, next) {
		if (!is_first) {
			printf(", ");
		}
		is_first = 0;
		switch (scalar->type) {
		case CONFIG_SCALAR_EMPTY:
			printf("()");
			break;
		case CONFIG_SCALAR_DOUBLE:
			printf("%f", scalar->value.d.d);
			break;
		case CONFIG_SCALAR_INTEGER:
			/* Try to be clever about printing dec/hex. */
			{
				uint32_t u32;

				u32 = scalar->value.i32.i;
				if (0x100 > u32) {
					printf("0x%02x", u32);
				} else if (0x10000 > u32) {
					printf("0x%04x", u32);
				} else {
					printf("0x%08x", u32);
				}
			}
			break;
		case CONFIG_SCALAR_KEYWORD:
			printf("%s",
			    keyword_get_string(scalar->value.keyword));
			break;
		case CONFIG_SCALAR_RANGE:
			printf("%d..%d", scalar->value.range.first,
			    scalar->value.range.last);
			break;
		case CONFIG_SCALAR_STRING:
			printf("\"%s\"", scalar->value.str);
			break;
		}
	}
	if (1 < num || a_do_parens) {
		fputc(')', stdout);
	}
}

void
ctrl_client_config(struct CtrlClient *a_client, int a_crate_i, int a_module_j,
    int a_submodule_k, struct ConfigBlock *a_block)
{
	struct PackerList packer_list;
	struct PackerNode *first;
	struct Packer *packer;

	TAILQ_INIT(&packer_list);
	packer = packer_list_get(&packer_list, 32);
	/* We don't want to make a full sequence here, remove seq bytes. */
	packer->ofs -= 2;
	PACK(*packer, 8, VL_CTRL_CONFIG, fail);
	PACK(*packer, 8, a_crate_i, fail);
	PACK(*packer, 8, a_module_j, fail);
	if (-1 == a_submodule_k) {
		PACK(*packer, 8, 0, fail);
	} else {
		PACK(*packer, 8, 1, fail);
		PACKER_LIST_PACK(packer_list, 8, a_submodule_k);
	}
	config_pack_children(&packer_list, a_block);
	if (TAILQ_EMPTY(&packer_list)) {
		return;
	}
	first = TAILQ_FIRST(&packer_list);
	if (first != TAILQ_LAST(&packer_list, PackerList)) {
		log_error(LOGL, "Online config way too big!");
	} else {
		first->dgram.bytes = first->packer.ofs;
		udp_client_send(a_client->client, &first->dgram);
	}
fail:
	log_error(LOGL, "Packing failed.");
	packer_list_free(&packer_list);
}

int
ctrl_client_config_dump(struct CtrlClient *a_client, struct CtrlConfigList
    *a_list)
{
	struct DatagramArray dgram_array;
	struct UDPDatagram dgram;
	struct Packer packer;
	size_t dgram_array_i;

	assert(TAILQ_EMPTY(a_list));
	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, fail);
	PACK(packer,  8, VL_CTRL_CONFIG_DUMP, fail);
	if (!client_send_recv_seq(a_client, &dgram, &packer, &dgram_array)) {
fail:
		log_error(LOGL, "Could not fetch config dump.");
		return 0;
	}
	dgram_array_i = -1;
	if (!unpack_config_list(&dgram_array, &dgram_array_i, &packer,
	    a_list)) {
		log_error(LOGL, "Failed to unpack config dump.");
		ctrl_config_dump_free(a_list);
		FREE(dgram_array.array);
		return 0;
	}
	FREE(dgram_array.array);
	return 1;
}

void
ctrl_client_crate_array_free(struct CtrlCrateArray *a_crate_array)
{
	size_t i;

	if (NULL == a_crate_array->array) {
		return;
	}
	for (i = 0; i < a_crate_array->num; ++i) {
		struct CtrlCrate *crate;
		size_t j;

		crate = &a_crate_array->array[i];
		FREE(crate->name);
		for (j = 0; j < crate->module_num; ++j) {
			struct CtrlModule *module;

			module = &crate->module_array[j];
			FREE(module->submodule_array);
		}
		FREE(crate->module_array);
	}
	a_crate_array->num = 0;
	FREE(a_crate_array->array);
}

int
ctrl_client_crate_array_get(struct CtrlClient *a_client, struct CtrlCrateArray
    *a_crate_array)
{
	struct DatagramArray dgram_array;
	struct UDPDatagram dgram;
	struct Packer packer;
	size_t dgram_array_i;
	uint8_t i, num;

	a_crate_array->num = -1;
	a_crate_array->array = NULL;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_CRATE_ARRAY, pack_fail);
	if (!client_send_recv_seq(a_client, &dgram, &packer, &dgram_array)) {
pack_fail:
		log_error(LOGL, "Could not fetch crate array.");
		return 0;
	}
	dgram_array_i = -1;
	if (!packer_lookup(&dgram_array, &dgram_array_i, &packer) ||
	    !unpack8(&packer, &num)) {
		goto unpack_fail;
	}
	a_crate_array->num = num;
	CALLOC(a_crate_array->array, num);
	for (i = 0; num > i; ++i) {
		struct CtrlCrate *crate;
		uint8_t j, module_num;

		crate = &a_crate_array->array[i];
		if (!packer_lookup(&dgram_array, &dgram_array_i, &packer)) {
			goto unpack_fail;
		}
		crate->name = unpack_strdup(&packer);
		if (NULL == crate->name) {
			goto unpack_fail;
		}
		if (!packer_lookup(&dgram_array, &dgram_array_i, &packer) ||
		    !unpack8(&packer, &module_num)) {
			goto unpack_fail;
		}
		crate->module_num = module_num;
		CALLOC(crate->module_array, module_num);
		for (j = 0; module_num > j; ++j) {
			struct CtrlModule *module;
			uint16_t module_type;
			uint8_t k, submodule_num;

			module = &crate->module_array[j];
			if (!packer_lookup(&dgram_array, &dgram_array_i,
			    &packer) ||
			    !unpack16(&packer, &module_type)) {
				goto unpack_fail;
			}
			module->type = module_type;
			if (!packer_lookup(&dgram_array, &dgram_array_i,
			    &packer) ||
			    !unpack8(&packer, &submodule_num)) {
				goto unpack_fail;
			}
			module->submodule_num = submodule_num;
			CALLOC(module->submodule_array, submodule_num);
			for (k = 0; submodule_num > k; ++k) {
				struct CtrlSubModule *submodule;
				uint16_t submodule_type;

				submodule = &module->submodule_array[k];
				if (!packer_lookup(&dgram_array,
				    &dgram_array_i, &packer) ||
				    !unpack16(&packer, &submodule_type)) {
					goto unpack_fail;
				}
				submodule->type = submodule_type;
			}
		}
	}
	FREE(dgram_array.array);
	return 1;
unpack_fail:
	FREE(dgram_array.array);
	log_error(LOGL, "Crate array data corrupt.");
	return 0;
}

int
ctrl_client_crate_info_get(struct CtrlClient *a_client, struct CtrlCrateInfo
    *a_crate_info, int a_crate_i)
{
	struct UDPDatagram dgram;
	struct Packer packer;
	uint16_t u16;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, fail);
	PACK(packer,  8, VL_CTRL_CRATE_INFO, fail);
	PACK(packer,  8, a_crate_i, fail);
	if (!client_send_recv(a_client, &dgram, &packer)) {
fail:
		log_error(LOGL, "Could not fetch crate info.");
		return 0;
	}
	if (!unpack16(&packer, &u16)) {
		log_error(LOGL, "Crate info corrupt.");
		return 0;
	}
	if (0xffff == u16) {
		unpack_loc(&packer);
		return 0;
	}
	if (!unpack8(&packer, &a_crate_info->dt_release) ||
	    !unpack16(&packer, &a_crate_info->acvt) ||
	    !unpack32(&packer, &a_crate_info->shadow.buf_bytes) ||
	    !unpack32(&packer, &a_crate_info->shadow.max_bytes)) {
		log_error(LOGL, "Crate info corrupt.");
		return 0;
	}
	return 1;
}

int
ctrl_client_goc_read(struct CtrlClient *a_client, uint8_t a_crate_i, uint8_t
    a_sfp, uint16_t a_card, uint32_t a_offset, uint16_t a_num, uint32_t
    *a_dst)
{
	struct DatagramArray dgram_array;
	struct UDPDatagram dgram;
	struct Packer packer;
	size_t dgram_array_i;
	uint32_t id;
	uint32_t u32;
	unsigned i;
	int ret;

	dgram_array.array = NULL;
	ret = 0;
	id = (uint32_t)(time_getd() * 1e6);

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_GOC_READ, pack_fail);
	PACK(packer,  8, a_crate_i, pack_fail);
	PACK(packer,  8, a_sfp, pack_fail);
	PACK(packer, 16, a_card, pack_fail);
	PACK(packer, 32, a_offset, pack_fail);
	PACK(packer, 16, a_num, pack_fail);
	PACK(packer, 32, id, pack_fail);
	if (!client_send_recv_seq(a_client, &dgram, &packer, &dgram_array)) {
pack_fail:
		log_error(LOGL, "Could not goc read.");
		goto fail;
	}
	dgram_array_i = -1;
	if (!packer_lookup(&dgram_array, &dgram_array_i, &packer)) {
		goto fail;
	}
	if (!unpack32(&packer, &u32)) {
		log_error(LOGL, "Goc read reply corrupt.");
		goto fail;
	}
	if (u32 != id) {
		log_error(LOGL, "Goc read ID mismatch.");
		goto fail;
	}
	for (i = 0; i < a_num; ++i) {
		if (!packer_lookup(&dgram_array, &dgram_array_i, &packer)) {
			goto fail;
		}
		if (!unpack32(&packer, &u32)) {
			log_error(LOGL, "Goc read value %u/%u missing.",
			    i, a_num);
			goto fail;
		}
		a_dst[i] = u32;
	}
	ret = 1;
fail:
	FREE(dgram_array.array);
	return ret;
}

void
ctrl_client_goc_write(struct CtrlClient *a_client, uint8_t a_crate_i, uint8_t
    a_sfp, uint16_t a_card, uint32_t a_offset, uint16_t a_num, uint32_t
    a_value)
{
	struct UDPDatagram dgram;
	struct Packer packer;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_GOC_WRITE, pack_fail);
	PACK(packer,  8, a_crate_i, pack_fail);
	PACK(packer,  8, a_sfp, pack_fail);
	PACK(packer, 16, a_card, pack_fail);
	PACK(packer, 32, a_offset, pack_fail);
	PACK(packer, 16, a_num, pack_fail);
	PACK(packer, 32, a_value, pack_fail);
	udp_client_send(a_client->client, &dgram);
	return;
pack_fail:
	log_error(LOGL, "Could not goc write.");
}

struct CtrlClient *
ctrl_client_create(char const *a_address, uint16_t a_port)
{
	struct UDPClient *udp_client;
	struct CtrlClient *client;

	udp_client = udp_client_create(UDP_IPV4, a_address, a_port);
	if (NULL == udp_client) {
		log_error(LOGL, "Could not start UDP client.");
		return NULL;
	}
	CALLOC(client, 1);
	client->client = udp_client;
	return client;
}

void
ctrl_client_free(struct CtrlClient **a_client)
{
	struct CtrlClient *client;

	client = *a_client;
	if (NULL == client) {
		return;
	}
	udp_client_free(&client->client);
	FREE(*a_client);
}

int
ctrl_client_is_online(struct CtrlClient *a_client)
{
	struct UDPDatagram dgram;
	struct Packer packer;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_ONLINE, pack_fail);
	return client_send_recv(a_client, &dgram, &packer);
pack_fail:
	return 0;
}

int
ctrl_client_module_access_get(struct CtrlClient *a_client, int a_crate_i, int
    a_module_j, int a_submodule_k, struct CtrlModuleAccess *a_arr, size_t
    a_arrn)
{
	struct UDPDatagram dgram;
	struct Packer packer;
	struct DatagramArray dgram_array;
	size_t dgram_array_i, i;
	uint32_t ofs_guess;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_MODULE_ACCESS, pack_fail);
	PACK(packer,  8, a_crate_i, pack_fail);
	PACK(packer,  8, a_module_j, pack_fail);
	if (-1 == a_submodule_k) {
		PACK(packer, 8, 0, pack_fail);
	} else {
		PACK(packer, 8, 1, pack_fail);
		PACK(packer, 8, a_submodule_k, pack_fail);
	}

	ofs_guess = 0;
	for (i = 0; i < a_arrn; ++i) {
		struct CtrlModuleAccess const *a;
		uint8_t *pmask;
		uint8_t mask;

		a = &a_arr[i];

		pmask = PACKER_GET_PTR(packer);
		PACK(packer, 8, 0, pack_fail);

		mask = 0;
		switch (a->bits) {
		case 16: mask = 0 << 0; break;
		case 32: mask = 1 << 0; break;
		default:
			log_error(LOGL, "Can only access 16 or 32 bits, "
			    "asked for %u.", a->bits);
			return 0;
		}

		if (a->ofs != ofs_guess) {
			int is_long;

			is_long = 1;
			if (a->ofs > ofs_guess) {
				uint32_t d;

				d = a->ofs - ofs_guess;
				if (d < 256) {
					mask |= 1 << 1;
					PACK(packer, 8, d, pack_fail);
					is_long = 0;
				} else if (d < 65536) {
					mask |= 2 << 1;
					PACK(packer, 16, d, pack_fail);
					is_long = 0;
				}
			}
			if (is_long) {
				mask |= 3 << 1;
				PACK(packer, 32, a->ofs, pack_fail);
			}
		}
		ofs_guess = a->ofs + a->bits / 8;

		if (a->do_read) {
			mask |= 1 << 3;
		}

		*pmask = mask;

		if (!a->do_read) {
			switch (a->bits) {
			case 16:
				PACK(packer, 16, a->value, pack_fail);
				break;
			case 32:
				PACK(packer, 32, a->value, pack_fail);
				break;
			}
		}
	}
	if (!client_send_recv_seq(a_client, &dgram, &packer, &dgram_array)) {
		log_error(LOGL, "Could not fetch module access.");
		return 0;
	}
	dgram_array_i = -1;
	if (!unpack_module_access(a_arr, a_arrn, &dgram_array,
	    &dgram_array_i)) {
		log_error(LOGL, "Failed to unpack module access.");
		return 0;
	}
	return 1;
pack_fail:
	log_error(LOGL, "Module access too large.");
	return 0;
}

void
ctrl_client_register_array_free(struct CtrlRegisterArray *a_register_array)
{
	a_register_array->num = 0;
	FREE(a_register_array->reg_array);
}

int
ctrl_client_register_array_get(struct CtrlClient *a_client, struct
    CtrlRegisterArray *a_register_array, int a_crate_i, int a_module_j, int
    a_submodule_k)
{
	struct DatagramArray dgram_array;
	struct UDPDatagram dgram;
	struct Packer packer;
	size_t dgram_array_i;

	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	PACK(packer,  8, VL_CTRL_REGISTER_LIST, pack_fail);
	PACK(packer,  8, a_crate_i, pack_fail);
	PACK(packer,  8, a_module_j, pack_fail);
	if (-1 == a_submodule_k) {
		PACK(packer, 8, 0, pack_fail);
	} else {
		PACK(packer, 8, 1, pack_fail);
		PACK(packer, 8, a_submodule_k, pack_fail);
	}
	if (!client_send_recv_seq(a_client, &dgram, &packer, &dgram_array)) {
pack_fail:
		log_error(LOGL, "Could not fetch register array.");
		return 0;
	}
	dgram_array_i = -1;
	if (!ctrl_client_register_array_unpack(a_register_array, &dgram_array,
	    &dgram_array_i, &packer)) {
		log_error(LOGL, "Failed to unpack register array.");
		ctrl_client_register_array_free(a_register_array);
		FREE(dgram_array.array);
		return 0;
	}
	FREE(dgram_array.array);
	return 1;
}

void
ctrl_client_register_array_print(struct CtrlRegisterArray *a_register_array)
{
	struct RegisterEntryClient const *reg_list;
	unsigned i;

	reg_list = module_register_list_client_get(
	    a_register_array->module_type);
	for (i = 0; a_register_array->num > i; ++i) {
		struct CtrlRegister const *reg;
		unsigned j, num;

		reg = &a_register_array->reg_array[i];
		printf(" %s (0x%04x) = ", reg_list[i].name,
		    reg_list[i].address);
		num = MAX(1, reg_list[i].array_length);
		if (1 < num) {
			printf("\n  ");
		}
		for (j = 0; num > j; ++j) {
			if (16 >= reg_list[i].bits) {
				printf("0x%04x", reg->value[j]);
			} else {
				printf("0x%08x", reg->value[j]);
			}
			if (7 > (7 & j)) {
				printf(" ");
			} else if (num - 1 > j) {
				printf("\n  ");
			}
		}
		printf("\n");
	}
	/* Remaining data is custom-packed. */
}

int
ctrl_client_register_array_unpack(struct CtrlRegisterArray *a_register_array,
    struct DatagramArray *a_dgram_array, size_t *a_dgram_array_i, struct
    Packer *a_packer)
{
	struct RegisterEntryClient const *reg_entry_list;
	unsigned i, num;
	uint16_t module_type16;

	a_register_array->num = -1;
	a_register_array->reg_array = NULL;
	if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
		return 0;
	}
	if (!unpack16(a_packer, &module_type16)) {
		log_error(LOGL, "Could not get module type, server reply "
		    "corrupt.");
		return 0;
	}
	if (65535 == module_type16) {
		log_error(LOGL, "Packer could not dump requested module, -1 "
		    "returned.");
		unpack_loc(a_packer);
		return 0;
	}
	a_register_array->module_type = module_type16;
	reg_entry_list = module_register_list_client_get(module_type16);
	for (num = 0; NULL != reg_entry_list[num].name; ++num)
		;
	a_register_array->num = num;
	CALLOC(a_register_array->reg_array, num);
	for (i = 0; num > i; ++i) {
		struct RegisterEntryClient const *reg_entry;
		struct CtrlRegister *reg;
		uint16_t u16;
		unsigned j, value_num;

		reg_entry = &reg_entry_list[i];
		reg = &a_register_array->reg_array[i];
		reg->entry = reg_entry;
		value_num = MAX(1, reg_entry->array_length);
		CALLOC(reg->value, value_num);
		for (j = 0; value_num > j; ++j) {
			if (16 >= reg_entry->bits) {
				if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
					return 0;
				}
				if (!unpack16(a_packer, &u16)) {
					log_error(LOGL, "Could not read "
					    "register value, packed data "
					    "corrupt.");
					return 0;
				}
				reg->value[j] = u16;
			} else if (32 >= reg_entry->bits) {
				if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
					return 0;
				}
				if (!unpack32(a_packer, &reg->value[j])) {
					log_error(LOGL, "Could not read "
					    "register value, packed data "
					    "corrupt.");
					return 0;
				}
			} else {
				log_die(LOGL, "Module with register bit "
				    "num(%d) > 32 encountered, fix the lib.",
				    reg_entry->bits);
			}
		}
	}
	/* Parse custom tail. */
if (0) {
	/* TODO: This is module-specific... */
	for (;;) {
		uint16_t name, value;

		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			break;
		}
		if (!unpack16(a_packer, &name)) {
			break;
		}
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			break;
		}
		if (!unpack16(a_packer, &value)) {
			break;
		}
	}
}
	return 1;
}

void
ctrl_config_dump_free(struct CtrlConfigList *a_list)
{
	while (!TAILQ_EMPTY(a_list)) {
		struct CtrlConfig *c;

		c = TAILQ_FIRST(a_list);
		TAILQ_REMOVE(a_list, c, next);
		scalar_list_free(&c->scalar_list);
		ctrl_config_dump_free(&c->child_list);
		FREE(c);
	}
}

void
ctrl_config_dump_print(struct CtrlConfigList const *a_list, unsigned a_level)
{
	struct CtrlConfig *config;

	TAILQ_FOREACH(config, a_list, next) {
		config_indent(a_level);
		printf("%s", keyword_get_string(config->name));
		if (!config->is_touched) {
			printf("*");
		}
		if (KW_BARRIER == config->name ||
		    KW_TAGS == config->name) {
			puts("");
		} else if (CONFIG_BLOCK == config->type) {
			config_scalar_list_print(&config->scalar_list, 1);
			puts(" {");
			ctrl_config_dump_print(&config->child_list, a_level +
			    1);
			config_indent(a_level);
			puts("}");
		} else {
			printf(" = ");
			config_scalar_list_print(&config->scalar_list, 0);
			puts("");
		}
	}
}

struct CtrlServer *
ctrl_server_create(void)
{
	struct CtrlServer *server;
	int port;

	port = config_get_int32(NULL, KW_CONTROL_PORT, CONFIG_UNIT_NONE, -1,
	    65535);
	if (0 == port) {
		return NULL;
	}
	if (0 > port) {
		port = CTRL_DEFAULT_PORT;
	}
	CALLOC(server, 1);
	server->server = NULL;
	server->server = udp_server_create(UDP_IPV4, port);
	if (NULL == server->server) {
		goto CTRL_SERVER_CREATE_FAIL;
	}
	if (!thread_start(&server->thread, server_run, server)) {
		log_error(LOGL, "Could not create control thread.");
		goto CTRL_SERVER_CREATE_FAIL;
	}

	return server;

CTRL_SERVER_CREATE_FAIL:
	if (NULL != server->server) {
		udp_server_free(&server->server);
	}
	FREE(server);
	return NULL;
}

void
ctrl_server_free(struct CtrlServer **a_server)
{
	struct CtrlServer *server;
	int dummy;

	server = *a_server;
	if (NULL == server) {
		return;
	}
	dummy = 0;
	if (!udp_server_write(server->server, &dummy, 1)) {
		log_die(LOGL, "Could not ask server to quit.");
	}
	thread_clean(&server->thread);
	udp_server_free(&server->server);
	FREE(*a_server);
}

int
packer_lookup(struct DatagramArray *a_dgram_array, size_t *a_dgram_array_i,
    struct Packer *a_packer)
{
	struct UDPDatagram *dgram;

	if ((size_t)-1 != *a_dgram_array_i &&
	    *a_dgram_array_i < a_dgram_array->num) {
		dgram = &a_dgram_array->array[*a_dgram_array_i];
		if (dgram->bytes > a_packer->ofs) {
			return 1;
		}
	}
	++*a_dgram_array_i;
	if (a_dgram_array->num <= *a_dgram_array_i) {
		log_error(LOGL, "Packer lookup %"PRIz" >= num %"PRIz".",
		    *a_dgram_array_i, a_dgram_array->num);
		return 0;
	}
	dgram = &a_dgram_array->array[*a_dgram_array_i];
	PACKER_CREATE(*a_packer, dgram->buf, dgram->bytes);
	/* MD5 and num+idx already parsed right after recv. */
	a_packer->ofs = 4 + 1 + 1;
	return 1;
}

void
scalar_list_free(struct CtrlConfigScalarList *a_list)
{
	while (!TAILQ_EMPTY(a_list)) {
		struct CtrlConfigScalar *s;

		s = TAILQ_FIRST(a_list);
		TAILQ_REMOVE(a_list, s, next);
		if (CONFIG_SCALAR_STRING == s->type) {
			FREE(s->value.str);
		}
		FREE(s);
	}
}

void
send_config_dump(struct UDPServer *a_server, struct UDPAddress const
    *a_address)
{
	struct PackerList packer_list;

	LOGF(verbose)(LOGL, "Sending config dump.");
	TAILQ_INIT(&packer_list);
	config_pack_children(&packer_list, NULL);
	send_packer_list(a_server, a_address, &packer_list);
	packer_list_free(&packer_list);
}

void
send_crate_array(struct UDPServer *a_server, struct UDPAddress const
    *a_address)
{
	struct PackerList packer_list;

	LOGF(verbose)(LOGL, "Sending crate array.");
	TAILQ_INIT(&packer_list);
	crate_pack(&packer_list);
	send_packer_list(a_server, a_address, &packer_list);
	packer_list_free(&packer_list);
}

void
send_crate_info(struct UDPServer *a_server, struct UDPAddress const
    *a_address, int a_crate_i)
{
	struct UDPDatagram dgram;
	struct Packer packer;

	LOGF(verbose)(LOGL, "Sending crate info for crate=%d.", a_crate_i);
	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, fail);
	crate_info_pack(&packer, a_crate_i);
	send_packer(a_server, a_address, &dgram, &packer);
fail:
	;
}

void
send_goc_read(struct UDPServer *a_server, struct UDPAddress const *a_address,
    uint8_t a_crate_i, uint8_t a_sfp, uint16_t a_card, uint32_t a_offset,
    uint16_t a_num, uint32_t a_id)
{
	struct PackerList packer_list;
	uint32_t *value;
	unsigned i;

	LOGF(verbose)(LOGL, "Sending goc read.");
	MALLOC(value, a_num);
	crate_gsi_pex_goc_read(a_crate_i, a_sfp, a_card, a_offset, a_num,
	    value);
	TAILQ_INIT(&packer_list);
	PACKER_LIST_PACK(packer_list, 32, a_id);
	for (i = 0; i < a_num; ++i) {
		PACKER_LIST_PACK(packer_list, 32, value[i]);
	}
	send_packer_list(a_server, a_address, &packer_list);
	packer_list_free(&packer_list);
}

void
send_module_access(struct UDPServer *a_server, struct UDPAddress const
    *a_address, uint8_t a_crate_i, uint8_t a_module_j, int a_submodule_k,
    struct Packer *a_packer)
{
	struct PackerList packer_list;

	LOGF(verbose)(LOGL, "Handling module access.");
	TAILQ_INIT(&packer_list);
	crate_module_access_pack(a_crate_i, a_module_j, a_submodule_k,
	    a_packer, &packer_list);
	send_packer_list(a_server, a_address, &packer_list);
	packer_list_free(&packer_list);
}

void
send_online(struct UDPServer *a_server, struct UDPAddress const *a_address)
{
	struct UDPDatagram dgram;
	struct Packer packer;

	LOGF(verbose)(LOGL, "Sending online status.");
	PACKER_CREATE_STATIC(packer, dgram.buf);
	PACK(packer, 32, NURDLIB_MD5, pack_fail);
	send_packer(a_server, a_address, &dgram, &packer);
pack_fail:
	;
}

void
send_packer(struct UDPServer *a_server, struct UDPAddress const *a_address,
    struct UDPDatagram *a_dgram, struct Packer const *a_packer)
{
	a_dgram->bytes = a_packer->ofs;
	udp_server_send(a_server, a_address, a_dgram);
}

void
send_packer_list(struct UDPServer *a_server, struct UDPAddress const
    *a_address, struct PackerList const *a_list)
{
	struct PackerNode *pn;
	unsigned i, num;

	num = 0;
	TAILQ_FOREACH(pn, a_list, next) {
		++num;
	}
	i = 0;
	TAILQ_FOREACH(pn, a_list, next) {
		pn->dgram.buf[4] = num;
		pn->dgram.buf[5] = i++;
		send_packer(a_server, a_address, &pn->dgram, &pn->packer);
	}
}

void
send_register_array(struct UDPServer *a_server, struct UDPAddress const
    *a_address, int a_crate_i, int a_module_j, int a_submodule_k)
{
	struct PackerList packer_list;

	LOGF(verbose)(LOGL, "Sending register list for crate=%d, module=%d, "
	    "submodule=%d.", a_crate_i, a_module_j, a_submodule_k);
	TAILQ_INIT(&packer_list);
	crate_register_array_pack(&packer_list, a_crate_i, a_module_j,
	    a_submodule_k);
	send_packer_list(a_server, a_address, &packer_list);
	packer_list_free(&packer_list);
}

void
server_run(void *a_server)
{
	struct CtrlServer *server;
	struct UDPAddress *address;

	LOGF(info)(LOGL, "Control server online.");
	server = a_server;

	address = NULL;
	for (;;) {
		struct UDPDatagram dgram;
		struct Packer packer;
		uint32_t md5;
		int submodule_int;
		uint8_t cmd, crate_i, module_j, has_submodule, submodule_k;
		uint8_t sfp;
		uint16_t card, num;
		uint32_t offset, id, value;

		udp_address_free(&address);
		dgram.bytes = 0;
		udp_server_receive(server->server, &address, &dgram, 1.0);
		if (0 == dgram.bytes) {
			continue;
		}
		if (NULL == address) {
			break;
		}
		PACKER_CREATE(packer, dgram.buf, dgram.bytes);
		if (!unpack32(&packer, &md5) || NURDLIB_MD5 != md5) {
			/* Client has the wrong md5, send ours. */
			PACKER_CREATE_STATIC(packer, dgram.buf);
			PACK(packer, 32, NURDLIB_MD5, pack_fail);
			udp_server_send(server->server, address, &dgram);
pack_fail:
			continue;
		}
		if (!unpack8(&packer, &cmd)) {
			continue;
		}
		submodule_int = -1;
		switch ((enum CtrlCommand)cmd) {
		case VL_CTRL_ONLINE:
			send_online(server->server, address);
			break;
		case VL_CTRL_CRATE_ARRAY:
			send_crate_array(server->server, address);
			break;
		case VL_CTRL_CRATE_INFO:
			if (unpack8(&packer, &crate_i)) {
				send_crate_info(server->server, address,
				    crate_i);
			}
			break;
		case VL_CTRL_REGISTER_LIST:
			if (unpack8(&packer, &crate_i) &&
			    unpack8(&packer, &module_j) &&
			    unpack8(&packer, &has_submodule)) {
				if (has_submodule &&
				    unpack8(&packer, &submodule_k)) {
					submodule_int = submodule_k;
				}
				send_register_array(server->server, address,
				    crate_i, module_j, submodule_int);
			}
			break;
		case VL_CTRL_CONFIG:
			if (unpack8(&packer, &crate_i) &&
			    unpack8(&packer, &module_j) &&
			    unpack8(&packer, &has_submodule)) {
				if (has_submodule &&
				    unpack8(&packer, &submodule_k)) {
					submodule_int = submodule_k;
				}
				if (!crate_config_write(crate_i, module_j,
				    submodule_int, &packer)) {
					LOGF(verbose)(LOGL,
					    "Could not write new config.");
				}
			}
			break;
		case VL_CTRL_CONFIG_DUMP:
			send_config_dump(server->server, address);
			break;
		case VL_CTRL_GOC_READ:
			if (unpack8(&packer, &crate_i) &&
			    unpack8(&packer, &sfp) &&
			    unpack16(&packer, &card) &&
			    unpack32(&packer, &offset) &&
			    unpack16(&packer, &num) &&
			    unpack32(&packer, &id)) {
				send_goc_read(server->server, address,
				    crate_i, sfp, card, offset, num, id);
			}
			break;
		case VL_CTRL_GOC_WRITE:
			if (unpack8(&packer, &crate_i) &&
			    unpack8(&packer, &sfp) &&
			    unpack16(&packer, &card) &&
			    unpack32(&packer, &offset) &&
			    unpack16(&packer, &num) &&
			    unpack32(&packer, &value)) {
				crate_gsi_pex_goc_write(crate_i, sfp, card,
				    offset, num, value);
			}
			break;
		case VL_CTRL_MODULE_ACCESS:
			if (unpack8(&packer, &crate_i) &&
			    unpack8(&packer, &module_j) &&
			    unpack8(&packer, &has_submodule)) {
				if (has_submodule &&
				    unpack8(&packer, &submodule_k)) {
					submodule_int = submodule_k;
				}
				send_module_access(server->server, address,
				    crate_i, module_j, submodule_int,
				    &packer);
			}
		}
	}
	LOGF(info)(LOGL, "Control server offline.");
}

int
unpack_config_list(struct DatagramArray *a_dgram_array, size_t
    *a_dgram_array_i, struct Packer *a_packer, struct CtrlConfigList *a_list)
{
	unsigned i, config_num;
	uint16_t u16;

	assert(TAILQ_EMPTY(a_list));
	if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
		return 0;
	}
	if (!unpack16(a_packer, &u16)) {
		fprintf(stderr, "%s:%d: Dgram missing # of configs.\n",
		    __FILE__, __LINE__);
		return 0;
	}
	config_num = u16;
	LOGF(debug)(LOGL, "config_num = %u", config_num);
	for (i = 0; config_num > i; ++i) {
		struct CtrlConfig *config;
		uint8_t u8;

		CALLOC(config, 1);
		TAILQ_INIT(&config->scalar_list);
		TAILQ_INIT(&config->child_list);
		TAILQ_INSERT_TAIL(a_list, config, next);
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &u8)) {
			log_error(LOGL, "Could not unpack 'type'.");
			return 0;
		}
		config->type = 1 & u8;
		config->is_touched = 0 != (2 & u8);
		LOGF(debug)(LOGL, "type = %d, is_touched = %d", config->type,
		    config->is_touched);
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack16(a_packer, &u16)) {
			log_error(LOGL, "Could not unpack 'name'.");
			return 0;
		}
		config->name = u16;
		LOGF(debug)(LOGL, "name = %s",
		    keyword_get_string(config->name));
		if (!unpack_scalar_list(a_dgram_array, a_dgram_array_i,
		    a_packer, &config->scalar_list)) {
			log_error(LOGL, "Could not unpack scalar list.");
			return 0;
		}
		if (CONFIG_BLOCK == config->type &&
		    KW_BARRIER != config->name &&
		    KW_TAGS != config->name &&
		    !unpack_config_list(a_dgram_array, a_dgram_array_i,
		    a_packer, &config->child_list)) {
			log_error(LOGL, "Could not unpack config list.");
			return 0;
		}
	}
	return 1;
}

void
unpack_loc(struct Packer *a_packer)
{
	char *file;
	uint32_t line;

	file = unpack_strdup(a_packer);
	if (!unpack32(a_packer, &line)) {
		line = -1;
	}
	log_error(LOGL, " __FILE__=%s __LINE__=%d.",
	    NULL == file ? "-nofile-" : file, line);
	FREE(file);
}

int
unpack_module_access(struct CtrlModuleAccess *a_arr, size_t a_arrn, struct
    DatagramArray *a_dgram_array, size_t *a_dgram_array_i)
{
	struct Packer packer;
	uint8_t i, num;

	ZERO(packer);
	if (!packer_lookup(a_dgram_array, a_dgram_array_i, &packer)) {
		log_error(LOGL, "No data in module access.");
		return 0;
	}
	if (!unpack8(&packer, &num)) {
		log_error(LOGL, "Could not get access #, "
		    "server reply corrupt.");
		return 0;
	}
	if (255 == num) {
		log_error(LOGL, "Could not access requested module, "
		    "-1 returned.");
		unpack_loc(&packer);
		return 0;
	}
	for (i = 0; i < a_arrn; ++i) {
		struct CtrlModuleAccess *a;

		a = &a_arr[i];
		if (!a->do_read) {
			continue;
		}
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, &packer)) {
			log_error(LOGL, "Could not unpack module access.");
			return 0;
		}
		if (16 == a->bits) {
			uint16_t u16;

			if (!unpack16(&packer, &u16)) {
				log_error(LOGL, "Could not unpack 0x%08x:%u.",
				    a->ofs, a->bits);
				return 0;
			}
			a->value = u16;
		} else if (32 == a->bits) {
			uint32_t u32;

			if (!unpack32(&packer, &u32)) {
				log_error(LOGL, "Could not unpack 0x%08x:%u.",
				    a->ofs, a->bits);
				return 0;
			}
			a->value = u32;
		}
		--num;
	}
	if (0 != num) {
		log_error(LOGL, "Module access number mismatch.");
	}
	return 1;
}

int
unpack_scalar(struct DatagramArray *a_dgram_array, size_t *a_dgram_array_i,
    struct Packer *a_packer, struct CtrlConfigScalar *a_scalar)
{
	union {
		uint64_t	u64;
		double	d;
	} u;
	uint32_t u32;
	uint16_t u16;
	uint8_t u8;

	if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
		log_error(LOGL, "Could not unpack.");
		return 0;
	}
	if (!unpack8(a_packer, &u8)) {
		log_error(LOGL, "Could not scalar type.");
		return 0;
	}
	a_scalar->type = u8;
	LOGF(debug)(LOGL, "type = %d", a_scalar->type);
	if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
		log_error(LOGL, "Could not unpack.");
		return 0;
	}
	if (!unpack16(a_packer, &u16)) {
		log_error(LOGL, "Could not scalar vector_index.");
		return 0;
	}
	a_scalar->vector_index = u16;
	LOGF(debug)(LOGL, "vector_index = %u", a_scalar->vector_index);
	switch (a_scalar->type) {
	case CONFIG_SCALAR_EMPTY:
		break;
	case CONFIG_SCALAR_DOUBLE:
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack64(a_packer, &u.u64)) {
			log_error(LOGL, "Could not unpack double.");
			return 0;
		}
		a_scalar->value.d.d = u.d;
		LOGF(debug)(LOGL, "  value.d.d = %f",
		    a_scalar->value.d.d);
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &u8)) {
			log_error(LOGL, "Could not unpack value.d.unit.");
			return 0;
		}
		a_scalar->value.d.unit = u8;
		LOGF(debug)(LOGL, "  value.d.unit = %d",
		    a_scalar->value.d.unit);
		break;
	case CONFIG_SCALAR_INTEGER:
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack32(a_packer, &u32)) {
			log_error(LOGL, "Could not unpack integer.");
			return 0;
		}
		a_scalar->value.i32.i = u32;
		LOGF(debug)(LOGL, "  value.i32.i = %d",
		    a_scalar->value.i32.i);
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &u8)) {
			log_error(LOGL, "Could not unpack value.d.unit.");
			return 0;
		}
		LOGF(debug)(LOGL, "  value.d.unit = %d",
		    a_scalar->value.d.unit);
		a_scalar->value.d.unit = u8;
		break;
	case CONFIG_SCALAR_KEYWORD:
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack16(a_packer, &u16)) {
			log_error(LOGL, "Could not unpack value.keyword.");
			return 0;
		}
		a_scalar->value.keyword = u16;
		LOGF(debug)(LOGL, "  value.keyword = %s",
		    keyword_get_string(a_scalar->value.keyword));
		break;
	case CONFIG_SCALAR_RANGE:
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &u8)) {
			log_error(LOGL,
			    "Could not unpack value.range.first.");
			return 0;
		}
		a_scalar->value.range.first = u8;
		LOGF(debug)(LOGL, "  value.range.first = %d",
		    a_scalar->value.range.first);
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &u8)) {
			log_error(LOGL, "Could not unpack value.range.last.");
			return 0;
		}
		a_scalar->value.range.last = u8;
		LOGF(debug)(LOGL, "  value.range.last = %d",
		    a_scalar->value.range.last);
		break;
	case CONFIG_SCALAR_STRING:
		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		a_scalar->value.str = unpack_strdup(a_packer);
		if (NULL == a_scalar->value.str) {
			log_error(LOGL, "Could not unpack string.");
			return 0;
		} else {
			LOGF(debug)(LOGL, "  value.str = %s",
			    a_scalar->value.str);
		}
		break;
	}
	return 1;
}

int
unpack_scalar_list(struct DatagramArray *a_dgram_array, size_t
    *a_dgram_array_i, struct Packer *a_packer, struct CtrlConfigScalarList
    *a_list)
{
	uint16_t i, scalar_num;

	assert(TAILQ_EMPTY(a_list));
	if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer)) {
		return 0;
	}
	if (!unpack16(a_packer, &scalar_num)) {
		fprintf(stderr, "%s:%d: Dgram missing # of scalers.\n",
		    __FILE__, __LINE__);
		return 0;
	}
	LOGF(debug)(LOGL, "scalar_num = %d", scalar_num);
	for (i = 0; scalar_num > i;) {
		struct CtrlConfigScalar *scalar;
		uint8_t rle_type;

		if (!packer_lookup(a_dgram_array, a_dgram_array_i, a_packer))
		{
			log_error(LOGL, "Could not unpack.");
			return 0;
		}
		if (!unpack8(a_packer, &rle_type)) {
			log_error(LOGL, "Could not unpack 'rle_type'.");
			return 0;
		}
		LOGF(debug)(LOGL, "  [%d] rle_type = %d", i, rle_type);
		if (0x80 & rle_type) {
			unsigned count, vector_index;

			count = (0x7f & rle_type) + 2;
			i += count;
			if (i > scalar_num) {
				log_error(LOGL, "RLE copy overrun "
				    "(i > scalar_num, %d > %d).",
				    i, scalar_num);
				return 0;
			}
			CALLOC(scalar, 1);
			TAILQ_INSERT_TAIL(a_list, scalar, next);
			if (!unpack_scalar(a_dgram_array, a_dgram_array_i,
			    a_packer, scalar)) {
				log_error(LOGL, "Could not unpack RLE ref.");
				return 0;
			}
			vector_index = scalar->vector_index + 1;
			for (--count; 0 != count; --count) {
				struct CtrlConfigScalar *next;

				CALLOC(next, 1);
				memcpy_(next, scalar, sizeof *next);
				next->vector_index = vector_index++;
				/* Gotta dup allocated stuff! */
				if (CONFIG_SCALAR_STRING == next->type) {
					next->value.str =
					    strdup_(next->value.str);
				}
				TAILQ_INSERT_TAIL(a_list, next, next);
			}
		} else {
			unsigned count;

			count = rle_type + 1;
			i += count;
			if (i > scalar_num) {
				log_error(LOGL, "RLE raw overrun "
				    "(i > scalar_num, %d > %d).",
				    i, scalar_num);
				return 0;
			}
			for (; 0 != count; --count) {
				CALLOC(scalar, 1);
				TAILQ_INSERT_TAIL(a_list, scalar, next);
				if (!unpack_scalar(a_dgram_array,
				    a_dgram_array_i, a_packer, scalar)) {
					log_error(LOGL,
					    "Could not unpack scalar.");
					return 0;
				}
			}
		}
	}
	return 1;
}
