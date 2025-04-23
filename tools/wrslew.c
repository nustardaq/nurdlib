/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2018-2024
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

#include <math.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <module/module.h>
#include <module/gsi_etherbone/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/serialio.h>
#include <nurdlib/trloii.h>
#include <util/argmatch.h>
#include <util/time.h>
#if NURDLIB_TRLOII
#	include <hwmap/hwmap_mapvme.h>
#endif
#if NCONF_mTRIDI_bYES
#	include <include/tridi_access.h>
#	include <include/tridi_functions.h>
#endif
#if NCONF_mVULOM4_bYES
#	include <include/trlo_access.h>
#	include <include/trlo_functions.h>
#endif
#if NCONF_mRFX1_bYES
#	include <include/rfx1_access.h>
#	include <include/rfx1_functions.h>
#endif

#define VETAR_JUMP 60
#define VME_MAP_LENGTH 0x00100000

#if NCONF_mTRIDI_bYES
#	define READ_TRIDI(field) \
    KW_GSI_TRIDI == g_module_type ? TRIDI_READ(trlo2.hwmap, field) :
#	define WRITE_TRIDI(field, val) \
    case KW_GSI_TRIDI: TRIDI_WRITE(trlo2.hwmap, field, val); break;
#else
#	define READ_TRIDI(field)
#	define WRITE_TRIDI(field, val)
#endif
#if NCONF_mVULOM4_bYES
#	define READ_TRLO(field) \
    KW_GSI_VULOM == g_module_type ? TRLO_READ(trlo2.hwmap, field) :
#	define WRITE_TRLO(field, val) \
    case KW_GSI_VULOM: TRLO_WRITE(trlo2.hwmap, field, val); break;
#else
#	define READ_TRLO(field)
#	define WRITE_TRLO(field, val)
#endif
#if NCONF_mRFX1_bYES
#	define READ_RFX1(field) \
    KW_GSI_RFX1 == g_module_type ? RFX1_READ(trlo2.hwmap, field) :
#	define WRITE_RFX1(field, val) \
    case KW_GSI_RFX1: RFX1_WRITE(trlo2.hwmap, field, val); break;
#else
#	define READ_RFX1(field)
#	define WRITE_RFX1(field, val)
#endif

#define READ(field) (\
	READ_TRIDI(field) \
	READ_TRLO(field) \
	READ_RFX1(field) \
	0)
#define WRITE(field, val) do { \
	switch (g_module_type) { \
	WRITE_TRIDI(field, val) \
	WRITE_TRLO(field, val) \
	WRITE_RFX1(field, val) \
	default: abort(); break; \
	} \
} while (0)

#define TRLO2_BIND(type, TYPE, CONN) do {\
	uint32_t address;\
	address = config_get_block_param_int32(module_block, 0);\
	trlo2.hwmap = hwmap_map_vme(NULL, address, VME_MAP_LENGTH, \
	    "Nurdlib:slew:TRLO II", &trlo2.unmapinfo); \
	trlo2.pulse_mux_dests = TYPE##_PULSE_MUX_DESTS;\
	trlo2.slew_counter_hi = TYPE##_SLEW_COUNTER_ADD_OFFSET_HI;\
	trlo2.slew_counter_lo = TYPE##_SLEW_COUNTER_ADD_OFFSET_LO;\
	trlo2.mux_output_i = TYPE##_MUX_DEST_##CONN##_OUT(1);\
	trlo2.serial_tstamp_out = TYPE##_MUX_SRC_SERIAL_TSTAMP_OUT;\
	trlo2.wired_zero = TYPE##_MUX_SRC_WIRED_ZERO;\
	/* Slow sender. */\
	TYPE##_WRITE(trlo2.hwmap, setup.serial_timestamp_speed, 3);\
	/* Setup latching. */\
	TYPE##_WRITE(trlo2.hwmap, setup.mux[TYPE##_MUX_DEST_##CONN##_OUT(0)],\
	    TYPE##_MUX_SRC_GATE_DELAY(0));\
	TYPE##_WRITE(trlo2.hwmap, setup.stretch[0], 5);\
	TYPE##_ACC_ALL_OR_CLEAR(trlo2.hwmap, setup.pulse_mux_dest_mask);\
	TYPE##_ACC_ALL_OR_SET(trlo2.hwmap, setup.pulse_mux_dest_mask,\
	    TYPE##_MUX_DEST_GATE_DELAY(0));\
	TYPE##_ACC_ALL_OR_SET(trlo2.hwmap, setup.pulse_mux_dest_mask,\
	    TYPE##_MUX_DEST_SLEW_LATCH);\
	TYPE##_ACC_ALL_OR_SET(trlo2.hwmap, setup.pulse_mux_dest_mask,\
	    TYPE##_MUX_DEST_SERIAL_TSTAMP_LATCH);\
	/* Forward tstamps. */\
	TYPE##_WRITE(trlo2.hwmap, setup.mux[TYPE##_MUX_DEST_SERIAL_TSTAMP_IN],\
	    TYPE##_MUX_SRC_SERIAL_TSTAMP_OUT);\
} while (0)

