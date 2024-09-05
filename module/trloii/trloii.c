/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2024
 * Bastian Löher
 * Michael Munch
 * Stephane Pietri
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

#include <module/trloii/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>
#include <nurdlib/serialio.h>
#include <nurdlib/keyword.h>
#include <nurdlib/log.h>
#include <nurdlib/trloii.h>
#include <util/bits.h>
#include <util/fmtmod.h>

#define NAME "TRLO II"

static void			cvt_set(struct Module *, unsigned);
static uint32_t			get_accept_pulse(struct Module *, void *,
    struct Counter *) FUNC_RETURNS;
static uint32_t			get_accept_trig(struct Module *, void *,
    struct Counter *) FUNC_RETURNS;
static uint32_t			get_in_ecl(struct Module *, void *, struct
    Counter *) FUNC_RETURNS;
static uint32_t			get_in_nim(struct Module *, void *, struct
    Counter *) FUNC_RETURNS;
static uint32_t			get_master_start(struct Module *, void *,
    struct Counter *) FUNC_RETURNS;
static struct TRLOIIModule	*get_trloii(struct Module *);

/*
 * Implementations for when we have the module support.
 */

#define IMPL_YES(type, TYPE, NIM_LEMO, nim_lemo)\
static void \
type##_cvt_set(void volatile *a_opaque, unsigned a_cvt)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	/* TODO: This is a hack. Trust me. */\
	TYPE##_WRITE(type, trimi.fcatime, a_cvt - 1);\
	TYPE##_WRITE(type, trimi.ctime, a_cvt);\
	printf("CVT %u\n", a_cvt);\
}\
static void \
type##_get_accept_pulse(void volatile *a_opaque, struct Counter *a_counter)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, pulse.pulse,  TYPE##_PULSE_MUX_SRC_SCALER_LATCH);\
	SERIALIZE_IO;\
	a_counter->value = TYPE##_READ(type, \
	    scaler.mux_src[TYPE##_MUX_SRC_ACCEPT_PULSE]);\
}\
static void \
type##_get_accept_trig(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, pulse.pulse,  TYPE##_PULSE_MUX_SRC_SCALER_LATCH);\
	SERIALIZE_IO;\
	a_counter->value = TYPE##_READ(type, \
	    scaler.mux_src[TYPE##_MUX_SRC_ACCEPT_TRIG(a_i)]);\
}\
static void \
type##_get_in_ecl(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, pulse.pulse, TYPE##_PULSE_MUX_SRC_SCALER_LATCH);\
	SERIALIZE_IO;\
	a_counter->value = TYPE##_READ(type, \
	    scaler.mux_src[TYPE##_MUX_SRC_ECL_IN(a_i)]);\
}\
static void \
type##_get_in_nim(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, pulse.pulse, TYPE##_PULSE_MUX_SRC_SCALER_LATCH);\
	SERIALIZE_IO;\
	a_counter->value = TYPE##_READ(type, \
	    scaler.mux_src[TYPE##_MUX_SRC_##NIM_LEMO##_IN(a_i)]);\
}\
static void \
type##_get_master_start(void volatile *a_opaque, struct Counter *a_counter)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, pulse.pulse, TYPE##_PULSE_MUX_SRC_SCALER_LATCH);\
	SERIALIZE_IO;\
	a_counter->value = TYPE##_READ(type, \
	    scaler.mux_src[TYPE##_MUX_SRC_MASTER_START]);\
}\
static void \
type##_multi_event_set_limit(void volatile *a_opaque, unsigned a_limit)\
{\
	type##_opaque volatile *type;\
\
	type = a_opaque;\
	TYPE##_WRITE(type, setup.max_multi_trig, a_limit);\
}

/*
 * Place-holders for when we don't.
 */

#define DIE(TYPE) \
    log_die(LOGL, #TYPE" not supported in this nurdlib build. "\
	"Check the make output carefully!")

#define IMPL_NO(type, TYPE)\
static void \
type##_cvt_set(void volatile *a_opaque, unsigned a_cvt)\
{\
	(void)a_opaque;\
	(void)a_cvt;\
	DIE(TYPE);\
}\
static void \
type##_get_accept_pulse(void volatile *a_opaque, struct Counter *a_counter)\
{\
	(void)a_opaque;\
	(void)a_counter;\
	DIE(TYPE);\
}\
static void \
type##_get_accept_trig(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	(void)a_opaque;\
	(void)a_i;\
	(void)a_counter;\
	DIE(TYPE);\
}\
static void \
type##_get_in_ecl(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	(void)a_opaque;\
	(void)a_i;\
	(void)a_counter;\
	DIE(TYPE);\
}\
static void \
type##_get_in_nim(void volatile *a_opaque, unsigned a_i, struct Counter \
    *a_counter)\
{\
	(void)a_opaque;\
	(void)a_i;\
	(void)a_counter;\
	DIE(TYPE);\
}\
static void \
type##_get_master_start(void volatile *a_opaque, struct Counter *a_counter)\
{\
	(void)a_opaque;\
	(void)a_counter;\
	DIE(TYPE);\
}\
static void \
type##_multi_event_set_limit(void volatile *a_opaque, unsigned a_limit)\
{\
	(void)a_opaque;\
	(void)a_limit;\
	DIE(TYPE);\
}

