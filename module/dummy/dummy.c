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

/* Module functions and data which are publicly visible. */
#include <module/dummy/dummy.h>
/* Module internal structs. */
#include <module/dummy/internal.h>
/* Generated register offsets. */
#include <module/dummy/offsets.h>
/* To floor() things. */
#include <math.h>
/* Hardware access. */
#include <module/map/map.h>
/* Nurdlib configuration structs/functions. */
#include <nurdlib/config.h>
/* Nurdlib crate structs/functions. */
#include <nurdlib/crate.h>
/* Bits bits bits. */
#include <util/bits.h>
/* For PRIz to print size_t. */
#include <util/fmtmod.h>
/* What time is it? */
#include <util/time.h>

/*
 * Define the name of the module, used for informative printing.
 * Usually the name of the module is <manufacturer>_<model>, but we use
 * 'Dummy' here.
 */
#define NAME "Dummy"

/*
 * The following functions must be implemented:
 *  NAME_deinit
 *  NAME_destroy
 *  NAME_get_map
 *  NAME_get_signature
 *  NAME_init_fast
 *  NAME_init_slow
 *  NAME_parse_data
 *  NAME_readout
 *  NAME_readout_dt_on
 *  NAME_zero_suppress
 * If the module should use auto-pedestals:
 *  NAME_use_pedestals
 * See 'module/module.h' for more information on each one.
 */

/* Prototypes. */
MODULE_PROTOTYPES(dummy);

/* Prototypes only for this module, to pretend it provides some data. */
static void	init_registers(struct DummyModule *);
static void	store_event(struct DummyModule *, unsigned);

/* Implementation. */

/*
 * Returns 0 if module buffers are empty, otherwise a suitable crate readout
 * fail bit, see CRATE_READOUT_FAIL_* in nurdlib/crate.h.
 */
uint32_t
dummy_check_empty(struct Module *a_module)
{
	(void)a_module;
	LOGF(spam)(LOGL, NAME" check_empty.");
	return 0;
}

void
dummy_counter_increase(struct Module *a_module, unsigned a_inc)
{
	struct DummyModule *dummy;

	LOGF(spam)(LOGL, NAME" counter_increase {");
	MODULE_CAST(KW_DUMMY, dummy, a_module);
	a_module->event_counter.value = MAP_READ(dummy->sicy_map, counter);
	a_module->event_counter.value =
	    (a_module->event_counter.value + a_inc) &
	    dummy->module.event_counter.mask;
	MAP_WRITE(dummy->sicy_map, counter, a_module->event_counter.value);
	LOGF(spam)(LOGL, NAME" counter_increase(ctr=0x%08x) }",
	    a_module->event_counter.value);
}

struct Module *
dummy_create_(struct Crate *a_crate, struct ConfigBlock *a_block)
{
	enum Keyword const c_mode[] = {
		KW_SLOPE,
		KW_TRACES
	};
	struct DummyModule *dummy;

	(void)a_crate;
	LOGF(verbose)(LOGL, NAME" create {");

	/*
	 * Macro to create a module and initialise the base object.
	 */
	MODULE_CREATE(dummy);

	/*
	 * Let's say that we can store at most 32 events until the module
	 * buffers are full.
	 *
	 * A particular value can have two important consequences:
	 *
	 * 0: This is the default value, which means that the module does not
	 * deliver data and the crate will not call any of its readout*
	 * functions or check event counters. Initialization calls, also on
	 * crate reinits, will still be made.
	 *
	 * >0: If nurdlib knows how to, it will tell the DAQ to accept at most
	 * this many triggers, before the readout procedure starts.
	 * If the max event size is 100 B and the buffer size is 1000 B, this
	 * number must not be more than 10!
	 */
	dummy->module.event_max = 32;

	/*
	 * Get module configuration from config block for very long-lived
	 * settings.
	 *
	 * Configs are rather strict and do not allow probing for config types
	 * or what configs are available, e.g. one cannot do "if
	 * (config_exists(self_destruct) { start(); }".
	 *
	 * Please note that settings that can be set online should happen in
	 * init_fast, so check also there!
	 */

	/* Get an integer value from the block's first parameter. */
	dummy->address = config_get_block_param_int32(a_block, 0);
	LOGF(verbose)(LOGL, "Address=%08x.", dummy->address);

	/*
	 * Get value from a config named KW_MODE.
	 * arg 0: Config block.
	 * arg 1: Config name.
	 * arg 2: List of allowed keywords.
	 */
	dummy->mode = CONFIG_GET_KEYWORD(a_block, KW_MODE, c_mode);
	LOGF(verbose)(LOGL, "Mode=%s.",
	    keyword_get_string(dummy->mode));

	/*
	 * Let's pretend that we have 24 bits for the event counter.
	 */
	dummy->module.event_counter.mask = BITS_MASK_TOP(23);

	LOGF(verbose)(LOGL, NAME" create }");

	return (void *)dummy;
}

