/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
 * Håkan T Johansson
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

/*
 * Small (not exactly minimal) example for gluing nurdlib to MBS/drasi.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <util/stdint.h>
#include <util/string.h>

#define CONFIG_NAME_PRIMARY "main.cfg"

#if FUSER_DRASI
#	include <lwroc_message_inject.h>
#	include <lwroc_mon_block.h>
#	include <lwroc_net_conn_monitor.h>
#	include <lwroc_readout.h>
#	include <lwroc_thread_util.h>
#	include <lwroc_track_timestamp.h>
#	include <lmd/lwroc_lmd_ev_sev.h>
#	include <f_user_daq.h>
#	include <util/thread.h>
#	include "lwroc_parse_util.h"

#	define LAND_VME_ADCTDC_COUNTER 0x10000000
#	define LAND_VME_MULTIEVENT 0x40000000

/* Callback into drasi threading infrastructure. */
static struct Mutex g_thread_no_mutex;
static int g_thread_no;

extern struct lwroc_readout_functions _lwroc_readout_functions;
extern lwroc_mon_block *_lwroc_mon_main_handle;
extern lwroc_net_conn_monitor *_lwroc_mon_main_system_handle;
static lmd_stream_handle *g_lmd_stream;

static void
thread_callback(void *a_data)
{
	int no;

	(void)a_data;
	thread_mutex_lock(&g_thread_no_mutex);
	/* Make sure to start with 1. */
	no = ++g_thread_no;
	thread_mutex_unlock(&g_thread_no_mutex);
	/* TODO: Can we give a good purpose? */
	lwroc_thread_user_init(no, NULL);
}

static void untriggered_loop(int *);

#elif FUSER_MBS
#	include <time.h>
#	include <smem_mbs.h>
#	include <typedefs.h>
#	include <s_setup.h>
#	include <f_ut_get_setup_seg.h>
#	include <f_ut_printm.h>
#endif

#include <module/map/map_cmvlc.h>

#include <s_veshe.h>
#include <sbs_def.h>
#include <nurdlib.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>

int		f_user_get_virt_ptr(long *, long []);
int		f_user_init(unsigned char, long *, long *, long *);
int		f_user_readout(unsigned char, unsigned char, register long *,
    register long *, long *, s_veshe *, long *, long *);
int		f_user_trig_clear(unsigned char);

void		local_cmdline_usage(void);
int		local_parse_cmdline_arg(const char *);
void		f_user_pre_parse_setup(void);

static void	dt_release(void *);
static void	log_callback(char const *, int, unsigned, char const *);

void		f_user_cmvlc_init(struct Crate *);
void		f_user_cmvlc_deinit(struct Crate *);
uint32_t	f_user_cmvlc_fetch(struct Crate *, struct EventBuffer *);

int		f_user_prepare_cmvlc(unsigned char,
    struct cmvlc_stackcmdbuf *);
int		f_user_format_event(unsigned char, unsigned char,
    const long *, long, long *, long *, void *, long *);

void		f_user_local_init(struct Crate *);
void		f_user_local_deinit(struct Crate *);

/* Path to config file. */
static char g_cfg_path[256] = CONFIG_NAME_PRIMARY;

/* Nurdlib crate context. */
static struct Crate *g_crate;

/*
 * This example couples each TRIVA trigger to a readout tag, such that eg we
 * can read one set of modules on trigger 1 and another set of modules on
 * trigger 2. Note that the modules in different tags can overlap, nurdlib
 * will track each module's counter!
 */
static struct CrateTag *g_tag[16];

/* Size of event memory for each trigger as configured by MBS/drasi. */
static uint32_t g_ev_bytes[16];

/*
 * Data for the deadtime releasing function. Could just be global primitives,
 * but let's use some fancy tricks.
 */
struct DT {
	unsigned	char trig_typ;
	long	*read_stat;
};
static struct DT g_dt;

/*
 * This callback should release the deadtime in the DAQ backend.
 */
