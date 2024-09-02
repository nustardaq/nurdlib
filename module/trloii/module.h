/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2017-2024
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

#ifndef MODULE_TRLOII_MODULE_H
#define MODULE_TRLOII_MODULE_H

#include <module/trloii/internal.h>
#include <nurdlib/base.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/keyword.h>
#include <nurdlib/log.h>
#include <nurdlib/trloii.h>
#include <util/bits.h>

struct Crate;

#define CHKSUM8(x) (((x)&0xff) ^ (((x)>>8)&0xff) ^ (((x)>>16)&0xff) ^ \
    (((x)>>24)&0xff))

/*
 * TODO: Only make one scaler latch.
 */
#define MAP_INPUTS(trloii, T2_NAME, CONN) do { \
		uint32_t mask_; \
		unsigned ch_i_; \
		mask_ = config_get_bitmask(trloii->module.config, \
		    KW_##CONN, 0, T2_NAME##_NUM_MUX_SRC_##CONN - 1); \
		LOGF(debug)(LOGL, #CONN "-mask = 0x%x.", mask_); \
		for (ch_i_ = 0; 0 != mask_; ++ch_i_) { \
			if (0 != (1 & mask_)) { \
				uint8_t i_; \
				i_ = T2_NAME##_MUX_SRC_##CONN(ch_i_); \
				trloii->scaler[trloii->scaler_n] = i_; \
				LOGF(debug)(LOGL, \
				    " " #CONN "[%"PRIz"] = %u.", \
				    trloii->scaler_n, i_); \
				++trloii->scaler_n; \
			} \
			mask_ >>= 1; \
		} \
	} while (0)
#define MAP_INPUTS_SUFF(trloii, T2_NAME, CONN, SUFF) do { \
		uint32_t mask_; \
		unsigned ch_i_; \
		mask_ = config_get_bitmask(trloii->module.config, \
		    KW_##CONN, 0, T2_NAME##_NUM_MUX_SRC_##CONN##SUFF - 1); \
		LOGF(debug)(LOGL, #CONN "-mask = 0x%x.", mask_); \
		for (ch_i_ = 0; 0 != mask_; ++ch_i_) { \
			if (0 != (1 & mask_)) { \
				uint8_t i_; \
				i_ = T2_NAME##_MUX_SRC_##CONN##SUFF(ch_i_); \
				trloii->scaler[trloii->scaler_n] = i_; \
				LOGF(debug)(LOGL, \
				    " " #CONN "[%"PRIz"] = %u.", \
				    trloii->scaler_n, i_); \
				++trloii->scaler_n; \
			} \
			mask_ >>= 1; \
		} \
	} while (0)

#define TRLOII_MODULE_EMPTY(NLIB_NAME, nlib_name) \
void \
gsi_##nlib_name##_deinit(struct Module *a_module) \
{ \
	(void)a_module; \
	log_die(LOGL, "gsi_" #nlib_name "_deinit not implemented."); \
} \
int \
gsi_##nlib_name##_init_fast(struct Crate *a_crate, struct Module *a_module) \
{ \
	(void)a_crate; \
	(void)a_module; \
	log_die(LOGL, "gsi_" #nlib_name "_init_fast not implemented."); \
	return 0; \
} \
int \
gsi_##nlib_name##_init_slow(struct Crate *a_crate, struct Module *a_module) \
{ \
	(void)a_crate; \
	(void)a_module; \
	log_die(LOGL, "gsi_" #nlib_name "_init_slow not implemented."); \
	return 0; \
} \
uint32_t \
gsi_##nlib_name##_readout(struct Crate *a_crate, struct Module *a_module, \
    struct EventBuffer *a_event_buffer) \
{ \
	(void)a_crate; \
	(void)a_module; \
	(void)a_event_buffer; \
	log_die(LOGL, "gsi_" #nlib_name "_readout not implemented."); \
	return 0; \
} \
uint32_t \
gsi_##nlib_name##_readout_dt(struct Crate *a_crate, struct Module *a_module) \
{ \
	(void)a_crate; \
	(void)a_module; \
	log_die(LOGL, "gsi_" #nlib_name "_readout_dt not implemented."); \
	return 0; \
} \
uint32_t \
gsi_##nlib_name##_readout_shadow(struct Crate *a_crate, struct Module \
    *a_module, struct EventBuffer *a_event_buffer) \
{ \
	(void)a_crate; \
	(void)a_module; \
	(void)a_event_buffer; \
	log_die(LOGL, "gsi_" #nlib_name "_readout_shadow not implemented."); \
	return 0; \
}

#define TRLOII_MODULE(NLIB_NAME, nlib_name, T2_NAME, t2_name, NIM_LEMO) \
void \
gsi_##nlib_name##_deinit(struct Module *a_module) \
{ \
	struct TRLOIIModule *trloii; \
\
	LOGF(verbose)(LOGL, NAME" deinit {"); \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	FREE(trloii->ctrl); \
	t2_name##_unmap_hardware(trloii->unmapinfo); \
	LOGF(verbose)(LOGL, NAME" deinit }"); \
} \
int \
gsi_##nlib_name##_init_fast(struct Crate *a_crate, struct Module *a_module) \
{ \
	struct TRLOIIModule *trloii; \
\
	(void)a_crate; \
	LOGF(verbose)(LOGL, NAME" init_fast {"); \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	trloii->data_counter = 0; \
	trloii->module.event_counter.value = 0; \
	trloii->module.shadow.data_counter_value = 0; \
	trloii->scaler_n = 0; \
	/* TODO: ECL_IO_IN? */ \
	MAP_INPUTS_SUFF(trloii, T2_NAME, ECL, _IN); \
	MAP_INPUTS_SUFF(trloii, T2_NAME, NIM_LEMO, _IN); \
	MAP_INPUTS(trloii, T2_NAME, PRNG_POISSON); \
	MAP_INPUTS(trloii, T2_NAME, PULSER); \
	MAP_INPUTS(trloii, T2_NAME, ALL_OR); \
	MAP_INPUTS(trloii, T2_NAME, COINCIDENCE); \
	MAP_INPUTS(trloii, T2_NAME, INPUT_COINC); \
	MAP_INPUTS(trloii, T2_NAME, ACCEPT_TRIG); \
	LOGF(verbose)(LOGL, NAME" init_fast }"); \
	return 1; \
} \
int \
gsi_##nlib_name##_init_slow(struct Crate *a_crate, struct Module *a_module) \
{ \
	struct TRLOIIModule *trloii; \
	t2_name##_opaque volatile *opaque; \
\
	(void)a_crate; \
	LOGF(verbose)(LOGL, NAME" init_slow {"); \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	opaque = t2_name##_setup_map_hardware(trloii->address >> 24, \
	    &trloii->unmapinfo); \
	trloii->hwmap = opaque; \
	trloii->ctrl = malloc(sizeof(struct t2_name##_readout_control)); \
	/* TODO: Config! */ \
	t2_name##_setup_readout_control(opaque, trloii->ctrl, \
	    0xcf, 0xc7, 1 << 1, 1 << 3); \
	t2_name##_setup_check_version(opaque);\
	T2_NAME##_WRITE(opaque, pulse.pulse, \
	    T2_NAME##_PULSE_MULTI_TRIG_BUF_CLEAR);\
	T2_NAME##_WRITE(opaque, pulse.pulse, \
	    T2_NAME##_PULSE_SERIAL_TSTAMP_BUF_CLEAR);\
	T2_NAME##_WRITE(opaque, pulse.pulse, \
	    T2_NAME##_PULSE_SERIAL_TSTAMP_FAIL_CLEAR);\
	SERIALIZE_IO;\
	LOGF(verbose)(LOGL, NAME" init_slow }"); \
	return 1; \
} \
uint32_t \
gsi_##nlib_name##_readout(struct Crate *a_crate, struct Module *a_module, \
    struct EventBuffer *a_event_buffer) \
{ \
	struct TRLOIIModule *trloii; \
	t2_name##_opaque volatile *opaque; \
	uint32_t *p32; \
	uint32_t chksum_header; \
	uint32_t w1, w2; \
	uint32_t chksum_data = 0; \
	uint32_t ret; \
	unsigned event, entries; \
\
	(void)a_crate; \
	LOGF(spam)(LOGL, NAME" readout {"); \
	ret = 0; \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	p32 = a_event_buffer->ptr; \
	opaque = trloii->hwmap; \
	if (!trloii->has_timestamp) { \
		goto nlib_name##_readout_done; \
	} \
	entries = trloii->ts_status & 0x3ff; \
	if (sizeof(uint32_t) * entries > a_event_buffer->bytes) { \
		log_error(LOGL, "Timestamp storage (%"PRIz"B) too small for" \
		    " %u words (%"PRIz"B).", a_event_buffer->bytes, \
		    entries, sizeof(uint32_t) * entries); \
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH; \
		goto nlib_name##_readout_done; \
	} \
	for (event = 0; event < entries; event += 2) { \
		uint64_t stamp; \
		int desync; \
\
		w1 = T2_NAME##_READ(opaque, serial_tstamp[0]); \
		w2 = T2_NAME##_READ(opaque, serial_tstamp[0]); \
\
		chksum_data ^= CHKSUM8(w1); \
		chksum_data ^= CHKSUM8(w2); \
\
		stamp = (((uint64_t)w2) << 32) | w1; \
		desync = (w2 >> 30) & 1; \
		if (desync) { \
			log_error(LOGL, "Serial timestamp receiver has data" \
			    " marked desync (0x%08x 0x%08x).", \
			    w1, w2); \
			ret = CRATE_READOUT_FAIL_DATA_CORRUPT; \
			goto nlib_name##_readout_done; \
		} \
		/* Store it in network-order/big-endian. */ \
		*p32++ = htonl_(stamp >> 32); \
		*p32++ = htonl_(stamp & 0xffffffff); \
	} \
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32); \
	chksum_header = (trloii->ts_status >> 24) & 0x0ff; \
	if (chksum_data != chksum_header) { \
		log_error(LOGL, "Serial timestamp header has checksum=" \
		    "0x%02x, but data=0x%02x.", chksum_header, chksum_data); \
		ret = CRATE_READOUT_FAIL_DATA_CORRUPT; \
		goto nlib_name##_readout_done; \
	} \
	trloii->data_counter += entries; \
nlib_name##_readout_done: \
	if (0 != ret) { \
		log_error(LOGL, "Serial timestamp buffer failed, " \
		    "status=0x%08x.", trloii->ts_status); \
		T2_NAME##_WRITE(opaque, pulse.pulse, \
		    T2_NAME##_PULSE_SERIAL_TSTAMP_BUF_CLEAR); \
	} \
	if (0 != trloii->scaler_n) { \
		unsigned i; \
		*p32++ = trloii->scaler_n; \
		for (i = 0; i < trloii->scaler_n; ++i) { \
			*p32++ = T2_NAME##_READ(opaque, \
			    scaler.mux_src[trloii->scaler[i]]); \
		} \
	} \
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32); \
	LOGF(spam)(LOGL, NAME" readout(0x%08x) }", ret); \
	return ret; \
} \
uint32_t \
gsi_##nlib_name##_readout_dt(struct Crate *a_crate, struct Module *a_module) \
{ \
	struct TRLOIIModule *trloii; \
	t2_name##_opaque volatile *opaque; \
	uint32_t status; \
	/* \
	 * status_a <= (checksum & \
	 * bad_bits & \
	 * ptn_sync_lost & ptn_synced & \
	 * bitstr_sync_lost & bitstr_synced & \
	 * (not in_sync) & \
	 * "00000" & \
	 * entries); \
	 */ \
	unsigned entries; \
	uint32_t sync_lost, synced, badbits, ret; \
\
	(void)a_crate; \
	LOGF(spam)(LOGL, NAME" readout_dt {"); \
	ret = 0; \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	if (!trloii->has_timestamp) { \
		goto nlib_name##_readout_dt_done; \
	} \
	opaque = trloii->hwmap; \
	status = T2_NAME##_READ(opaque, out.serial_timestamp_status); \
	LOGF(spam)(LOGL, "Status=0x%08x.", status); \
\
	entries   = (status >>  0) & 0x3ff; \
	/*no_sync   = (status >> 15) & 0x01;*/ \
	sync_lost = (status >> 16) & 0x0a; \
	synced    = (status >> 16) & 0x05; \
	badbits   = (status >> 20) & 0x0f; \
	/* \
	 * drift  = (status >> 10) & 0x7ff; clock freq mismatch, \
	 *                                  small, so ignored. \
	 * desync = (status >> 21) & 0x001; (currently) desynced? \
	 * fail   = (status >> 22) & 0x00f; recent fail (desync). \
	 */ \
\
	a_module->event_counter.value = \
	    (trloii->data_counter + entries) / 2; \
	if (sync_lost) { \
		log_error(LOGL, "Serial timestamp receiver has had sync " \
		    "failure, status=0x%08x.", status); \
		T2_NAME##_WRITE(opaque, pulse.pulse, \
		    T2_NAME##_PULSE_SERIAL_TSTAMP_FAIL_CLEAR); \
		if (!synced) { \
			log_error(LOGL, "Serial timestamp receiver not " \
			    "synced, status=0x%08x.", status); \
			T2_NAME##_WRITE(opaque, pulse.pulse, \
			    T2_NAME##_PULSE_SERIAL_TSTAMP_BUF_CLEAR); \
			ret = CRATE_READOUT_FAIL_DATA_MISSING; \
			goto nlib_name##_readout_dt_done; \
		} \
	} \
	if (badbits) { \
	        log_error(LOGL, "Serial timestamp receiver has seen bad " \
		    "bits (#=%u), status=0x%08x.", badbits, status); \
		T2_NAME##_WRITE(opaque, pulse.pulse, \
		    T2_NAME##_PULSE_SERIAL_TSTAMP_FAIL_CLEAR); \
		T2_NAME##_WRITE(opaque, pulse.pulse, \
		    T2_NAME##_PULSE_SERIAL_TSTAMP_BUF_CLEAR); \
		ret = CRATE_READOUT_FAIL_DATA_MISSING; \
		goto nlib_name##_readout_dt_done; \
	} \
	trloii->ts_status = status; \
\
	T2_NAME##_WRITE(opaque, pulse.pulse, T2_NAME##_PULSE_MUX_SRC_SCALER_LATCH); \
\
nlib_name##_readout_dt_done: \
	LOGF(spam)(LOGL, NAME" readout_dt(ret=0x%08x,ctr=0x%08x) }", ret, \
	    a_module->event_counter.value); \
	return ret; \
} \
uint32_t \
gsi_##nlib_name##_readout_shadow(struct Crate *a_crate, struct Module \
    *a_module, struct EventBuffer *a_event_buffer) \
{ \
	struct TRLOIIModule *trloii; \
	t2_name##_opaque volatile *opaque; \
	uint32_t status; \
	/* \
	 * status_a <= (checksum & \
	 * bad_bits & \
	 * ptn_sync_lost & ptn_synced & \
	 * bitstr_sync_lost & bitstr_synced & \
	 * (not in_sync) & \
	 * "00000" & \
	 * entries); \
	 */ \
	unsigned entries; \
	uint32_t ret; \
	uint32_t *p32; \
	uint32_t chksum_header; \
	uint32_t chksum_data = 0; \
	unsigned word; \
\
	(void)a_crate; \
	ret = 0; \
	MODULE_CAST(KW_GSI_##NLIB_NAME, trloii, a_module); \
	if (!trloii->has_timestamp) { \
		goto nlib_name##_readout_shadow_done; \
	} \
	opaque = trloii->hwmap; \
	status = T2_NAME##_READ(opaque, out.serial_timestamp_status); \
	/* \
	 * Shadow version cannot do 64 bitters because check-sum is done per \
	 * 32 and we may happen inside a timestamp. Endianness caution! \
	 */ \
	entries = (status >>  0) & 0x3ff; \
	if (sizeof(uint32_t) * entries > a_event_buffer->bytes) { \
		log_error(LOGL, "Timestamp storage (%"PRIz"B) too small for" \
		    " %u words (%"PRIz"B).", a_event_buffer->bytes, \
		    entries, sizeof(uint32_t) * entries); \
		ret = CRATE_READOUT_FAIL_DATA_TOO_MUCH; \
		goto nlib_name##_readout_shadow_done; \
	} \
	p32 = a_event_buffer->ptr; \
	for (word = 0; word < entries; ++word) { \
		uint32_t u32; \
\
		u32 = T2_NAME##_READ(opaque, serial_tstamp[0]); \
		chksum_data ^= CHKSUM8(u32); \
		if (0 == (1 & trloii->data_counter)) { \
			trloii->u32_low = u32; \
		} else { \
			int desync; \
\
			/* Store it in network-order/big-endian. */ \
			*p32++ = htonl_(u32); \
			*p32++ = htonl_(trloii->u32_low); \
			desync = (u32 >> 30) & 1; \
			if (desync) { \
				log_error(LOGL, \
				    "Serial timestamp receiver has data " \
				    "marked desync (0x%08x).", u32); \
				ret = CRATE_READOUT_FAIL_DATA_CORRUPT; \
				goto nlib_name##_readout_shadow_done; \
			} \
		} \
		++trloii->data_counter; \
	} \
	a_module->shadow.data_counter_value = trloii->data_counter / 2; \
	EVENT_BUFFER_ADVANCE(*a_event_buffer, p32); \
	chksum_header = (status >> 24) & 0xff; \
	if (chksum_data != chksum_header) { \
		log_error(LOGL, "Serial timestamp header has checksum=" \
		    "0x%02x, but data=0x%02x.", chksum_header, chksum_data); \
		ret = CRATE_READOUT_FAIL_DATA_CORRUPT; \
		goto nlib_name##_readout_shadow_done; \
	} \
nlib_name##_readout_shadow_done: \
	if (0 != ret) { \
		T2_NAME##_WRITE(opaque, pulse.pulse, \
		    T2_NAME##_PULSE_SERIAL_TSTAMP_BUF_CLEAR); \
	} \
	return ret; \
}

#define TRLOII_MODULE_GENERIC(nlib_name) \
MODULE_PROTOTYPES(gsi_##nlib_name); \
static uint32_t gsi_##nlib_name##_readout_shadow(struct Crate *, struct \
    Module *, struct EventBuffer *) FUNC_RETURNS; \
uint32_t \
gsi_##nlib_name##_check_empty(struct Module *a_module) \
{ \
	(void)a_module; \
	/* TODO? */ \
	return 0; \
} \
struct Module * \
gsi_##nlib_name##_create_(struct Crate *a_crate, struct ConfigBlock \
    *a_block) \
{ \
	struct TRLOIIModule *trloii; \
\
	LOGF(verbose)(LOGL, NAME" create {"); \
	MODULE_CREATE(trloii); \
	/* TODO: Need to clarify how many events can be stored. */ \
	trloii->module.event_max = 170; \
	trloii_create(a_crate, trloii, a_block); \
	MODULE_SCALER_PARSE(a_crate, a_block, trloii, trloii_scaler_parse); \
	LOGF(verbose)(LOGL, NAME" create }"); \
	return (void *)trloii; \
} \
void \
gsi_##nlib_name##_destroy(struct Module *a_module) \
{ \
	(void)a_module; \
	LOGF(verbose)(LOGL, NAME" destroy."); \
} \
struct Map * \
gsi_##nlib_name##_get_map(struct Module *a_module) \
{ \
	(void)a_module; \
	LOGF(verbose)(LOGL, NAME" get_map."); \
	return NULL; \
} \
void \
gsi_##nlib_name##_get_signature(struct ModuleSignature const **a_array, \
    size_t *a_num) \
{ \
	MODULE_SIGNATURE_BEGIN \
	MODULE_SIGNATURE_END(a_array, a_num) \
} \
void \
gsi_##nlib_name##_memtest(struct Module *a_module, enum Keyword a_mode) \
{ \
	(void)a_module; \
	(void)a_mode; \
} \
uint32_t \
gsi_##nlib_name##_parse_data(struct Crate *a_crate, struct Module *a_module, \
    struct EventConstBuffer const *a_event_buffer, int a_do_pedestals) \
{ \
	(void)a_crate; \
	(void)a_module; \
	(void)a_event_buffer; \
	(void)a_do_pedestals; \
	return 0; \
} \
void \
gsi_##nlib_name##_setup_(void) \
{ \
	MODULE_SETUP(gsi_##nlib_name, MODULE_FLAG_EARLY_DT); \
	/* TODO: Need to run this! */ \
	MODULE_CALLBACK_BIND(gsi_##nlib_name, readout_shadow); \
}

#endif