void
dummy_deinit(struct Module *a_module)
{
	struct DummyModule *dummy;

	LOGF(verbose)(LOGL, NAME" deinit {");

	/* This asserts the module type before casting. */
	MODULE_CAST(KW_DUMMY, dummy, a_module);

	/* Undo 'init_fast' and then 'init_slow'. */
	(void)dummy;

	LOGF(verbose)(LOGL, NAME" deinit }");
}

void
dummy_destroy(struct Module *a_module)
{
	struct DummyModule *dummy;

	LOGF(verbose)(LOGL, NAME" destroy {");

	MODULE_CAST(KW_DUMMY, dummy, a_module);
	/*
	 * Undo 'create_'.
	 * Since we didn't allocate any resources for this module, we don't
	 * have to do anything here.
	 */
	(void)dummy;
	LOGF(verbose)(LOGL, NAME" destroy }");
}

struct Map *
dummy_get_map(struct Module *a_module)
{
	struct DummyModule *dummy;

	LOGF(verbose)(LOGL, NAME" get_map {");
	MODULE_CAST(KW_DUMMY, dummy, a_module);
	LOGF(verbose)(LOGL, NAME" get_map }");
	return dummy->sicy_map;
}

void
dummy_get_signature(struct ModuleSignature const **a_array, size_t *a_num)
{
	/*
	 * This function lets nurdlib know recognisable bits in the first word
	 * of the module payload. Modules that are reportedly too similar will
	 * require a BARRIER keyword between them in the config.
	 * TODO: Put the module id in 'store_event' in the header to make this
	 * more interesting!
	 */
	MODULE_SIGNATURE_BEGIN
	    MODULE_SIGNATURE(
		0,
		BITS_MASK(16, 31),
		0x1234)
	MODULE_SIGNATURE_END(a_array, a_num)
}

int
dummy_init_fast(struct Crate *a_crate, struct Module *a_module)
{
	struct ModuleGate gate;
	struct DummyModule *dummy;
	size_t ch;

	LOGF(verbose)(LOGL, NAME" init_fast {");

	/*
	 * Some modules need crate-wide resources, this dummy does not.
	 */
	(void)a_crate;

	MODULE_CAST(KW_DUMMY, dummy, a_module);

	/*
	 * Get a 32 bit integer without a unit.
	 * arg 2: Value unit.
	 * arg 3: Minimum value inclusive.
	 * arg 4: Maximum value inclusive.
	 * I.e. value in [-30, 30] in this case.
	 */
	dummy->offset = config_get_int32(a_module->config,
	    KW_OFFSET, CONFIG_UNIT_NONE, -30, 30);
	LOGF(verbose)(LOGL, "Offset=%d.", dummy->offset);

	/*
	 * Get a floating point value in milli-volts.
	 * arg 2: Value unit.
	 * arg 3: Minimum value inclusive.
	 * arg 4: Maximum value inclusive.
	 * I.e. value in [-10, 10] in this case.
	 */
	dummy->range = config_get_double(a_module->config,
	    KW_RANGE, CONFIG_UNIT_MV, -1e6, 1e6);
	LOGF(verbose)(LOGL, "Range=%f mV.", dummy->range);

	/*
	 * Get a bitmask.
	 * arg 2: First bit inclusive.
	 * arg 3: Last bit inclusive.
	 * I.e. 32 bits in this case.
	 */
	dummy->channel_enable = config_get_bitmask(a_module->config,
	    KW_CHANNEL_ENABLE, 0, 31);
	LOGF(verbose)(LOGL, "Channel_enable=0x%08x.",
	    dummy->channel_enable);

	/*
	 * Get a 32 bit integer array.
	 * The array must have the exact size of the configured array!
	 */
	CONFIG_GET_INT_ARRAY(dummy->thresholds, a_module->config,
	    KW_THRESHOLD, CONFIG_UNIT_NONE, 0, 255);
	for (ch = 0; ch < LENGTH(dummy->thresholds); ++ch) {
		LOGF(verbose)(LOGL, "Threshold[%"PRIz"]=%u.",
		    ch, dummy->thresholds[ch]);
	}

	/*
	 * Get data from GATE config block.
	 * arg 2: minimum trigger delay in ns.
	 * arg 3: maximum trigger delay in ns.
	 * arg 4-5: Gate width limits in nanoseconds.
	 */
	module_gate_get(&gate, a_module->config, 0, 2000, 0, 4000);
	dummy->gate_delay = gate.time_after_trigger_ns;
	dummy->gate_width = gate.width_ns;
	LOGF(verbose)(LOGL, "Gate=(delay:%d, width:%d)",
	    dummy->gate_delay, dummy->gate_width);

	/* Set module registers according to configs. */
	for (ch = 0; ch < LENGTH(dummy->thresholds); ++ch) {
		MAP_WRITE(dummy->sicy_map, threshold(ch),
		    dummy->thresholds[ch]);
	}

	/* User callback when we're finished. */
	if (NULL != dummy->init_callback) {
		dummy->init_callback();
	}

	LOGF(verbose)(LOGL, NAME" init_fast }");

	/*
	 * All went well, return 1!
	 * If you would like nurdlib to keep reinitializing, e.g. if the
	 * cause of the problem is a missing cable and the DAQ would run as
	 * soon as that cable is plugged in, return 0.
	 * If there's no simple recovery, e.g. the DAQ computer needs a power
	 * cycle, call log_die.
	 */
	return 1;
}