void
dt_release(void *a_data)
{
	struct DT *dt;

	dt = a_data;
	f_user_trig_clear(dt->trig_typ);
	*dt->read_stat = TRIG__CLEARED;
}

#if NCONF_mMAP_bCMVLC
/*
 */
void f_user_cmvlc_init(struct Crate *a_crate)
{
	struct cmvlc_stackcmdbuf stack_object;
	struct cmvlc_stackcmdbuf *stack;

	/* We'd get it as pointer, so do that. */
	stack = &stack_object;

	/* Clear library state of stacks. */
	cmvlc_reset_stacks(g_cmvlc);

	cmvlc_stackcmd_init(stack);
	cmvlc_stackcmd_start(stack, stack_out_pipe_data);

	cmvlc_stackcmd_marker(stack, 0x12345678);

	crate_cmvlc_init(a_crate, stack, 1);
	crate_cmvlc_init(a_crate, stack, 0);

	cmvlc_stackcmd_marker(stack, 0x3456789a);

	cmvlc_stackcmd_end(stack);

	/* The readout stack is for free-running.
	 * Cheat by triggering it by a periodic timer.
	 * 1 kHz - most the MVLC internal timers can do.
	 */
	cmvlc_setup_stack(g_cmvlc, stack, 4, 0x0040 | 20); /* trig by timer */

	/* Timer 0 period. */
	cmvlc_mvlc_write(g_cmvlc, 0x1180, 1);

	/* Tell MVLC where to send readout data. */
	if (cmvlc_readout_attach(g_cmvlc) < 0)
	  log_die(LOGL, "Failed to attach MVLC data output.");

	{
	  /* Setup pointers to stacks, their triggers. */
	  if (cmvlc_set_daq_mode(g_cmvlc, 0, 1, NULL, 0, 0) < 0)
	    log_die(LOGL, "Failed to setup MVLC stacks and IRQ stack map.");
	}

	/* Enable DAQ mode. */
	if (cmvlc_set_daq_mode(g_cmvlc, 1, 0, NULL, 0, 0) < 0)
		log_die(LOGL, "Failed to set MVLC DAQ mode.");

	/* Reset the internal data buffer handling (UDP packet reader). */
	/* Is this a good location? */
	cmvlc_readout_reset(g_cmvlc);
}

void f_user_cmvlc_deinit(struct Crate *a_crate)
{
	(void) a_crate;

	/* Disable DAQ mode. */
	if (cmvlc_set_daq_mode(g_cmvlc, 0, 0, NULL, 0, 0) < 0)
		log_die(LOGL, "Failed to disable MVLC DAQ mode.");
}

/*
 */
uint32_t f_user_cmvlc_fetch(struct Crate *a_crate,
    struct EventBuffer *a_event_buffer)
{
	uint32_t result;

	int ret;
	uint32_t dest[0x10000];
	size_t   event_len = 0;
	struct cmvlc_event_info info;

	const uint32_t *input_u32 = (const uint32_t *) dest;

	uint32_t remain;
	uint32_t used;

	LOGF(spam)(LOGL, " f_user_cmvlc_fetch {");

        result = 0;

	ret = cmvlc_readout_get_event(g_cmvlc, dest,
				      sizeof (dest) / sizeof (dest[0]),
				      &event_len, &info);

	if (ret < 0)
	  {
	    log_error(LOGL, "Failed to get cmvlc event: "
		      "%d - %s", ret, cmvlc_last_error(g_cmvlc));
	    result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
	    goto done;
	  }

	if (event_len < 5 ||
	    info._errors ||
	    info._stacknum != 4 ||
	    info._ctrlid != 0)
	  {
	    log_error(LOGL, "Bad cmvlc event: "
		      "len: %d flags: %d stack: %d ctrlid: %d.",
		      (int) event_len,
		      info._errors,
		      info._stacknum,
		      info._ctrlid);
	    result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
	    goto done;
	  }