#if NCONF_mTRIDI_bYES
#	include <include/tridi_access.h>
IMPL_YES(tridi, TRIDI, NIM, nim)
#else
IMPL_NO(tridi, TRIDI)
#endif
#if NCONF_mVULOM4_bYES
#	include <include/trlo_access.h>
IMPL_YES(trlo, TRLO, LEMO, lemo)
#else
IMPL_NO(trlo, TRLO)
#endif
#if NCONF_mRFX1_bYES
#	include <include/rfx1_access.h>
IMPL_YES(rfx1, RFX1, LEMO, lemo)
#else
IMPL_NO(rfx1, RFX1)
#endif

void
cvt_set(struct Module *a_module, unsigned a_cvt_ns)
{
	struct TRLOIIModule *trloii;
	unsigned cvt;

	LOGF(spam)(LOGL, NAME" cvt_set(%u ns) {", a_cvt_ns);
	cvt = (a_cvt_ns + 99) / 100;
	cvt = 0xffff - MIN(cvt, 0xfffe);
	LOGF(spam)(LOGL, "Raw=0x%x.", cvt);
	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_cvt_set(trloii->hwmap, cvt);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_cvt_set(trloii->hwmap, cvt);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_cvt_set(trloii->hwmap, cvt);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	LOGF(spam)(LOGL, NAME" cvt_set }");
}

uint32_t
get_accept_pulse(struct Module *a_module, void *a_data, struct Counter
    *a_counter)
{
	struct TRLOIIModule *trloii;

	(void)a_data;
	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_get_accept_pulse(trloii->hwmap, a_counter);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_get_accept_pulse(trloii->hwmap, a_counter);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_get_accept_pulse(trloii->hwmap, a_counter);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	return 0;
}