struct Sample {
	uint64_t	ts;
	uint64_t	slew;
	uint64_t	wr;
	time_t	pc;
};

static void	sighandler(int);
static void	usage(char const *, ...) FUNC_PRINTF(1, 2);

static char const *g_argv0;
static int g_do_run;
static enum Keyword g_module_type;

void
sighandler(int a_signum)
{
	static int counter = 0;

	(void)a_signum;
	printf("Caught ctrl-c.\n");
	g_do_run = 0;
	++counter;
	if (counter > 2) {
		printf("Another part of me won't stop, so I kill myself.\n");
		abort();
	}
}

void
usage(char const *a_fmt, ...)
{
	FILE *str;
	int exit_code;

	/* Some platforms don't like null or empty format strings. */
	if (0 == strcmp(a_fmt, "null")) {
		str = stdout;
		exit_code = EXIT_SUCCESS;
	} else {
		va_list args;

		va_start(args, a_fmt);
		vfprintf(stderr, a_fmt, args);
		fprintf(stderr, "\n");
		va_end(args);
		str = stderr;
		exit_code = EXIT_FAILURE;
	}
	fprintf(str,
"Usage: %s [-h] [-v] -c file\n", g_argv0);
	fprintf(str,
" -h, --help           This help.\n");
	fprintf(str,
" -v, --verbose        Spam mode in nurdlib readout.\n");
	fprintf(str,
" -c, --config=file    Nurdlib cfg file with one TRLO II and one VETAR.\n");
	fprintf(str,
"Example config file:\n");
	fprintf(str,
" GSI_TRIDI(0x02000000) {}\n");
	fprintf(str,
" GSI_VETAR(0x50000000) {}\n");
	fprintf(str,
" -p, --pc             Skip checking against PC time.\n");
	exit(exit_code);
}