	/* Check that the markers in the data are as expected. */
	/* dest[1] and dest[2] hold the low and high words of the
	 * event counter.  (Not useful for free-running.)
	 */
	if (dest[0]                != 0x12345678 ||
	    dest[event_len-1]      != 0x3456789a) /* 8 or 2 */
	  {
	    log_error(LOGL, "Malformed cmvlc event.");
	    /* Since we are *not* writing the buffer we got from
	     * cmvlc to the output buffer, we dump it.
	     */
	    log_dump(LOGL, dest, event_len * sizeof (uint32_t));
	    result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
	    goto done;
	  }

	input_u32 = dest + 1; /* Start marker. */
	remain = event_len - 1;

	result |= crate_cmvlc_fetch_dt(a_crate,
				       input_u32, remain, &used);

	if (result != 0)
	  {
	    log_error(LOGL, "Cmvlc crate_cmvlc_fetch_dt failed.");
	    goto done;
	  }

	input_u32 += used;
	remain -= used;

	result |= crate_cmvlc_fetch(a_crate,
				    a_event_buffer,
				    input_u32, remain, &used);

	if (result != 0)
	  {
	    log_error(LOGL, "Cmvlc crate_cmvlc_fetch failed.");
	    goto done;
	  }

	input_u32 += used;
	remain -= used;

	/*
	if (used > 0x8000)
	  printf ("crate_cmvlc_fetch: used = %d\n", used);
	*/

	if (remain != 1)
	  {
	    log_error(LOGL, "Cmvlc block "
		      "did not exhaust buffer except end marker.");
	    /* log_dump(LOGL, dest, event_len * sizeof (uint32_t)); */
	    result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
	    goto done;
	  }

done:
	LOGF(spam)(LOGL, " f_user_cmvlc_fetch(0x%08x) }", result);
	return result;
}

int f_user_prepare_cmvlc(unsigned char readout_no,
			 struct cmvlc_stackcmdbuf *stack)
{
	int _early_dt_release = 0;

	printf ("Prepare MVLC sequencer readout #%d.\n", readout_no);

	/* Read all event counters before reading the data. */
	crate_cmvlc_init(g_crate, stack, 1);

	if (_early_dt_release &&
	    readout_no != 15)
	  fud_setup_cmvlc_readout_release_dt(readout_no, stack);

	cmvlc_stackcmd_marker(stack, 0x87654321);

	/* Data readout. */
	crate_cmvlc_init(g_crate, stack, 0);

	if (!_early_dt_release &&
	    readout_no != 15)
	  fud_setup_cmvlc_readout_release_dt(readout_no, stack);

	return 0;
}