uint32_t
get_accept_trig(struct Module *a_module, void *a_data, struct Counter
    *a_counter)
{
	struct TRLOIIModule *trloii;

	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_get_accept_trig(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_get_accept_trig(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_get_accept_trig(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	return 0;
}

uint32_t
get_in_ecl(struct Module *a_module, void *a_data, struct Counter *a_counter)
{
	struct TRLOIIModule *trloii;

	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_get_in_ecl(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_get_in_ecl(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_get_in_ecl(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	return 0;
}

uint32_t
get_in_nim(struct Module *a_module, void *a_data, struct Counter *a_counter)
{
	struct TRLOIIModule *trloii;

	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_get_in_nim(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_get_in_nim(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_get_in_nim(trloii->hwmap, (uintptr_t)a_data,
		    a_counter);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	return 0;
}

uint32_t
get_master_start(struct Module *a_module, void *a_data, struct Counter
    *a_counter)
{
	struct TRLOIIModule *trloii;

	(void)a_data;
	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == a_module->type) {
		tridi_get_master_start(trloii->hwmap, a_counter);
	} else if (KW_GSI_VULOM == a_module->type) {
		trlo_get_master_start(trloii->hwmap, a_counter);
	} else if (KW_GSI_RFX1 == a_module->type) {
		rfx1_get_master_start(trloii->hwmap, a_counter);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	return 0;
}

struct TRLOIIModule *
get_trloii(struct Module *a_module)
{
	if (KW_GSI_TRIDI != a_module->type &&
	    KW_GSI_VULOM != a_module->type &&
	    KW_GSI_RFX1 != a_module->type) {
		log_die(LOGL, "I want a TRIDI, RFX1 or a VULOM, not a %s!",
		    keyword_get_string(a_module->type));
	}
	return (void *)a_module;
}

void
trloii_create(struct Crate *a_crate, struct TRLOIIModule *a_trloii, struct
    ConfigBlock *a_block)
{
	LOGF(verbose)(LOGL, NAME" create {");

	a_trloii->address = config_get_block_param_int32(a_block, 0);
	LOGF(info)(LOGL, "Address=%08x.", a_trloii->address);

	a_trloii->has_timestamp = config_get_boolean(a_block, KW_TIMESTAMP);
	if (a_trloii->has_timestamp) {
		LOGF(info)(LOGL, "Has timestamps.");
		/*
		 * Skip MSB because data_counter counts 32-bitters but event
		 * counters count 64-bit timestamps.
		 */
		a_trloii->module.event_counter.mask = BITS_MASK_TOP(30);
	} else {
		a_trloii->module.event_counter.mask = 0;
	}

	a_trloii->multi_event_tag_name = config_get_string(a_block,
	    KW_MULTI_EVENT);
	if ('\0' != *a_trloii->multi_event_tag_name) {
		LOGF(verbose)(LOGL, "Multi-event tag = \"%s\".",
		    a_trloii->multi_event_tag_name);
	}

	a_trloii->acvt_has = config_get_boolean(a_block, KW_ACVT);
	if (a_trloii->acvt_has) {
		crate_acvt_set(a_crate, &a_trloii->module, cvt_set);
	}

	LOGF(verbose)(LOGL, NAME" create }");
}

char const *
trloii_get_multi_event_tag_name(struct Module *a_module)
{
	struct TRLOIIModule *trloii;
	char const *name;

	LOGF(spam)(LOGL, NAME" get_multi_event_tag_name {");
	trloii = get_trloii(a_module);
	name = trloii->multi_event_tag_name;
	LOGF(spam)(LOGL, NAME" get_multi_event_tag_name(name=\"%s\") }",
	    name);
	return '\0' == *name ? NULL : name;
}

struct tridi_readout_control *
trloii_get_tridi_ctrl(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_tridi_ctrl {");
	MODULE_CAST(KW_GSI_TRIDI, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_tridi_ctrl }");
	return trloii->ctrl;
}

tridi_opaque volatile *
trloii_get_tridi_map(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_tridi_map {");
	MODULE_CAST(KW_GSI_TRIDI, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_tridi_map }");
	return trloii->hwmap;
}

struct trlo_readout_control *
trloii_get_trlo_ctrl(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_trlo_ctrl {");
	MODULE_CAST(KW_GSI_VULOM, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_trlo_ctrl }");
	return trloii->ctrl;
}

trlo_opaque volatile *
trloii_get_trlo_map(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_trlo_map {");
	MODULE_CAST(KW_GSI_VULOM, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_trlo_map }");
	return trloii->hwmap;
}

struct rfx1_readout_control *
trloii_get_rfx1_ctrl(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_rfx1_ctrl {");
	MODULE_CAST(KW_GSI_RFX1, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_rfx1_ctrl }");
	return trloii->ctrl;
}

rfx1_opaque volatile *
trloii_get_rfx1_map(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" get_rfx1_map {");
	MODULE_CAST(KW_GSI_RFX1, trloii, a_module);
	LOGF(spam)(LOGL, NAME" get_rfx1_map }");
	return trloii->hwmap;
}

int
trloii_is_timestamper(struct Module *a_module)
{
	struct TRLOIIModule *trloii;

	LOGF(spam)(LOGL, NAME" is_timestamper {");
	trloii = get_trloii(a_module);
	LOGF(spam)(LOGL, NAME" is_timestamper(%d) }", trloii->has_timestamp);
	return trloii->has_timestamp;
}

void
trloii_multi_event_set_limit(struct Module *a_module, unsigned a_limit)
{
	struct TRLOIIModule *trloii;

	LOGF(verbose)(LOGL, NAME" multi_event_set_limit(%u) {", a_limit);
	trloii = get_trloii(a_module);
	if (KW_GSI_TRIDI == trloii->module.type) {
		tridi_multi_event_set_limit(trloii->hwmap, a_limit);
	} else if (KW_GSI_VULOM == trloii->module.type) {
		trlo_multi_event_set_limit(trloii->hwmap, a_limit);
	} else if (KW_GSI_RFX1 == trloii->module.type) {
		rfx1_multi_event_set_limit(trloii->hwmap, a_limit);
	} else {
		log_die(LOGL, "This cannot happen!");
	}
	LOGF(verbose)(LOGL, NAME" multi_event_set_limit }");
}

void
trloii_scaler_parse(struct Crate *a_crate, struct ConfigBlock *a_block, char
    const *a_name, struct TRLOIIModule *a_trloii)
{
	enum Keyword const c_type_array[] = {
		KW_ACCEPT_PULSE,
		KW_ACCEPT_TRIG,
		KW_ECL,
		KW_MASTER_START,
		KW_NIM
	};
	ScalerGetCallback func;
	enum Keyword type;
	unsigned ch;

	LOGF(verbose)(LOGL, NAME" scaler_parse(%s) {", a_name);
	type = CONFIG_GET_KEYWORD(a_block, KW_TYPE, c_type_array);
	if (KW_ACCEPT_PULSE == type) {
		ch = 0;
		func = get_accept_pulse;
	} else if (KW_ACCEPT_TRIG == type) {
		ch = config_get_int32(a_block, KW_CHANNEL, CONFIG_UNIT_NONE,
		    0, 31);
		func = get_accept_trig;
	} else if (KW_ECL == type) {
		ch = config_get_int32(a_block, KW_CHANNEL, CONFIG_UNIT_NONE,
		    0, 31);
		func = get_in_ecl;
	} else if (KW_MASTER_START == type) {
		ch = 0;
		func = get_master_start;
	} else if (KW_NIM == type) {
		ch = config_get_int32(a_block, KW_CHANNEL, CONFIG_UNIT_NONE,
		    0, 31);
		func = get_in_nim;
	} else {
		log_die(LOGL, "Should not happen!");
	}
	crate_scaler_add(a_crate, a_name, &a_trloii->module,
	    (void *)(uintptr_t)ch, func);
	LOGF(verbose)(LOGL, NAME" scaler_parse }");
}