int
dummy_init_slow(struct Crate *a_crate, struct Module *a_module)
{
	struct DummyModule *dummy;

	LOGF(verbose)(LOGL, NAME" init_slow {");

	/*
	 * Some modules need crate-wide resources, this dummy module does not.
	 */
	(void)a_crate;

	MODULE_CAST(KW_DUMMY, dummy, a_module);

	/* Map the module. */
	dummy->sicy_map = map_map(dummy->address, MAP_SIZE, KW_NOBLT, 0, 0,
	    MAP_POKE_REG(read_only), MAP_POKE_REG(write_only), 0);

	/* Populate the registers with some numbers for testing. */
	init_registers(dummy);

	LOGF(verbose)(LOGL, NAME" init_slow }");

	/*
	 * See init_fast for more info on the return value.
	 */
	return 1;
}

void
dummy_init_set_callback(struct Module *a_module, void (*a_callback)(void))
{
	struct DummyModule *dummy;

	MODULE_CAST(KW_DUMMY, dummy, a_module);
	dummy->init_callback = a_callback;
}

void
dummy_memtest(struct Module *a_module, enum Keyword a_mode)
{
	(void)a_module;
	(void)a_mode;
}

uint32_t
dummy_parse_data(struct Crate *a_crate, struct Module *a_module, struct
    EventConstBuffer const *a_event_buffer, int a_do_pedestals)
{
	struct DummyModule *dummy;

	(void)a_crate;
	(void)a_do_pedestals;
	LOGF(spam)(LOGL, NAME" parse_data(%p,%"PRIz") {", a_event_buffer->ptr,
	    a_event_buffer->bytes);
	MODULE_CAST(KW_DUMMY, dummy, a_module);
	(void)dummy;
	/*
	 * Add data checking here.
	 * If a_do_pedestals is non-zero, ADC values should be set to
	 * added for pedestal evaluation with "module_pedestal_add". The
	 * caen_v7nn module has a rather simple implementation.
	 */
	LOGF(spam)(LOGL, NAME" parse_data }");
	return 0;
}