int f_user_format_event(unsigned char trig, unsigned char crate_number,
			const long *input, long input_len, long *input_used,
			long *buf,
			void *subevent, long *bytes_read)
{
	struct EventBuffer event_buffer_orig;
	struct EventBuffer event_buffer;

	const uint32_t *input_u32 = (const uint32_t *) input;
	static int _master_starts = 0;

	uint32_t result;

	uint32_t remain, used;

	(void)trig;
	(void)crate_number;
	(void)subevent;

	(void)input_u32;

	if (trig == 1)
		_master_starts++;

	/* Setup event-buffer struct for nurdlib's memory handling. */
	event_buffer.bytes = g_ev_bytes[trig];
	event_buffer.ptr = buf;

	COPY(event_buffer_orig, event_buffer);

	remain = input_len / sizeof (uint32_t);

        result = 0;

	used = 0;

	/* log_dump(LOGL, input, input_len); */

	/* printf ("rem: %d\n", remain); */

	result |= crate_cmvlc_fetch_dt(g_crate,
				       input_u32, remain, &used);

	/* printf ("rem: %d used: %d\n", remain, used); */

	if (result != 0)
	  {
	    log_error(LOGL, "Cmvlc crate_cmvlc_fetch_dt failed.");
	    goto done;
	  }

	input_u32 += used;
	input_len -= used * sizeof (uint32_t);
	*input_used += used * sizeof (uint32_t);

	if (input_len < (long) sizeof(uint32_t))
	  {
	    log_error(LOGL, "Missing separator in cmvlc event: len=%ld.",
                      input_len);
            result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
            goto done;
	  }

	if (input_u32[0] != 0x87654321)
	  {
	    log_error(LOGL, "Malformed cmvlc event.");
            result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
            goto done;
	  }

	input_u32 += 1;
	input_len -= sizeof (uint32_t);
	*input_used += sizeof (uint32_t);

	result |= crate_cmvlc_fetch(g_crate, &event_buffer,
				    input_u32, remain, &used);

	/* printf ("rem: %d used: %d\n", remain, used); */

	if (result != 0)
	  {
	    log_error(LOGL, "Cmvlc crate_cmvlc_fetch failed.");
	    goto done;
	  }

	input_u32 += used;
	input_len -= used * sizeof (uint32_t);
	*input_used += used * sizeof (uint32_t);

	if (input_len != 0)
	  {
	    log_error(LOGL, "Cmvlc block "
		      "did not exhaust buffer.");
	    /* log_dump(LOGL, dest, event_len * sizeof (uint32_t)); */
	    result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
	    goto done;
	  }

	EVENT_BUFFER_INVARIANT(event_buffer, event_buffer_orig);

	*bytes_read = (uintptr_t)event_buffer.ptr - (uintptr_t)buf;

	/*
	printf ("il %ld iu %ld br %ld\n", input_len, *input_used, *bytes_read);
	*/
	goto done;
done:
	return result;
}
#endif

void f_user_local_init(struct Crate *a_crate)
{
#if NCONF_mMAP_bCMVLC
	if (crate_free_running_get(a_crate))
		f_user_cmvlc_init(a_crate);
#else
	(void) a_crate;
#endif
}

void f_user_local_deinit(struct Crate *a_crate)
{
#if NCONF_mMAP_bCMVLC
	if (crate_free_running_get(a_crate))
		f_user_cmvlc_deinit(a_crate);
#else
	(void) a_crate;
#endif
}

/*
 * MBS uses two initialization functions for historical reasons, nowadays a
 * single one is sufficient so we leave this one empty.
 */
int
f_user_get_virt_ptr(long *pl_loc_hwacc, long *pl_rem_cam)
{
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
#if FUSER_DRASI
	/*
	 * untriggered_loop will only be called if no trigger module has been
	 * set for drasi (--triva/trimi).
	 */
	_lwroc_readout_functions.untriggered_loop = untriggered_loop;
#if NCONF_mMAP_bCMVLC
	{
	/* See comments in f_user_cmvlc_mdpp.c. */
	unsigned char readout_for_trig[16] =
	/*   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 */
	  {  0,  8,  0,  9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };

	fud_setup_cmvlc_readout(readout_for_trig,
				f_user_prepare_cmvlc,
				f_user_format_event);
	}
#endif
#endif
	return 0;
}

#if FUSER_DRASI
/*
 * Handle command line arguments.
 */
void local_cmdline_usage(void)
{
	printf ("  --cfg=FILENAME           Nurdlib configuration "
	    "(default %s).\n", CONFIG_NAME_PRIMARY);
	printf ("\n");
}

int local_parse_cmdline_arg(const char *request)
{
	const char *post;

	if (LWROC_MATCH_C_PREFIX("--cfg=", post)) {
		strncpy(g_cfg_path, post, sizeof g_cfg_path);
		g_cfg_path[sizeof g_cfg_path - 1] = 0;
		return 1;
	}

	return 0;
}

/* Overrides default (attribute weak) do-nothing function. */
void f_user_pre_parse_setup(void)
{
	_lwroc_fud_cmdline_fcns.usage = local_cmdline_usage;
	_lwroc_fud_cmdline_fcns.parse_arg = local_parse_cmdline_arg;
}
#endif

/*
 * Let's initialize!
 */
