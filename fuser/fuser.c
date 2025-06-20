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

static void	dt_release(void *);
static void	log_callback(char const *, int, unsigned, char const *);

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
#endif
	return 0;
}

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
	g_crate = nurdlib_setup(log_callback, g_cfg_path);

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