int
main(int argc, char *argv[])
{
	struct GsiEtherboneModule vetar;
	struct Counter vetar_counter;
	struct ConfigBlock *module_block;
#if NURDLIB_TRLOII
	struct {
		void	volatile *hwmap;
		void	*unmapinfo;
		uint32_t	pulse_mux_dests;
		uint32_t	slew_counter_hi;
		uint32_t	slew_counter_lo;
		uint32_t	mux_output_i;
		uint32_t	serial_tstamp_out;
		uint32_t	wired_zero;
	} trlo2;
#endif
	double alpha, beta;
	char const *path = NULL;
	uint64_t iteration, wr_prev;
	uint32_t vetar_counter_soft = 0;
	int do_pc_check, is_ts_wired;

	g_argv0 = argv[0];
	signal(SIGINT, sighandler);
	if (1 == argc) {
		usage("null");
	}
	do_pc_check = 1;
	while (argc > g_argind) {
		char const *str;

		if (arg_match(argc, argv, 'h', "help", NULL)) {
			usage("null");
		} else if (arg_match(argc, argv, 'v', "verbose", NULL)) {
			log_level_push(g_log_level_spam_);
		} else if (arg_match(argc, argv, 'c', "config", &str)) {
			path = str;
		} else if (arg_match(argc, argv, 'p', "pc", NULL)) {
			do_pc_check = 0;
		} else {
			usage("Weird argument \"%s\".", argv[g_argind]);
		}
	}
	if (NULL == path) {
		usage("I need a config file!");
	}

	module_setup();
	config_load(path);

	for (module_block = config_get_block(NULL, KW_NONE); NULL !=
	    module_block; module_block = config_get_block_next(module_block,
	    KW_NONE)) {
		enum Keyword module_type;

		module_type = config_get_block_name(module_block);
#if NCONF_mTRIDI_bYES
		if (KW_GSI_TRIDI == module_type) {
			g_module_type = module_type;
			TRLO2_BIND(tridi, TRIDI, NIM);
		}
#endif
#if NCONF_mVULOM4_bYES
		if (KW_GSI_VULOM == module_type) {
			g_module_type = module_type;
			TRLO2_BIND(trlo, TRLO, LEMO);
		}
#endif
#if NCONF_mRFX1_bYES
		if (KW_GSI_RFX1 == module_type) {
			g_module_type = module_type;
			TRLO2_BIND(rfx1, RFX1, LEMO);
		}
#endif
		if (KW_GSI_VETAR == module_type) {
			gsi_etherbone_create(module_block, &vetar,
			    module_type);
			vetar.module.type = module_type;
			if (!gsi_etherbone_init_slow(&vetar)) {
				fprintf(stderr, "Failed to init VETAR.\n");
				exit(EXIT_FAILURE);
			}
			ZERO(vetar_counter);
			vetar.module.crate_counter = &vetar_counter;
			vetar_counter_soft = vetar.module.event_counter.value;
		} else {
			fprintf(stderr, "Weird module %s.\n",
			    keyword_get_string(module_type));
		}
	}

	WRITE(setup.mux[trlo2.mux_output_i], trlo2.serial_tstamp_out);
	is_ts_wired = 1;

	;
	if (0x10000 > READ(setup.slew_counter_add)) {
		WRITE(setup.slew_counter_add, 1 << 24);
	}

	/*
	 * Setup latching.
	 * The macros should work for both TRIDI and VULOM.
	 */

	puts("Entering slewing loop.");
	alpha = 0;
	beta = 1;
	iteration = 0;
	wr_prev = 0;
	for (g_do_run = 1; g_do_run;) {
		static struct Sample sample_array[200];
		struct Sample *entry0, *entry;
		uint64_t target;
		double sum_x, sum_y, sum_xx, sum_xy;
		double k_target, k_curr, k_new;
		size_t i, n;
		uint32_t add;

		/* Grab a group of timestamp pairs. */
		entry = entry0 = sample_array;
		for (i = 0; i < LENGTH(sample_array);) {
			struct EventBuffer event_buffer;
			uint32_t ts_hi, ts_lo;
			uint32_t slew_hi, slew_lo;
			uint32_t ret;

			/* Latch. */
			WRITE(pulse.pulse, trlo2.pulse_mux_dests);
			SERIALIZE_IO;

			/* PC time. */
			entry->pc = time(NULL);

			/* TRLO II timestamp. */
			ts_lo = READ(serial_tstamp[0]);
			SERIALIZE_IO;
			ts_hi = READ(serial_tstamp[0]);
			entry->ts = ((uint64_t)ts_hi << 32) | ts_lo;

			/* TRLO II slewing timestamp. */
			slew_hi = READ(out.slew_counter_cur_hi);
			SERIALIZE_IO;
			slew_lo = READ(out.slew_counter_cur_lo);
			SERIALIZE_IO;
			entry->slew =
			    (((uint64_t)slew_hi << 32) | slew_lo) << 4;

			/* VETAR WR timestamp. */
			++vetar_counter_soft;
			ret = gsi_etherbone_readout_dt(&vetar);
			if (0 != ret) {
				fprintf(stderr, "Vetar readout_dt failed.\n");
			} else if (vetar.module.event_counter.value !=
			    vetar_counter_soft) {
				fprintf(stderr, "Vetar counter mismatch "
				    "(hw=%08x,sw=%08x).\n",
				    vetar.module.event_counter.value,
				    vetar_counter_soft);
			} else {
				event_buffer.bytes = sizeof(uint64_t);
				event_buffer.ptr = &entry->wr;
				ret = gsi_etherbone_readout(&vetar,
				    &event_buffer);
				if (0 != ret) {
					fprintf(stderr,
					    "Vetar readout failed.\n");
				}
			}
			ret = gsi_etherbone_check_empty(&vetar);
			if (0 != ret) {
				fprintf(stderr, "Vetar check_empty "
				    "failed.\n");
			}
			if (do_pc_check &&
			    (entry->wr*1e-9 < entry->pc - VETAR_JUMP ||
			     entry->wr*1e-9 > entry->pc + VETAR_JUMP)) {
				fprintf(stderr, "Large mismatch between WR=%f"
				    " and PC=%f times.\n", entry->wr*1e-9,
				    entry->pc*1.);
				ret = 1;
			}
			if (entry->wr < wr_prev) {
				fprintf(stderr, "Out of order WR timestamps, "
				    "from=%08x:%08x to=%08x:%08x.\n",
				    (uint32_t)(wr_prev >> 32),
				    (uint32_t)wr_prev,
				    (uint32_t)(entry->wr >> 32),
				    (uint32_t)entry->wr);
				ret = 1;
			}
			if (0 != ret) {
				/*
				 * The Vetar can give crazy WR timestamps, we
				 * should wait until it behaves reasonable
				 * again. Since this can take some time, we
				 * have to:
				 * -) Unwire the TRLO II timestamper so all
				 * clients see bad timestamps.
				 * -) Restart the slewing, happens after the
				 * timestamper is wired back, hopefully the
				 * recovery takes long enough that the next
				 * timestamp is much later.
				 */
				if (is_ts_wired) {
					WRITE(setup.mux[trlo2.mux_output_i],
					    trlo2.wired_zero);
					is_ts_wired = 0;
				}
				time_sleep(1);
				continue;
			}
			wr_prev = entry->wr;
			if (!is_ts_wired) {
				/* Vetar back on track, rewire and restart. */
				WRITE(setup.mux[trlo2.mux_output_i],
				    trlo2.serial_tstamp_out);
				is_ts_wired = 1;
				time_sleep(1);
				iteration = 0;
				entry = sample_array;
				i = 0;
				continue;
			}

			if (0 != (0x40000000 & ts_hi)) {
				/*
				 * Sometimes we're too fast for the ratatime
				 * decoder, wait a little.
				 */
				time_sleep(0.1);
				continue;
			}

			if (0 == i || LENGTH(sample_array) - 1 == i) {
				printf(" Raw   ts=%08x:%08x(%u) "
				    "slew=%08x:%08x(%u) "
				    "wr=%08x:%08x(%u) pc=%u\n",
				    (uint32_t)(entry->ts >> 32),
				    (uint32_t)entry->ts,
				    (uint32_t)(entry->ts * 1e-9),
				    (uint32_t)(entry->slew >> 32),
				    (uint32_t)entry->slew,
				    (uint32_t)(entry->slew * 1e-9),
				    (uint32_t)(entry->wr >> 32),
				    (uint32_t)entry->wr,
				    (uint32_t)(entry->wr * 1e-9),
				    (uint32_t)entry->pc);
				if (0 == i) {
					printf(" ...\n");
				}
			}

			++entry;
			++i;
			/* Grab 'n' samples over 0.1 seconds. */
			time_sleep(0.1 / LENGTH(sample_array));
		}

		/* Find min-max differences. */
		{
			int64_t lower = 1e9;
			int64_t upper = -1e9;

			entry = sample_array;
			for (i = 0; i < LENGTH(sample_array); ++i) {
				int64_t d;

				d = entry->ts - entry->wr;
				lower = MIN(lower, d);
				upper = MAX(upper, d);
				++entry;
			}
			printf(" ---> lower=%d upper=%d <---\n", (int)lower,
			    (int)upper);
		}

		/*
		 * Ordinary least squares to fit the current
		 * 'slew_counter_add' to the WR world, works on diffs against
		 * the first sample to improve precision, Dslew~ = alpha +
		 * beta * Dwr.
		 */
		n = LENGTH(sample_array) - 1;
		entry = sample_array;
		sum_xy = sum_xx = sum_y = sum_x = 0;
		for (i = 0; i < n; i++) {
			double x, y;

			++entry;
			x = entry->wr - entry0->wr;
			y = entry->ts - entry0->ts;
			sum_x += x;
			sum_y += y;
			sum_xx += x*x;
			sum_xy += x*y;
		}
		beta = (n * sum_xy - sum_x * sum_y) /
		    (n * sum_xx - sum_x * sum_x);
		alpha = (sum_y - beta * sum_x) / n;
		printf("Fit Dslew~ = %g + %g * Dwr\n", alpha, beta);

		if (2 > iteration) {
			uint64_t offset;
			uint32_t ofs_hi, ofs_lo;

			/*
			 * First iterations -> set offset and initial
			 * 'slew_counter_add', only set it in the beginning
			 * because timestamps must never go backwards when
			 * we're on the long trudge.
			 *
			 * Two iterations seem enough to hit bulls eye.
			 */
			offset = entry->wr - entry->ts;
			ofs_hi = offset >> 32;
			ofs_lo = offset;
			printf("Offset=%08x:%08x\n", ofs_hi, ofs_lo);
			offset >>= 4;
			ofs_hi = offset >> 32;
			ofs_lo = offset;
			WRITE(setup.slew_counter_offset, ofs_hi);
			SERIALIZE_IO;
			WRITE(pulse.slew_counter, trlo2.slew_counter_hi);
			SERIALIZE_IO;
			WRITE(setup.slew_counter_offset, ofs_lo);
			SERIALIZE_IO;
			WRITE(pulse.slew_counter, trlo2.slew_counter_lo);
			SERIALIZE_IO;

			/* Set the first estimated slopes directly. */
			add = READ(setup.slew_counter_add) << 4;
			add /= beta;
			printf("Initial slew_counter_add=%08x\n", add);
			WRITE(setup.slew_counter_add, (add + 8) >> 4);

			++iteration;
			continue;
		}

		if (alpha > 1e6) {
			fprintf(stderr, "Timestamps seem okay, but the fit is"
			    " off the walls, restarting.\n");
			entry = sample_array;
			for (i = 0; i < LENGTH(sample_array); ++i, ++entry) {
				printf("%2"PRIz": ts=%08x:%08x "
				    "slew=%08x:%08x wr=%08x:%08x\n",
				    i,
				    (uint32_t)(entry->ts >> 32),
				    (uint32_t)entry->ts,
				    (uint32_t)(entry->slew >> 32),
				    (uint32_t)entry->slew,
				    (uint32_t)(entry->wr >> 32),
				    (uint32_t)entry->wr);
			}
			iteration = 0;
			entry = sample_array;
			i = 0;
			continue;
		}

		/* Estimate slope for slewer to hit target in 1 s. */
		target = entry->wr + (uint64_t)1e9;
		k_target = (double)(target - entry->ts) / (target -
		    entry->wr);

		/*
		 * Take the geom mean, new = sqrt(old * estimate), old =
		 * 'add', estimate = '(add / k_curr) * k_target', reduces
		 * oscillations.
		 */
		k_curr = beta;
		k_new = sqrt(k_target / k_curr);
		printf("Slope old=%g target=%g new=%g\n", k_curr, k_target,
		    k_new);
		add = READ(setup.slew_counter_add) << 4;
		printf("slew_counter_add=%08x", add);
		add *= k_new;
		printf(" -> %08x\n", add);
		WRITE(setup.slew_counter_add, (add + 8) >> 4);
	}
	puts("Exiting slewing loop.");

	gsi_etherbone_deinit(&vetar);
#if NCONF_mTRIDI_bYES
	if (KW_GSI_TRIDI == g_module_type) {
		tridi_unmap_hardware(trlo2.unmapinfo);
	}
#endif
#if NCONF_mVULOM4_bYES
	if (KW_GSI_VULOM == g_module_type) {
		trlo_unmap_hardware(trlo2.unmapinfo);
	}
#endif
	config_shutdown();

	puts("Done.");
	return 0;
}