int
f_user_init(unsigned char bh_crate_nr, long *pl_loc_hwacc, long *pl_rem_cam,
    long *pl_stat)
{
	static int is_setup = 0;
	unsigned i;

	(void)bh_crate_nr;
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
	(void)pl_stat;

	if (is_setup) {
#if NCONF_mMAP_bCMVLC
		LOGF(info)(LOGL, "Re-initializing crate for MVLC.");
		crate_deinit(g_crate);
		/* time_sleep(g_crate->reinit_sleep_s); */
		crate_init(g_crate);
#endif
		return 0;
	}
	is_setup = 1;

	/* Setup DAQ backend logging first of all. */
	log_callback_set(log_callback);

	/* Figure out the max event size. */
	{
#if FUSER_DRASI
		g_ev_bytes[0] = fud_get_max_event_length();
		LOGF(info)(LOGL, "Event size per trigger=0x%u B.",
		    g_ev_bytes[0]);
		for (i = 1; i < LENGTH(g_ev_bytes); ++i) {
			g_ev_bytes[i] = g_ev_bytes[0];
		}
		lwroc_init_timestamp_track();
#elif FUSER_MBS
		struct stat st;
		s_setup *setup;

		f_ut_get_setup_seg(SM_READ | SM_WRITE, &setup);
		for (i = 0; i < LENGTH(g_ev_bytes); ++i) {
			g_ev_bytes[i] = setup->bl_max_se_len[bh_crate_nr][i];
		}
		/*
		 * See if the default config file is available, otherwise try
		 * "../<procid>_<control>/<CONFIG_NAME_PRIMARY>".
		 * Other than building paths into the binary, this is the only
		 * way to identify nodes between each other in MBS.
		 */
		if (0 != stat(CONFIG_NAME_PRIMARY, &st)) {
			snprintf_(g_cfg_path, sizeof g_cfg_path, "../%u_%u/%s",
			    setup->i_se_procid[0], setup->h_se_control,
			    CONFIG_NAME_PRIMARY);
		}
#endif
	}

	/*
	 * An optional file 'nurdlib_def_path.txt' to set the default config
	 * path.
	 */
	{
		FILE *file;

		file = fopen("nurdlib_def_path.txt", "rb");
		if (file) {
			char path[256];

			if (fgets(path, sizeof path, file)) {
				char *p;

				p = strchr(path, '\n');
				if (NULL != p) {
					*p = '\0';
				}
				config_default_path_set(path);
			}
			fclose(file);
		}
	}

#if FUSER_DRASI
	if (!thread_mutex_init(&g_thread_no_mutex)) {
		log_die(LOGL, "thread_mutex_init failed.");
	}
	g_thread_no = 0;
	thread_start_callback_set(thread_callback, NULL);
#endif

	/* Provide DAQ backend logging (again) and load config file. */
	/* The local init/deinit callbacks need to be set in
	 * nurdlib_setup() such that f_user_local_init (and
	 * f_user_cmvlc_init) init is called also for the first
	 * crate_init() call inside nurdlib_setup().
	 */
	g_crate = nurdlib_setup(log_callback, g_cfg_path,
	    f_user_local_init, f_user_local_deinit);

	/*
	 * Get the "Default" tag and tags "1", "2" etc, one for each TRIVA
	 * trigger number.
	 */
	g_tag[0] = crate_get_tag_by_name(g_crate, "Default");
	for (i = 1; i < LENGTH(g_tag); ++i) {
		char name[10];

		snprintf_(name, sizeof name, "%u", i);
		g_tag[i] = crate_get_tag_by_name(g_crate, name);
	}
	if (NULL == g_tag[1]) {
		g_tag[1] = g_tag[0];
	}

	/*
	 * Set deadtime releasing function.
	 * This is useful with 'release_dt=true' inside CRATE in the config
	 * and if all configured modules support it.
	 */
	crate_dt_release_set_func(g_crate, dt_release, &g_dt);

	return 0;
}