uint32_t
dummy_readout(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct DummyModule *dummy;
	uint32_t *outp, result;
	unsigned i, idx, ev;

	(void)a_crate;

	LOGF(spam)(LOGL, NAME" readout {");
	result = 0;

	MODULE_CAST(KW_DUMMY, dummy, a_module);

	/* Dereference destination memory pointer. */
	outp = a_event_buffer->ptr;

	/*
	 * This is only needed for this dummy module to simulate readout
	 * data in its buffer.
	 */
	store_event(dummy, dummy->event_diff);

	/* Read the event from the buffer. */
	idx = 0;
	for (ev = 0; ev < dummy->event_diff; ++ev) {
		uint32_t header;
		unsigned wordcount;

		/*
		 * IMPORTANT NOTE: NEVER do more than one thing in macros such
		 * as MAP_READ/WRITE, ie do NOT increment 'idx' inside!
		 */
		header = MAP_READ(dummy->sicy_map, buffer(idx));
		++idx;
		wordcount = header & 0xff;
		/*
		 * Make sure we have enough memory still. This checks within
		 * the ANSI C standard that outp[wordcount] is a valid
		 * location inside the given memory area. Don't forget that we
		 * also write the header, so the array offset is "1 +
		 * wordcount - 1"!
		 */
		if (!MEMORY_CHECK(*a_event_buffer, &outp[wordcount])) {
			result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
			goto dummy_readout_done;
		}
		*outp++ = header;
		for (i = 0; i < wordcount; ++i) {
			*outp++ = MAP_READ(dummy->sicy_map, buffer(idx));
			++idx;
		}
	}

dummy_readout_done:
	/*
	 * Save new buffer pointer, this function checks that the new pointer
	 * is valid within the original buffer, and reduces unique locations
	 * of pointer arithmetic.
	 */
	EVENT_BUFFER_ADVANCE(*a_event_buffer, outp);
	LOGF(spam)(LOGL, NAME" readout }");
	return result;
}

uint32_t
dummy_readout_dt(struct Crate *a_crate, struct Module *a_module)
{
	struct DummyModule *dummy;

	(void)a_crate;
	LOGF(spam)(LOGL, NAME" readout_dt {");
	MODULE_CAST(KW_DUMMY, dummy, a_module);
	/*
	 * This is typically a counter that is read from the module and is
	 * incremented on e.g. accepted triggers.
	 * Because this is a software module we have to increment it ourselves
	 * with 'dummy_counter_increase'.
	 */
	a_module->event_counter.value = MAP_READ(dummy->sicy_map, counter);

	LOGF(spam)(LOGL, NAME" readout_dt }");
	return 0;
}

/*
 * Module-wide properties are set here.
 */
void
dummy_setup_(void)
{
	/*
	 * Let's assume that this module supports early DT release, i.e. we
	 * can read out the buffers while the hardware accepts and converts
	 * new signals.
	 */
	MODULE_SETUP(dummy, MODULE_FLAG_EARLY_DT);
}

/* Implementation of module specific methods. */

void
init_registers(struct DummyModule *a_dummy)
{
	size_t ch;

	MAP_WRITE(a_dummy->sicy_map, clear, 0);
	for (ch = 0; ch < LENGTH(a_dummy->thresholds); ++ch) {
		MAP_WRITE(a_dummy->sicy_map, threshold(ch), 0);
	}
	MAP_WRITE(a_dummy->sicy_map, counter, 0);
}

void
store_event(struct DummyModule *a_dummy, unsigned a_event_diff)
{
	static unsigned s_event_number = 0;
	uint32_t ch;
	unsigned idx;
	unsigned id;
	unsigned wordcount;
	unsigned ev;

	idx = 0;
	id = 0x1234;
	wordcount = 4 + 32 - 1;

	for (ev = 0; ev < a_event_diff; ++ev) {
		double time_now;

		/* Header. */
		MAP_WRITE(a_dummy->sicy_map, buffer(idx),
		    (id << 16) | wordcount);
		++idx;

		/* Timestamp. */
		time_now = time_getd();
		MAP_WRITE(a_dummy->sicy_map, buffer(idx), floor(time_now));
		++idx;
		MAP_WRITE(a_dummy->sicy_map, buffer(idx),
		    1e9 * (time_now - floor(time_now)));
		++idx;

		/* Data. */
		for (ch = 0; ch < N_CHANNELS; ++ch) {
			MAP_WRITE(a_dummy->sicy_map, buffer(idx),
			    (ch << 24) | 0xaabb);
			++idx;
		}

		/* Footer. */
		MAP_WRITE(a_dummy->sicy_map, buffer(idx), s_event_number);
		++idx;
		++s_event_number;
	}
}