int
f_user_readout(unsigned char bh_trig_typ, unsigned char bh_crate_nr, register
    long *pl_loc_hwacc, register long *pl_rem_cam, long *pl_dat, s_veshe
    *ps_veshe, long *l_se_read_len, long *l_read_stat)
{
	struct EventBuffer event_buffer_orig;
	struct EventBuffer event_buffer;
	uint32_t *p32, *header;

	(void)bh_crate_nr;
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
	(void)ps_veshe;

	*l_read_stat = 0;

	g_dt.trig_typ = bh_trig_typ;
	g_dt.read_stat = l_read_stat;

	/* Setup event-buffer struct for nurdlib's memory handling. */
	event_buffer.bytes = g_ev_bytes[bh_trig_typ];
	event_buffer.ptr = pl_dat;
	/*
	 * We keep a copy of the original event-buffer to make sure what we
	 * get at the end is consistent. Nurdlib does this consistency
	 * checking too, but checking is very important so let's show how to
	 * do it.
	 */
	COPY(event_buffer_orig, event_buffer);

	/* Some electronics check the TRIVA trigger number, provide it. */
	crate_gsi_mbs_trigger_set(g_crate, bh_trig_typ);

	/*
	 * Increment tag counters. Nurdlib only cares about these requests for
	 * non-referenced counters, e.g. a VULOM scaler overrides these
	 * requests which is especially useful for multi-event DAQ:s.
	 */
	if (g_tag[bh_trig_typ]) {
		crate_tag_counter_increase(g_crate, g_tag[bh_trig_typ], 1);
	}

	/* Reserve a 32-bit header for the readout status. */
	header = (void *)pl_dat;
	p32 = header + 1;

	/* 0 = good, everything else bad. */
	*header = 0;

	/* This help keep the event-buffer consistent while building it. */
	EVENT_BUFFER_ADVANCE(event_buffer, p32);

#if NCONF_mMAP_bCMVLC
	/*
	 * Fetch data from MVLC sequencer output.
	 */
	*header |= f_user_cmvlc_fetch(g_crate, &event_buffer);
	if (0 != *header) {
		log_error(LOGL, "cmvlc_fetch failed.");
		goto f_user_readout_crate_done;
	}
#else
	/*
	 * Readout that must happen during deadtime, eg data-sizes and event
	 * counters.
	 */
	*header |= crate_readout_dt(g_crate);
	if (0 != *header) {
		log_error(LOGL, "readout_dt failed.");
		goto f_user_readout_crate_done;
	}

	/* Readout of bulk data, potentially out of deadtime. */
	*header |= crate_readout(g_crate, &event_buffer);
	if (0 != *header) {
		log_error(LOGL, "readout failed.");
		goto f_user_readout_crate_done;
	}
#endif

f_user_readout_crate_done:

	/* Check and clean up after the readout. */
	crate_readout_finalize(g_crate);

	/*
	 * Check that the event-buffer looks consistent. Buffer
	 * under-/over-runs are no joke...
	 */
	EVENT_BUFFER_INVARIANT(event_buffer, event_buffer_orig);

	if (0 != *header) {
		log_error(LOGL, "Had readout error: ret=0x%x, trg=%u",
		    *header, bh_trig_typ);
	}

	*l_se_read_len = (uintptr_t)event_buffer.ptr - (uintptr_t)pl_dat;

	return 0;
}

#if FUSER_DRASI
/* Only called by drasi for free-running systems without a trigger module. */
void
untriggered_loop(int *start_no_stop)
{
	uint64_t cycle;

	*start_no_stop = 0;
	g_lmd_stream = lwroc_get_lmd_stream("READOUT_PIPE");
	f_user_init(0, NULL, NULL, NULL);
	crate_free_running_set(g_crate, 1);
	/* Not really a TRIVA status, but a status... */
	_lwroc_mon_main_system_handle->_block._aux_status =
	    LWROC_TRIVA_STATUS_RUN;
	for (cycle = 1; !_lwroc_main_thread->_terminate; cycle++) {
		struct lwroc_lmd_subevent_info info;
		lmd_event_10_1_host *event;
		lmd_subevent_10_1_host *sev;
		void *buf;
		void *end;
		size_t event_size;
		long bytes_read = 0;

		event_size = sizeof (lmd_subevent_10_1_host) + g_ev_bytes[0];
		lwroc_reserve_event_buffer(g_lmd_stream, (uint32_t)cycle,
		    event_size, 0, 0);
		lwroc_new_event(g_lmd_stream, &event, 1);
		info.type = 10;
		info.subtype = 1;
		info.procid = 13;
		info.control = 1;
		info.subcrate = 0;
		buf = lwroc_new_subevent(g_lmd_stream, LWROC_LMD_SEV_NORMAL,
		    &sev, &info);

		while (!_lwroc_main_thread->_terminate) {
			long read_status;

			bytes_read = 0;
			read_status = 0;
			f_user_readout(1, 0, NULL, NULL, buf, (void *)&sev,
			    &bytes_read, &read_status);
			if (sizeof(uint32_t) < (size_t)bytes_read) {
				/* More than just custom header written. */
				break;
			}
			/*
			 * Use the monitor timer to force generation of
			 * an event even if empty.  Currently 10 Hz.
			 */
			if (LWROC_MON_UPDATE_PENDING(_lwroc_mon_main_handle))
			{
				break;
			}
			sched_yield();
		}

		end = (uint8_t *)buf + bytes_read;
		lwroc_finalise_subevent(g_lmd_stream, LWROC_LMD_SEV_NORMAL,
		    end);
		lwroc_finalise_event_buffer(g_lmd_stream);

		LWROC_MON_CHECK_COPY_BLOCK(_lwroc_mon_main_handle,
		    &_lwroc_mon_main, 0);
		LWROC_MON_CHECK_COPY_CONN_MON_BLOCK(
		    _lwroc_mon_main_system_handle, 0);
	}
	/* Not really a TRIVA status, but a status... */
	_lwroc_mon_main_system_handle->_block._aux_status =
	    LWROC_TRIVA_STATUS_STOP;
}
#endif

void
log_callback(char const *a_file, int a_line_no, unsigned a_level, char const
    *a_str)
{
#if FUSER_DRASI
	int lwroc_log_lvl;

	switch (a_level) {
#define LOG_LEVEL(FROM, TO) \
	case KW_##FROM: lwroc_log_lvl = LWROC_MSGLVL_##TO;  break;
	LOG_LEVEL(INFO, INFO);
	LOG_LEVEL(VERBOSE, LOG);
	LOG_LEVEL(DEBUG, DEBUG);
	LOG_LEVEL(SPAM, SPAM);
	LOG_LEVEL(ERROR, ERROR);
	default:
		lwroc_log_lvl = LWROC_MSGLVL_BUG;
		break;
	}
	lwroc_message_internal(lwroc_log_lvl, NULL, a_file, a_line_no, "%s",
	    a_str);
#elif FUSER_MBS
	struct tm tm;
	char str_t[32];
	time_t t_now;
	char const *str_l;

	time(&t_now);
	gmtime_r(&t_now, &tm);
	strftime(str_t, sizeof str_t, "%Y-%m-%d,%H:%M:%S", &tm);
	switch (a_level) {
#define LOG_LEVEL(FROM, TO) \
	case KW_##FROM: str_l = #TO; break;
	LOG_LEVEL(INFO, INFO);
	LOG_LEVEL(VERBOSE, VRBS);
	LOG_LEVEL(DEBUG, DEBG);
	LOG_LEVEL(SPAM, SPAM);
	LOG_LEVEL(ERROR, ERRR);
	default:
		str_l = "????";
		break;
	}
	printm("%s:%s: %s (%s:%d)", str_t, str_l, a_str, a_file, a_line_no);
#endif
}
