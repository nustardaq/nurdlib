/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2025
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

#include <nurdlib/crate.h>
#include <ctype.h>
#include <crate/internal.h>
#include <string.h>
#include <ctrl/ctrl.h>
#include <module/module.h>
#include <module/map/map.h>
#include <module/caen_v1n90/internal.h>
#include <module/caen_v820/internal.h>
#include <module/caen_v830/internal.h>
#include <module/gsi_ctdc/internal.h>
#include <module/gsi_etherbone/internal.h>
#include <module/gsi_febex/internal.h>
#include <module/gsi_kilom/internal.h>
#include <module/gsi_mppc_rob/internal.h>
#include <module/gsi_pex/internal.h>
#include <module/gsi_pexaria/internal.h>
#include <module/gsi_sam/internal.h>
#include <module/gsi_siderem/internal.h>
#include <module/gsi_tacquila/internal.h>
#include <module/gsi_tamex/internal.h>
#include <module/gsi_triva/internal.h>
#include <module/gsi_vetar/internal.h>
#include <module/pnpi_cros3/internal.h>
#include <module/trloii/internal.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <nurdlib/trloii.h>
#include <util/assert.h>
#include <util/bits.h>
#include <util/fmtmod.h>
#include <util/math.h>
#include <util/memcpy.h>
#include <util/pack.h>
#include <util/stdint.h>
#include <util/string.h>
#include <util/thread.h>
#include <util/time.h>
#include <util/vector.h>
#include <module/map/map_cmvlc.h>

#define BARRIER_WORD 0xbabababa
#define DT_TIMEOUT 1.0

/*
 * Basic lock contention profiling.
 */
#if 1
#	define THREAD_MUTEX_LOCK(var) thread_mutex_lock(var)
#else
#	define THREAD_MUTEX_LOCK(var) do {\
	static int s_tml_i_ = -1;\
	double t_, dt_;\
	if (-1 == s_tml_i_) {\
		strcpy(g_tml_reg_[s_tml_i_ = g_tml_reg_i_].name,\
		    __FUNCTION__);\
		strcat(g_tml_reg_[s_tml_i_ = g_tml_reg_i_].name, ":");\
		strcat(g_tml_reg_[s_tml_i_ = g_tml_reg_i_].name, #var);\
		g_tml_reg_[s_tml_i_ = g_tml_reg_i_].line_no = __LINE__;\
		++g_tml_reg_i_;\
	}\
	t_ = time_getd();\
	thread_mutex_lock(var);\
	dt_ = time_getd() - t_;\
	g_tml_reg_[s_tml_i_].t_max = MAX(g_tml_reg_[s_tml_i_].t_max, dt_);\
	g_tml_reg_[s_tml_i_].t += dt_;\
	++g_tml_reg_[s_tml_i_].n;\
	if (10000 == ++g_tml_c_) {\
		size_t i_;\
		printf("Mutex contention output (us):\n");\
		for (i_ = 0; i_ < g_tml_reg_i_; ++i_) {\
			printf(" acc=%10.2f  cnt=%10u  avg=%10.2f  max=%10.2f  (%s:%u)\n",\
			    g_tml_reg_[i_].t * 1e6,\
			    g_tml_reg_[i_].n,\
			    g_tml_reg_[i_].t / g_tml_reg_[i_].n * 1e6,\
			    g_tml_reg_[i_].t_max * 1e6,\
			    g_tml_reg_[i_].name,\
			    g_tml_reg_[i_].line_no);\
			g_tml_reg_[s_tml_i_].t_max = 0;\
		}\
		g_tml_c_ = 0;\
	}\
} while (0)
static size_t g_tml_reg_i_;
static struct {
	char	name[100];
	int	line_no;
	double	t_max;
	double	t;
	uint32_t	n;
} g_tml_reg_[10];
static uint32_t g_tml_c_;
#endif

enum CrateState {
	STATE_REINIT,   /* Uninited state. */
	STATE_PREPARED, /* Prepared, not ready for readout. */
	STATE_READY     /* Ready for readout. */
};

VECTOR_HEAD(ModuleRefVector, struct Module *);
TAILQ_HEAD(CrateScalerList, CrateScaler);
struct CrateScaler {
	char	name[32];
	struct	Module *module;
	void	*data;
	uint32_t	(*get_counter)(struct Module *, void *, struct Counter
	    *);
	TAILQ_ENTRY(CrateScaler)	next;
};
TAILQ_HEAD(CrateCounterList, CrateCounter);
VECTOR_HEAD(CounterRefVector, struct CrateCounter *);
struct CrateCounter {
	char	name[32];
	struct	CrateScaler *scaler;
	struct	Counter cur;
	uint32_t	prev;
	TAILQ_ENTRY(CrateCounter)	next;
};
TAILQ_HEAD(CrateTagList, CrateTag);
VECTOR_HEAD(TagRefVector, struct CrateTag *);
struct CrateTag {
	char	name[32];
	struct	ModuleRefVector module_ref_vec;
	struct	CounterRefVector counter_ref_vec;
	unsigned	module_num;
	unsigned	event_max;
	int	gsi_pex_is_needed;
	int	do_pedestals;
	TAILQ_ENTRY(CrateTag)	next;
};
/*
 * Temporary storage while init:ing modules, eg CAEN v7nn modules with a PAUX
 * connector will force GEO and we have to make sure that doesn't introduce
 * collisions with other modules!
 */
TAILQ_HEAD(ModuleIDList, ModuleID);
struct ModuleID {
	unsigned	id;
	TAILQ_ENTRY(ModuleID)	next;
};
TAILQ_HEAD(CrateList, Crate);
struct Crate {
	char	*name;
	enum	CrateState state;
	unsigned	event_max_override;
	struct {
		int	do_it;
		int	do_inhibit;
		int	is_on;
		void	(*func)(void *);
		void	*data;
		unsigned	for_it;
		unsigned	for_it_prev;
	} dt_release;
	struct	GsiCTDCCrate gsi_ctdc_crate;
	struct	GsiFebexCrate gsi_febex_crate;
	struct	GsiKilomCrate gsi_kilom_crate;
	struct	GsiMppcRobCrate gsi_mppc_rob_crate;
	struct	GsiSamCrate gsi_sam_crate;
	struct	GsiSideremCrate gsi_siderem_crate;
	struct	GsiTacquilaCrate gsi_tacquila_crate;
	struct	GsiTamexCrate gsi_tamex_crate;
	struct	PnpiCros3Crate pnpi_cros3_crate;
	struct {
		struct	ConfigBlock *config;
		struct	GsiPex *pex;
	} gsi_pex;
	struct	ModuleList module_list;
	struct	CrateTagList tag_list;
	struct	CrateCounterList counter_list;
	struct	CrateScalerList scaler_list;
	size_t	module_num;
	struct	ModuleRefVector module_configed_vec;
	struct {
		int	yes;
		unsigned	pioneer_counter;
		unsigned	ns;
		int	do_grow;
		struct	Module *module;
		void	(*set_cvt)(struct Module *, unsigned);
	} acvt;
	struct {
		size_t	buf_bytes;
		struct	MapBltDst *dst;
		int	is_running;
		struct	Thread thread;
		size_t	max_bytes;
		size_t	module_readable_num;
		int	do_buf_rebuild;
	} shadow;
	struct {
		struct	Module *module;
		char	const *tag_name;
		struct	CrateTag *tag;
	} trloii_multi_event;
	double	reinit_sleep_s;
	double	postinit_sleep_s;
	int	is_free_running;
	unsigned	module_init_id;
	struct	ModuleIDList module_init_id_list;
	struct	Mutex mutex;
	unsigned	gsi_mbs_trigger;
	struct {
		size_t	num;
		int	array[10];
	} sync;
	InitCallback	init_callback;
	InitCallback	deinit_callback;
	TAILQ_ENTRY(Crate)	next;
};

static uint32_t			check_empty(struct Crate *) FUNC_RETURNS;
static void			dt_release(struct Crate *);
static struct CrateCounter	*get_counter(struct Crate *, char const *)
	FUNC_RETURNS;
static struct Crate		*get_crate(unsigned) FUNC_RETURNS;
static struct Module		*get_module(struct Crate *, unsigned)
	FUNC_RETURNS;
static uint32_t			shadow_merge_module(struct Crate *, struct
    Module *, struct EventBuffer *) FUNC_RETURNS;
static void			module_counter_latch(struct Module *);
static void			module_init_id_clear(struct Crate *);
static void			module_init_id_mark(struct Crate *, struct
    Module const *);
static void			module_insert(struct Crate *, struct
    TagRefVector *, struct Module *);
static void			pop_log_level(struct Module const *);
static void			push_log_level(struct Module const *);
static uint32_t			read_module(struct Crate *, struct Module *,
    struct EventBuffer *) FUNC_RETURNS;
static void			shadow_func(void *);
static int			signature_match(struct Module const *, struct
    Module const *) FUNC_RETURNS;
static struct CrateTag		*tag_get(struct Crate *, char const *)
	FUNC_RETURNS;

static struct CrateList g_crate_list = TAILQ_HEAD_INITIALIZER(g_crate_list);

unsigned
crate_acvt_get_ns(struct Crate const *a_crate)
{
	return a_crate->acvt.ns;
}

void
crate_acvt_grow(struct Crate *a_crate)
{
	a_crate->acvt.do_grow = 1;
}

int
crate_acvt_has(struct Crate const *a_crate)
{
	return a_crate->acvt.yes;
}

void
crate_acvt_set(struct Crate *a_crate, struct Module *a_module, CvtSetCallback
    a_callback)
{
	a_crate->acvt.module = a_module;
	a_crate->acvt.set_cvt = a_callback;
}

int
crate_config_write(int a_crate_i, int a_module_j, int a_submodule_k, struct
    Packer *a_packer)
{
	struct Crate *crate;
	struct Module *module;
	struct ConfigBlock *config_block;
	struct Module **module_ref;
	int ret;

	LOGF(debug)(LOGL, "crate_config_write(cr=%d,mod=%d,sub=%d) {",
	    a_crate_i, a_module_j, a_submodule_k);
	ret = 0;

	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		LOGF(debug)(LOGL, "No such crate %d.", a_crate_i);
		goto crate_config_write_done;
	}
	module = get_module(crate, a_module_j);
	if (NULL == module) {
		LOGF(debug)(LOGL, "No such module %d.", a_module_j);
		goto crate_config_write_done;
	}
	if (-1 == a_submodule_k) {
		config_block = module->config;
	} else {
		if (NULL == module->props) {
			LOGF(debug)(LOGL, "Module %d has no props.",
			    a_module_j);
			goto crate_config_write_done;
		}
		if (NULL == module->props->get_submodule_config) {
			LOGF(debug)(LOGL, "Module %d has no submodule conf.",
			    a_module_j);
			goto crate_config_write_done;
		}
		config_block = module->props->get_submodule_config(module,
		    a_submodule_k);
		if (NULL == config_block) {
			LOGF(debug)(LOGL,
			    "Module %d has no submodule config block %d.",
			    a_module_j, a_submodule_k);
			goto crate_config_write_done;
		}
	}
	if (!config_merge(config_block, a_packer)) {
		LOGF(debug)(LOGL, "Config merge failed.");
		goto crate_config_write_done;
	}
	/* If the module has no pending reconfig, make one. */
	VECTOR_FOREACH(module_ref, &crate->module_configed_vec) {
		if (*module_ref == module) {
			module = NULL;
			break;
		}
	}
	if (NULL != module) {
		VECTOR_APPEND(&crate->module_configed_vec, module);
	}
	ret = 1;

crate_config_write_done:
	LOGF(debug)(LOGL, "crate_config_write }");
	return ret;
}

struct CrateCounter *
crate_counter_get(struct Crate *a_crate, char const *a_name)
{
	struct CrateCounter *counter;

	TAILQ_FOREACH(counter, &a_crate->counter_list, next) {
		if (0 == strcmp(counter->name, a_name)) {
			return counter;
		}
	}
	return NULL;
}

uint32_t
crate_counter_get_diff(struct CrateCounter const *a_counter)
{
	return COUNTER_DIFF_RAW(a_counter->cur, a_counter->prev);
}

struct Crate *
crate_create(void)
{
	struct TagRefVector tag_active_vec;
	struct Crate *crate;
	struct CrateTag *tag;
	struct CrateCounter *counter;
	struct ConfigBlock *crate_block;
	struct ConfigBlock *module_block;
	unsigned module_id;
	int crate_needs_pex = 0;

	LOGF(info)(LOGL, "crate_create {");

	CALLOC(crate, 1);
	TAILQ_INIT(&crate->module_list);
	TAILQ_INIT(&crate->tag_list);
	TAILQ_INIT(&crate->counter_list);
	TAILQ_INIT(&crate->scaler_list);
	TAILQ_INIT(&crate->module_init_id_list);

	/* Get the crate config. */
	crate_block = config_get_block(NULL, KW_CRATE);
	if (NULL == crate_block) {
		log_die(LOGL, "crate_create: Could not find a configured "
		    "crate, I bail.");
	}
	crate->name = strdup_(config_get_block_param_string(crate_block, 0));
	LOGF(info)(LOGL, "Crate=\"%s\".", crate->name);

#define FLAG_LOG(flag, msg) do { \
	if (flag) { \
		LOGF(info)(LOGL, #msg " enabled."); \
	} else { \
		LOGF(verbose)(LOGL, #msg " disabled."); \
	} \
} while (0)

	crate->acvt.yes = config_get_boolean(crate_block, KW_ACVT);
	crate->acvt.pioneer_counter = 0;
	crate->acvt.ns = 0;
	FLAG_LOG(crate->acvt.yes, "Adaptive CVT");

	crate->shadow.buf_bytes = config_get_int32(crate_block,
	    KW_SHADOW_BYTES, CONFIG_UNIT_B, 0, 1 << 30);
	if (0 != crate->shadow.buf_bytes) {
		LOGF(info)(LOGL, "Shadow readout enabled, buffer "
		    "size=0x%"PRIzx" B.", crate->shadow.buf_bytes);
		crate->shadow.dst = map_blt_dst_alloc(crate->shadow.buf_bytes);
	} else {
		LOGF(verbose)(LOGL, "Shadow readout disabled.");
	}

	crate->dt_release.do_it = config_get_boolean(crate_block,
	    KW_DEADTIME_RELEASE);
	crate->dt_release.for_it_prev = -1;
	FLAG_LOG(crate->dt_release.do_it, "Early deadtime release");

	crate->event_max_override = config_get_int32(crate_block,
	    KW_EVENT_MAX_OVERRIDE, CONFIG_UNIT_NONE, 0, 200000);
	LOGF(verbose)(LOGL, "event_max override=%u.",
	    crate->event_max_override);

	crate->reinit_sleep_s = config_get_int32(crate_block, KW_REINIT_SLEEP,
	    CONFIG_UNIT_S, 0, 60);
	LOGF(verbose)(LOGL, "Re-init sleep=%gs.", crate->reinit_sleep_s);
	crate->postinit_sleep_s = config_get_int32(crate_block,
	    KW_POSTINIT_SLEEP, CONFIG_UNIT_S, 0, 60);
	LOGF(verbose)(LOGL, "Post-init sleep=%gs.", crate->postinit_sleep_s);

	crate->is_free_running = config_get_boolean(crate_block,
	    KW_FREE_RUNNING);
	FLAG_LOG(crate->is_free_running, "Free-running");

	gsi_sam_crate_create(&crate->gsi_sam_crate);
	gsi_siderem_crate_create(&crate->gsi_siderem_crate);
	gsi_tacquila_crate_create(&crate->gsi_tacquila_crate);
	pnpi_cros3_crate_create(&crate->pnpi_cros3_crate);

	/*
	 * Iterate over tags & barriers & modules, extract tag names and put
	 * module/counter refs into tags and tag counters into modules.
	 */
	VECTOR_INIT(&tag_active_vec);
	/* TODO: Remove "Default" tag if not populated? Not sure... */
	tag = tag_get(crate, "Default");
	VECTOR_APPEND(&tag_active_vec, tag);
	counter = get_counter(crate, "auto:Default");
	VECTOR_APPEND(&tag->counter_ref_vec, counter);

#define CONFIG_GET_SOURCE \
	char const *path;\
	int line_no;\
	config_get_source(module_block, &path, &line_no, NULL)

	module_id = 0;
	for (module_block = config_get_block(crate_block, KW_NONE);
	    NULL != module_block;
	    module_block = config_get_block_next(module_block, KW_NONE)) {
		struct CrateTag **tag_ref;
		struct Module *module;
		enum Keyword module_type;
		unsigned i;
		int tags_need_pex = 0;

		module_type = config_get_block_name(module_block);
		if (KW_TAGS == module_type) {
			char buf[32];
			char const *p;

			/*
			 * Create new active tag list, create tags in the
			 * crate as needed.
			 */
			VECTOR_FREE(&tag_active_vec);
			for (i = 0;; ++i) {
				char const *name;

				if (!config_get_block_param_exists(
				    module_block, i)) {
					break;
				}
				name = config_get_block_param_string(
				    module_block, i);
				tag = tag_get(crate, name);
				VECTOR_APPEND(&tag_active_vec, tag);
			}
			if (0 == tag_active_vec.size) {
				CONFIG_GET_SOURCE;
				log_die(LOGL, "%s:%d: Missing tag name!",
				    path, line_no);
			}
			/* Check counter reference in body. */
			p = config_get_string(module_block, KW_SCALER_NAME);
			if ('\0' == *p) {
				int line_no, col_no;

				config_get_source(module_block, NULL,
				    &line_no, &col_no);
				snprintf_(buf, sizeof buf, "auto:%u:%u",
				    line_no, col_no);
				p = buf;
			}
			counter = get_counter(crate, p);
			/* Assign counter to all active tags. */
			VECTOR_FOREACH(tag_ref, &tag_active_vec) {
				struct CrateCounter **counter_ref;
				int counter_found;

				tag = *tag_ref;
				counter_found = 0;
				VECTOR_FOREACH(counter_ref,
				    &tag->counter_ref_vec) {
					if (*counter_ref == counter) {
						counter_found = 1;
						break;
					}
				}
				if (!counter_found) {
					VECTOR_APPEND(&tag->counter_ref_vec,
					    counter);
				}
			}
			continue;
		} else if (KW_ID_SKIP == module_type) {
			unsigned num;

			if (config_get_block_param_exists(module_block, 0)) {
				num = config_get_block_param_int32(
				    module_block, 0);
			} else {
				num = 1;
			}
			module_id += num;
			continue;
		} else if (KW_BARRIER == module_type) {
			module = module_create_base(sizeof *module, NULL);
			module->type = KW_BARRIER;
			module_insert(crate, &tag_active_vec, module);
			continue;
		} else if (KW_GSI_SIDEREM_CRATE == module_type) {
			gsi_siderem_crate_configure(&crate->gsi_siderem_crate,
			    module_block);
			continue;
		} else if (KW_GSI_TACQUILA_CRATE == module_type) {
			gsi_tacquila_crate_configure(
			    &crate->gsi_tacquila_crate, module_block);
			continue;
		} else if (KW_PNPI_CROS3_CRATE == module_type) {
			pnpi_cros3_crate_configure(&crate->pnpi_cros3_crate,
			    module_block);
			continue;
		} else if (KW_GSI_PEX == module_type) {
			if (NULL != crate->gsi_pex.config) {
				CONFIG_GET_SOURCE;
				log_die(LOGL, "%s:%d: More than one GSI PEX "
				    "configured!", path, line_no);
			}
			crate->gsi_pex.config = module_block;
			continue;
		}
		++crate->module_num;
		LOGF(info)(LOGL, "module[%u]=%s.", module_id,
		    keyword_get_string(module_type));
		module = module_create(crate, module_type, module_block);
		module->id = module_id++;
		module->crate_counter = &counter->cur;
		if (KW_GSI_SAM == module_type) {
			/*
			 * The GSI SAM is special since it has "client"
			 * electronics on GTB that benefits from one mapping
			 * for all, special init, and separate
			 * readout/parsing, so some awkward mix of
			 * localism/globalism.
			 */
			gsi_sam_crate_collect(&crate->gsi_sam_crate,
			    &crate->gsi_siderem_crate,
			    &crate->gsi_tacquila_crate,
			    &crate->pnpi_cros3_crate, module);
		} else if (KW_GSI_CTDC == module_type) {
			gsi_ctdc_crate_add(&crate->gsi_ctdc_crate, module);
			tags_need_pex = 1;
		} else if (KW_GSI_KILOM == module_type) {
			gsi_kilom_crate_add(&crate->gsi_kilom_crate, module);
			tags_need_pex = 1;
		} else if (KW_GSI_MPPC_ROB == module_type) {
			gsi_mppc_rob_crate_add(&crate->gsi_mppc_rob_crate,
			    module);
			tags_need_pex = 1;
		} else if (KW_GSI_FEBEX == module_type) {
			gsi_febex_crate_add(&crate->gsi_febex_crate, module);
			tags_need_pex = 1;
		} else if (KW_GSI_TAMEX == module_type) {
			gsi_tamex_crate_add(&crate->gsi_tamex_crate, module);
			tags_need_pex = 1;
		}
		VECTOR_FOREACH(tag_ref, &tag_active_vec) {
			tag = *tag_ref;
			tag->gsi_pex_is_needed |= tags_need_pex;
		}
		crate_needs_pex |= tags_need_pex;

		if (0 != module->event_max) {
			/*
			 * This is a module that can deliver event data, check
			 * if a barrier is necessary between the previous
			 * module of any active tag and this module.
			 */
			VECTOR_FOREACH(tag_ref, &tag_active_vec) {
				struct Module **module_ref;

				tag = *tag_ref;
				VECTOR_FOREACH_REV(module_ref,
				    &tag->module_ref_vec) {
					struct Module *rover;

					rover = *module_ref;
					if (KW_BARRIER == rover->type) {
						break;
					}
					if (0 == rover->event_max) {
						continue;
					}
					if (signature_match(module, rover)) {
						CONFIG_GET_SOURCE;
						log_die(LOGL, "%s:%d: nurdlib"
						    " demands a module "
						    "barrier.", path,
						    line_no);
					}
				}
			}
		}

		if (KW_GSI_TRIDI == module->type ||
		    KW_GSI_VULOM == module->type) {
			char const *tag_name;

			/*
			 * Multi-event setup for TRLO II modules.
			 */
			tag_name = trloii_get_multi_event_tag_name(module);
			if (NULL != tag_name) {
				if (NULL != crate->trloii_multi_event.module) {
					CONFIG_GET_SOURCE;
					log_die(LOGL, "%s:%d: Several GSI "
					    "Vulom/Tridi modules count "
					    "master starts, impossible!",
					    path, line_no);
				}
				crate->trloii_multi_event.module = module;
				crate->trloii_multi_event.tag_name = tag_name;
			}
		}

		module_insert(crate, &tag_active_vec, module);
	}
	VECTOR_FREE(&tag_active_vec);

	if (crate_needs_pex && NULL == crate->gsi_pex.config) {
		log_die(LOGL, "%s: Modules requiring PEX configured, but no "
		    "PEX!", crate->name);
	}

	/* Find and assign counter references. */
	TAILQ_FOREACH(counter, &crate->counter_list, next) {
		struct CrateScaler *scaler;

		LOGF(verbose)(LOGL, "Counter=%s.", counter->name);
		if (0 == strncmp(counter->name, "auto:", 5)) {
			continue;
		}
		TAILQ_FOREACH(scaler, &crate->scaler_list, next) {
			if (0 == strcmp(counter->name, scaler->name)) {
				counter->scaler = scaler;
				break;
			}
		}
		if (NULL == counter->scaler) {
			log_die(LOGL, "Undefined reference to scaler \"%s\".",
			    counter->name);
		}
	}

	if (crate->acvt.yes) {
		if (NULL == crate->acvt.module) {
			log_die(LOGL, "ACVT requested, but no ACVT capable "
			    "module configured.");
		}
		LOGF(info)(LOGL, "%s[%u]=%s chosen as ACVT module.",
		    crate->name, crate->acvt.module->id,
		    keyword_get_string(crate->acvt.module->type));
	}

	if (NULL != crate->trloii_multi_event.module) {
		TAILQ_FOREACH(tag, &crate->tag_list, next) {
			if (0 == strcmp(tag->name,
			    crate->trloii_multi_event.tag_name)) {
				crate->trloii_multi_event.tag = tag;
				break;
			}
		}
		if (NULL == crate->trloii_multi_event.tag) {
			log_die(LOGL, "%s: TRLO II multi-event tag \"%s\" "
			    "does not exist!", crate->name,
			    crate->trloii_multi_event.tag_name);
		}
	}

	if (!thread_mutex_init(&crate->mutex)) {
		log_die(LOGL, "Could not create readout mutex.");
	}

	config_touched_assert(crate_block, 0);
	TAILQ_INSERT_TAIL(&g_crate_list, crate, next);

	crate->dt_release.is_on = 1;

	LOGF(info)(LOGL, "crate_create(%s) }", crate->name);

	return crate;
}

void
crate_set_init_callback(struct Crate *a_crate, InitCallback init_callback,
  InitCallback deinit_callback)
{
	a_crate->init_callback = init_callback;
	a_crate->deinit_callback = deinit_callback;
}

void
crate_deinit(struct Crate *a_crate)
{
	struct Module *module;

	LOGF(info)(LOGL, "crate_deinit(%s) {", a_crate->name);

	/* This is used to e.g. stop the MVLC sequencer - should be early. */
	if (a_crate->deinit_callback) {
		a_crate->deinit_callback(a_crate);
	}

	if (a_crate->shadow.is_running) {
		LOGF(info)(LOGL, "Stopping shadow thread.");
		a_crate->shadow.is_running = 0;
		thread_clean(&a_crate->shadow.thread);
	}

	THREAD_MUTEX_LOCK(&a_crate->mutex);
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (NULL != module->props) {
			push_log_level(module);
			module->props->deinit(module);
			pop_log_level(module);
		}
	}
	gsi_sam_crate_deinit(&a_crate->gsi_sam_crate);
	if (NULL != a_crate->gsi_pex.pex) {
		gsi_pex_deinit(a_crate->gsi_pex.pex);
	}
	map_deinit();
	a_crate->state = STATE_REINIT;
	thread_mutex_unlock(&a_crate->mutex);

	LOGF(info)(LOGL, "crate_deinit(%s) }", a_crate->name);
}

int
crate_free_running_get(struct Crate *a_crate)
{
	return a_crate->is_free_running;
}

void
crate_free_running_set(struct Crate *a_crate, int a_yes)
{
	a_crate->is_free_running = a_yes;
}

int
crate_get_do_shadow(struct Crate const *a_crate)
{
	return 0 != a_crate->shadow.buf_bytes;
}

struct PnpiCros3Crate *
crate_get_cros3_crate(struct Crate *a_crate)
{
	return &a_crate->pnpi_cros3_crate;
}

struct GsiCTDCCrate *
crate_get_ctdc_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_ctdc_crate;
}

struct GsiFebexCrate *
crate_get_febex_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_febex_crate;
}

struct GsiKilomCrate *
crate_get_kilom_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_kilom_crate;
}

struct GsiMppcRobCrate *
crate_get_mppc_rob_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_mppc_rob_crate;
}

struct GsiSideremCrate *
crate_get_siderem_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_siderem_crate;
}

struct GsiTacquilaCrate *
crate_get_tacquila_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_tacquila_crate;
}

struct GsiTamexCrate *
crate_get_tamex_crate(struct Crate *a_crate)
{
	return &a_crate->gsi_tamex_crate;
}

int
crate_dt_is_on(struct Crate const *a_crate)
{
	return a_crate->dt_release.is_on;
}

void
crate_dt_release_inhibit_once(struct Crate *a_crate)
{
	LOGF(spam)(LOGL, "crate_dt_release_inhibit_once(%s) {",
	    a_crate->name);
	a_crate->dt_release.do_inhibit = 1;
	LOGF(spam)(LOGL, "crate_dt_release_inhibit_once(%s) }",
	    a_crate->name);
}

void
crate_dt_release_set_func(struct Crate *a_crate, void (*a_func)(void *), void
    *a_data)
{
	a_crate->dt_release.func = a_func;
	a_crate->dt_release.data = a_data;
}

void
crate_free(struct Crate **a_crate)
{
	struct Crate *crate;

	crate = *a_crate;
	if (NULL == crate) {
		return;
	}
	LOGF(info)(LOGL, "crate_free(%s) {", crate->name);
	if (STATE_REINIT != crate->state) {
		/* TODO: Is this fine? */
		crate_deinit(crate);
	}
	while (!TAILQ_EMPTY(&crate->module_list)) {
		struct Module *module;

		module = TAILQ_FIRST(&crate->module_list);
		TAILQ_REMOVE(&crate->module_list, module, next);
		module_free(&module);
	}
	while (!TAILQ_EMPTY(&crate->tag_list)) {
		struct CrateTag *tag;

		tag = TAILQ_FIRST(&crate->tag_list);
		TAILQ_REMOVE(&crate->tag_list, tag, next);
		FREE(tag);
	}
	gsi_siderem_crate_destroy(&crate->gsi_siderem_crate);
	gsi_tacquila_crate_destroy(&crate->gsi_tacquila_crate);
	pnpi_cros3_crate_destroy(&crate->pnpi_cros3_crate);
	thread_mutex_clean(&crate->mutex);
	map_blt_dst_free(&crate->shadow.dst);
	TAILQ_REMOVE(&g_crate_list, crate, next);
	FREE(crate->name);
	FREE(*a_crate);
	LOGF(info)(LOGL, "crate_free }");
}

struct CrateTag *
crate_get_tag_by_name(struct Crate *a_crate, char const *a_name)
{
	struct CrateTag *tag;

	if (NULL == a_name) {
		return TAILQ_FIRST(&a_crate->tag_list);
	}
	TAILQ_FOREACH(tag, &a_crate->tag_list, next) {
		if (0 == strcmp(tag->name, a_name)) {
			return tag;
		}
	}
	return NULL;
}

char const *
crate_get_name(struct Crate const *a_crate)
{
	return a_crate->name;
}

void
crate_gsi_pex_goc_read(uint8_t a_crate_i, uint8_t a_sfp, uint16_t a_card,
    uint32_t a_offset, uint16_t a_num, uint32_t *a_value)
{
#if !NCONF_mGSI_PEX_bNO
	struct Crate *crate;
	struct GsiPex *pex;
	unsigned i;

	LOGF(spam)(LOGL, "crate_gsi_pex_goc_read"
	    "(cr=%u,sfp=%u,cd=%u,ofs=%u,num=%u) {",
	    a_crate_i, a_sfp, a_card, a_offset, a_num);
	memset(a_value, 0, a_num * sizeof *a_value);
	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		goto crate_gsi_pex_goc_read_done;
	}
	pex = crate->gsi_pex.pex;
	if (NULL == pex) {
		goto crate_gsi_pex_goc_read_done;
	}
	THREAD_MUTEX_LOCK(&crate->mutex);
	for (i = 0; i < a_num; ++i) {
		uint32_t u32;

		if (gsi_pex_slave_read(pex, a_sfp, a_card, a_offset + 4 * i,
		    &u32)) {
			a_value[i] = u32;
		}
	}
	thread_mutex_unlock(&crate->mutex);
crate_gsi_pex_goc_read_done:
	LOGF(spam)(LOGL, "crate_gsi_pex_goc_read }");
#else
	(void)a_crate_i;
	(void)a_sfp;
	(void)a_card;
	(void)a_offset;
	(void)a_num;
	(void)a_value;
#endif
}

void
crate_gsi_pex_goc_write(uint8_t a_crate_i, uint8_t a_sfp, uint16_t a_card,
    uint32_t a_offset, uint16_t a_num, uint32_t a_value)
{
#if !NCONF_mGSI_PEX_bNO
	struct Crate *crate;
	struct GsiPex *pex;
	uint32_t ofs;
	unsigned i;

	LOGF(spam)(LOGL, "crate_gsi_pex_goc_write"
	    "(cr=%u,sfp=%u,cd=%u,ofs=%u,num=%u) {",
	    a_crate_i, a_sfp, a_card, a_offset, a_num);
	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		goto crate_gsi_pex_goc_write_done;
	}
	pex = crate->gsi_pex.pex;
	if (NULL == pex) {
		goto crate_gsi_pex_goc_write_done;
	}
	THREAD_MUTEX_LOCK(&crate->mutex);
	ofs = a_offset;
	for (i = 0; i < a_num; ++i) {
		int ret;

		ret = gsi_pex_slave_write(pex, a_sfp, a_card, ofs, a_value);
		ofs += sizeof a_value;
		(void)ret;
	}
	thread_mutex_unlock(&crate->mutex);
crate_gsi_pex_goc_write_done:
	LOGF(spam)(LOGL, "crate_gsi_pex_goc_write }");
#else
	(void)a_crate_i;
	(void)a_sfp;
	(void)a_card;
	(void)a_offset;
	(void)a_num;
	(void)a_value;
#endif
}

unsigned
crate_gsi_mbs_trigger_get(struct Crate const *a_crate)
{
	if (0 == a_crate->gsi_mbs_trigger) {
		log_die(LOGL, "%s: Someone wants GSI MBS trigger, "
		    "but the user code has left it at 0!", a_crate->name);
	}
	return a_crate->gsi_mbs_trigger;
}

void
crate_gsi_mbs_trigger_set(struct Crate *a_crate, unsigned a_trigger)
{
	a_crate->gsi_mbs_trigger = a_trigger;
}

struct GsiPex *
crate_gsi_pex_get(struct Crate const *a_crate)
{
	return a_crate->gsi_pex.pex;
}

void
crate_info_pack(struct Packer *a_packer, int a_crate_i)
{
	struct Crate const *crate;

	LOGF(debug)(LOGL, "crate_info_pack(cr=%d) {", a_crate_i);
	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		PACK(*a_packer, 16, -1, fail);
		PACK_LOC(*a_packer, fail);
	} else {
		PACK(*a_packer, 16, crate->event_max_override, fail);
		PACK(*a_packer,  8, crate->dt_release.do_it, fail);
		PACK(*a_packer, 16, crate->acvt.ns, fail);
		PACK(*a_packer, 32, crate->shadow.buf_bytes, fail);
		PACK(*a_packer, 32, crate->shadow.max_bytes, fail);
	}
fail:
	LOGF(debug)(LOGL, "crate_info_pack }");
}

void
crate_init(struct Crate *a_crate)
{
	struct CrateTag *tag;
	struct CrateCounter *counter;
	struct Module *module;

	LOGF(info)(LOGL, "crate_init(%s) {", a_crate->name);

	THREAD_MUTEX_LOCK(&a_crate->mutex);
crate_init_there_is_no_try:
	if (NULL != a_crate->gsi_pex.config) {
		if (NULL == a_crate->gsi_pex.pex) {
			a_crate->gsi_pex.pex =
			    gsi_pex_create(a_crate->gsi_pex.config);
		}
		gsi_pex_init(a_crate->gsi_pex.pex, a_crate->gsi_pex.config);
	}
	a_crate->shadow.module_readable_num = 0;
#define INIT_BATCH(name, member) do {\
		if (!name##_init_slow(&a_crate->member)) {\
			goto crate_init_done;\
		}\
	} while (0)
#define INIT_CRATE(name) INIT_BATCH(name, name)
	INIT_CRATE(gsi_sam_crate);
	/* Init_slow. */
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (NULL == module->props) {
			continue;
		}
		LOGF(info)(LOGL, "Slow-init module[%u]=%s.", module->id,
		    keyword_get_string(module->type));
		push_log_level(module);
		a_crate->module_init_id = module->id;
		if (!module->props->init_slow(a_crate, module)) {
			pop_log_level(module);
			goto crate_init_done;
		}
		module_init_id_mark(a_crate, module);
		pop_log_level(module);
	}
	module_init_id_clear(a_crate);
	/*
	 * Most modules know the event_max in *create_, but some may want to
	 * know what firmware is present, so we'll check after the slow init.
	 */
	TAILQ_FOREACH(tag, &a_crate->tag_list, next) {
		struct Module **module_ref;

		tag->event_max = 1e9;
		VECTOR_FOREACH(module_ref, &tag->module_ref_vec) {
			module = *module_ref;
			if (module->event_max > 0) {
				tag->event_max = MIN(tag->event_max,
				    module->event_max);
			}
		}
		if (0 < a_crate->event_max_override) {
			if (crate_get_do_shadow(a_crate) ||
			    a_crate->event_max_override <= tag->event_max) {
				tag->event_max = a_crate->event_max_override;
			} else {
				log_die(LOGL, "%s:%s: Crate event_max=%u but "
				    "configured modules only support<=%u, "
				    "your system would busy lock!",
				    a_crate->name, tag->name,
				    a_crate->event_max_override,
				    tag->event_max);
			}
		}
		LOGF(verbose)(LOGL, "%s: max events/trig=%u.", tag->name,
		    tag->event_max);
	}
	if (NULL != a_crate->trloii_multi_event.module) {
		trloii_multi_event_set_limit(
		    a_crate->trloii_multi_event.module,
		    a_crate->trloii_multi_event.tag->event_max);
	}
	/* Init fast. */
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (NULL == module->props) {
			continue;
		}
		LOGF(info)(LOGL, "Fast-init module[%u]=%s.", module->id,
		    keyword_get_string(module->type));
		push_log_level(module);
		a_crate->module_init_id = module->id;
		if (!module->props->init_fast(a_crate, module)) {
			pop_log_level(module);
			goto crate_init_done;
		}
		module_init_id_mark(a_crate, module);
		pop_log_level(module);
	}
	module_init_id_clear(a_crate);
	INIT_BATCH(caen_v1n90_micro, module_list);
	if (!caen_v1n90_micro_init_fast(&a_crate->module_list)) {
		goto crate_init_done;
	}
	INIT_CRATE(gsi_ctdc_crate);
	INIT_CRATE(gsi_febex_crate);
	INIT_CRATE(gsi_kilom_crate);
	INIT_CRATE(gsi_mppc_rob_crate);
	INIT_CRATE(gsi_siderem_crate);
	INIT_CRATE(gsi_tacquila_crate);
	INIT_CRATE(gsi_tamex_crate);
	INIT_CRATE(pnpi_cros3_crate);
	time_sleep(a_crate->postinit_sleep_s);
	/* Post-init, after all modules have been inited. */
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (NULL == module->props) {
			continue;
		}
		if (NULL != module->props->post_init) {
			push_log_level(module);
			LOGF(info)(LOGL, "Post-init module[%u]=%s.",
			    module->id, keyword_get_string(module->type));
			if (!module->props->post_init(a_crate, module)) {
				pop_log_level(module);
				goto crate_init_done;
			}
			pop_log_level(module);
		}
		config_touched_assert(module->config, 1);
	}
	if (NULL != a_crate->gsi_pex.pex) {
		gsi_pex_reset(a_crate->gsi_pex.pex);
	}
	TAILQ_FOREACH(counter, &a_crate->counter_list, next) {
		counter->cur.mask = ~0;
		if (NULL == counter->scaler) {
			counter->cur.value = 0;
		} else {
			if (0 != counter->scaler->get_counter(
			    counter->scaler->module, counter->scaler->data,
			    &counter->cur)) {
				goto crate_init_done;
			}
		}
		counter->prev = counter->cur.value;
	}
	/* If triggered, check that modules are empty. */
	if (!a_crate->is_free_running &&
	    0 != check_empty(a_crate)) {
		goto crate_init_done;
	}
	/* Finalize, reset module result and latch event counter. */
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		module->result = 0;
		if (NULL != module->props) {
			module_counter_latch(module);
		}
		if (NULL != module->crate_counter) {
			module->crate_counter_prev =
			    module->crate_counter->value;
		}
		if (0 != module->event_max) {
			++a_crate->shadow.module_readable_num;
		}
	}

	if (crate_get_do_shadow(a_crate)) {
		LOGF(info)(LOGL, "Starting shadow thread.");
		a_crate->shadow.is_running = 1;
		if (!thread_start(&a_crate->shadow.thread, shadow_func,
		    a_crate)) {
			log_die(LOGL, "Could not start shadow thread.");
		}
	}

	/* This is used to e.g. start the MVLC sequencer - should be late. */
	if (a_crate->init_callback) {
		a_crate->init_callback(a_crate);
	}

	a_crate->state = STATE_PREPARED;
	a_crate->shadow.do_buf_rebuild = 1;
crate_init_done:
	module_init_id_clear(a_crate);
	if (STATE_REINIT == a_crate->state) {
		/* TODO: Do we really want to open the mutex? */
		thread_mutex_unlock(&a_crate->mutex);
		crate_deinit(a_crate);
		THREAD_MUTEX_LOCK(&a_crate->mutex);
		time_sleep(a_crate->reinit_sleep_s);
		goto crate_init_there_is_no_try;
	}
	thread_mutex_unlock(&a_crate->mutex);
	LOGF(info)(LOGL, "crate_init(%s) }", a_crate->name);
}

void
crate_memtest(struct Crate const *a_crate, int a_chunks)
{
	struct Module *module;

	LOGF(info)(LOGL, "crate_memtest(%s) {", a_crate->name);
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		/*
		 * TODO: Crate state?
		 */
		if (NULL != module->props &&
		    NULL != module->props->memtest) {
			push_log_level(module);
			module->props->memtest(module, a_chunks);
			pop_log_level(module);
		}
	}
	LOGF(info)(LOGL, "crate_memtest(%s) }", a_crate->name);
}

void
crate_module_access_pack(uint8_t a_crate_i, uint8_t a_module_j, int
    a_submodule_k, struct Packer *a_packer, struct PackerList *a_list)
{
	struct Crate *crate;
	struct Module *module;

	LOGF(debug)(LOGL, "crate_module_access_pack(cr=%u,mod=%u,sub=%u) {",
	    a_crate_i, a_module_j, a_submodule_k);
	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		PACKER_LIST_PACK(*a_list, 16, -1);
		PACKER_LIST_PACK_LOC(*a_list);
		PACKER_LIST_PACK_STR(*a_list, "Crate not found");
		goto done;
	}
	module = get_module(crate, a_module_j);
	if (NULL == module) {
		PACKER_LIST_PACK(*a_list, 16, -1);
		PACKER_LIST_PACK_LOC(*a_list);
		PACKER_LIST_PACK_STR(*a_list, "Module not found");
		goto done;
	}
	THREAD_MUTEX_LOCK(&crate->mutex);
	module_access_pack(a_list, a_packer, module, a_submodule_k);
	thread_mutex_unlock(&crate->mutex);
done:
	LOGF(debug)(LOGL, "crate_module_access_pack }");
}

struct Module *
crate_module_find(struct Crate const *a_crate, enum Keyword a_module_type,
    unsigned a_idx)
{
	struct Module *module;
	unsigned idx;

	idx = 0;
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (a_module_type == module->type) {
			if (a_idx == idx) {
				return module;
			}
			++idx;
		}
	}
	return NULL;
}

size_t
crate_module_get_num(struct Crate const *a_crate)
{
	return a_crate->module_num;
}

void
crate_module_remap_id(struct Crate *a_crate, unsigned a_from, unsigned a_to)
{
	if (a_crate->module_init_id != a_from) {
		log_die(LOGL, "Module id=%u, but tried to remap id=%u!",
		    a_crate->module_init_id, a_from);
	}
	a_crate->module_init_id = a_to;
}

void
crate_pack(struct PackerList *a_list)
{
	struct Crate const *crate;
	uint8_t crate_num;

	LOGF(debug)(LOGL, "crate_pack {");
	assert(TAILQ_EMPTY(a_list));
	crate_num = 0;
	TAILQ_FOREACH(crate, &g_crate_list, next) {
		++crate_num;
	}
	LOGF(debug)(LOGL, "Number of crates=%u.", crate_num);
	PACKER_LIST_PACK(*a_list, 8, crate_num);
	TAILQ_FOREACH(crate, &g_crate_list, next) {
		struct Module *module;
		uint8_t num;

		LOGF(debug)(LOGL, "Crate=%s.", crate->name);
		PACKER_LIST_PACK_STR(*a_list, crate->name);
		/*
		 * We'd like to show barriers, so 'module_num' is not enough.
		 */
		num = 0;
		TAILQ_FOREACH(module, &crate->module_list, next) {
			++num;
		}
		LOGF(debug)(LOGL, "Number of (virtual) modules=%u.", num);
		PACKER_LIST_PACK(*a_list, 8, num);
		TAILQ_FOREACH(module, &crate->module_list, next) {
			PACKER_LIST_PACK(*a_list, 16, module->type);
			if (NULL == module->props ||
			    NULL == module->props->sub_module_pack) {
				PACKER_LIST_PACK(*a_list, 8, 0);
			} else if (!module->props->sub_module_pack(module,
			    a_list)) {
				log_error(LOGL, "Could not pack submodule.");
			}
		}
	}
	LOGF(debug)(LOGL, "crate_pack }");
}

uint32_t
crate_readout_dt(struct Crate *a_crate)
{
	struct CrateCounter *counter;
	struct Module *module;
	double t0;
	uint32_t result;
	unsigned for_it;

	LOGF(spam)(LOGL, "crate_readout_dt(%s) {", a_crate->name);
	result = 0;

	/* Reset eb_final pointers so they don't point to old data. */
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		module->eb_final.ptr = NULL;
		module->eb_final.bytes = 0;
	}

	if (STATE_REINIT == a_crate->state) {
		/*
		 * Re-init, but only go to prepared state.
		 * If early DT release was performed during read-out of the
		 * previous event, we may have accepted some garbage while in
		 * a strange state, so we have to clear and wait until the
		 * next event.
		 */
		crate_deinit(a_crate);
		time_sleep(a_crate->reinit_sleep_s);
		crate_init(a_crate);
		goto crate_readout_dt_done;
	}

	/*
	 * In-dt readout.
	 */
	THREAD_MUTEX_LOCK(&a_crate->mutex);

	/* Counters. */
	TAILQ_FOREACH(counter, &a_crate->counter_list, next) {
		if (NULL != counter->scaler) {
			counter->scaler->get_counter(counter->scaler->module,
			    counter->scaler->data, &counter->cur);
		}
		LOGF(spam)(LOGL, "Counter \"%s\" = 0x%08x->0x%08x/%u = "
		    "0x%08x",
		    counter->name, counter->prev, counter->cur.value,
		    bits_get_count(counter->cur.mask),
		    COUNTER_DIFF_RAW(counter->cur, counter->prev));
	}

	t0 = time_getd();

	/* All module event counters. */
	a_crate->dt_release.for_it = 0;
	for_it = 0;
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		uint32_t diff_module, diff_shadow;
		int ok;

		if (module->skip_dt) {
			LOGF(spam)(LOGL, "module[%d]->skip_dt.", module->id);
			continue;
		}
		if (0 == module->event_max) {
			LOGF(spam)(LOGL, "module[%d]->event_max == 0. "
			    "Skipping readout_dt.", module->id);
			continue;
		}
		push_log_level(module);
		ok = 0;
		diff_shadow = 0xdeadbeef;
		diff_module = 0xdeadbeef;
		/*
		 * Busy wait until we have event counter, and shadow data
		 * counters if applicable.
		 */
		for (;;) {
			struct Counter shadow_counter;
			double t;
			uint32_t ret;

			ret = module->props->readout_dt(a_crate, module);
			module->result |= ret;
			if (0 != ret) {
				log_error(LOGL, "%s[%u]=%s: readout_dt failed"
				    " = 0x%08x.", a_crate->name, module->id,
				    keyword_get_string(module->type), ret);
				break;
			}
			if (a_crate->is_free_running) {
				/*
				 * Don't check data availability when free-
				 * running.
				 */
				ok = 1;
				break;
			}
			diff_module = COUNTER_DIFF(*module->crate_counter,
			    module->event_counter, module->this_minus_crate);
			/* TODO: Clean this. */
			shadow_counter.value =
			    module->shadow.data_counter_value;
			shadow_counter.mask = module->event_counter.mask;
			diff_shadow = COUNTER_DIFF(*module->crate_counter,
			    shadow_counter, module->this_minus_crate);
			if (0 == diff_module &&
			    (!crate_get_do_shadow(a_crate) ||
			     NULL == module->props->readout_shadow ||
			     0 == diff_shadow)) {
				ok = 1;
				break;
			}
			crate_acvt_grow(a_crate);
			t = time_getd();
			if (t > t0 + DT_TIMEOUT) {
				log_error(LOGL, "%s[%u]=%s: readout_dt "
				    "timeout, trigger/master-start missing?",
				    a_crate->name, module->id,
				    keyword_get_string(module->type));
				module->result |=
				    CRATE_READOUT_FAIL_EVENT_COUNTER_MISMATCH;
				/*
				 * Try remaining modules even when we've timed
				 * out, things are broken anyway so let's
				 * update everything.
				 */
				break;
			}
			thread_mutex_unlock(&a_crate->mutex);
			sched_yield();
			THREAD_MUTEX_LOCK(&a_crate->mutex);
			/*
			 * TODO: This will suppress the successful
			 * readout_dt :( Keep a short list of suppressed logs?
			 */
			log_suppress_all_levels(1);
		}
		log_suppress_all_levels(0);
		pop_log_level(module);
		result |= module->result;
		if (ok) {
			if (0 == (MODULE_FLAG_EARLY_DT &
			    module->props->flags)) {
				/*
				 * Cannot release DT until after this module
				 * in "crate_readout".
				 */
				a_crate->dt_release.for_it = for_it + 1;
			}
		} else {
			log_error(LOGL, "%s[%u]=%s: Event counter: "
			    "crate=0x%08x/%u, this-crate=0x%08x, "
			    "module=0x%08x/%u diff=0x%08x, "
			    "shadow=0x%08x/%u diff=0x%08x.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type),
			    module->crate_counter->value,
			    bits_get_count(module->crate_counter->mask),
			    module->this_minus_crate,
			    module->event_counter.value,
			    bits_get_count(module->event_counter.mask),
			    diff_module,
			    module->shadow.data_counter_value,
			    bits_get_count(module->event_counter.mask),
			    diff_shadow);
		}
		if (crate_get_do_shadow(a_crate) &&
		    NULL != module->props->readout_shadow) {
			struct ModuleShadowBuffer *next;

			/*
			 * Switch buffers so the shadow thread can continue.
			 */
			THREAD_MUTEX_LOCK(&module->shadow.buf_mutex);
			module->shadow.buf_idx ^= 1;
			next = &module->shadow.buf[module->shadow.buf_idx];
			COPY(next->eb, next->store);
			thread_mutex_unlock(&module->shadow.buf_mutex);
		}
		++for_it;
	}

	if (a_crate->dt_release.do_it &&
	    a_crate->dt_release.for_it_prev != a_crate->dt_release.for_it) {
		LOGF(info)(LOGL,
		    "DT release possible on module # %u/%u.",
		    a_crate->dt_release.for_it, for_it);
	    a_crate->dt_release.for_it_prev = a_crate->dt_release.for_it;
	}

	if (a_crate->acvt.yes) {
		int ns;

		/*
		 * ACVT is done every N:th readout (single-event, multi-event
		 * etc) which should be somewhat correlated to the # of events
		 * that poll for data.
		 */
		assert(NULL != a_crate->acvt.module &&
		    NULL != a_crate->acvt.set_cvt);
		ns = -1;
		if (a_crate->acvt.do_grow) {
			a_crate->acvt.pioneer_counter = 0;
			ns = a_crate->acvt.ns + 100;
			a_crate->acvt.do_grow = 0;
		} else {
			++a_crate->acvt.pioneer_counter;
			if (1000 < a_crate->acvt.pioneer_counter) {
				a_crate->acvt.pioneer_counter = 0;
				ns = a_crate->acvt.ns < 100 ? 0 :
				    a_crate->acvt.ns - 100;
			}
		}
		if (-1 != ns && (unsigned)ns != a_crate->acvt.ns) {
			LOGF(debug)(LOGL, "ACVT = %u ns.", ns);
			a_crate->acvt.ns = ns;
			a_crate->acvt.set_cvt(a_crate->acvt.module, ns);
		}
	}

	if (0 != a_crate->module_configed_vec.size) {
		crate_dt_release_inhibit_once(a_crate);
	}

	thread_mutex_unlock(&a_crate->mutex);

crate_readout_dt_done:
	if (0 == result) {
		a_crate->state = STATE_READY;
	} else {
		a_crate->state = STATE_REINIT;
		log_error(LOGL, "%s: readout_dt failed!", a_crate->name);
	}
	a_crate->sync.num = 0;
	LOGF(spam)(LOGL, "crate_readout_dt(%s,0x%08x) }", a_crate->name,
	    result);
	return result;
}

uint32_t
crate_readout(struct Crate *a_crate, struct EventBuffer *a_event_buffer)
{
	struct EventBuffer eb_orig;
	struct Module *module;
	uint32_t result;
	unsigned is_mutex, for_it;

	LOGF(spam)(LOGL, "crate_readout(%s) {", a_crate->name);
	COPY(eb_orig, *a_event_buffer);
	result = 0;
	if (STATE_READY != a_crate->state) {
		goto crate_readout_done;
	}
	/* Read/merge all modules, release dt when it's safe. */
	is_mutex = 0;
	for_it = 0;
	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		if (a_crate->dt_release.for_it == for_it) {
			dt_release(a_crate);
		}
		if (KW_BARRIER == module->type) {
			uint32_t *p32;

			p32 = a_event_buffer->ptr;
			*p32++ = BARRIER_WORD;
			EVENT_BUFFER_ADVANCE(*a_event_buffer, p32);
		} else if (0 == module->event_max) {
		} else if (!crate_get_do_shadow(a_crate) ||
		    NULL == module->props->readout_shadow) {
			if (!is_mutex) {
				THREAD_MUTEX_LOCK(&a_crate->mutex);
				is_mutex = 1;
			}
			result |= read_module(a_crate, module,
			    a_event_buffer);
		} else {
			if (is_mutex) {
				thread_mutex_unlock(&a_crate->mutex);
				is_mutex = 0;
			}
			result |= shadow_merge_module(a_crate, module,
			    a_event_buffer);
		}
		++for_it;
	}
	if (0 == result) {
		if (!a_crate->is_free_running &&
		    crate_dt_is_on(a_crate)) {
			if (!is_mutex) {
				THREAD_MUTEX_LOCK(&a_crate->mutex);
				is_mutex = 1;
			}
			result |= check_empty(a_crate);
		}
	} else {
		a_crate->state = STATE_REINIT;
		log_error(LOGL, "%s: readout failed!", a_crate->name);
	}
	if (is_mutex) {
		thread_mutex_unlock(&a_crate->mutex);
	}
crate_readout_done:
	EVENT_BUFFER_INVARIANT(eb_orig, *a_event_buffer);
	LOGF(spam)(LOGL, "crate_readout(%s,0x%08x) }", a_crate->name, result);
	return result;
}

void
crate_readout_finalize(struct Crate *a_crate)
{
	struct CrateCounter *counter;

	LOGF(spam)(LOGL, "crate_readout_finalize(%s) {", a_crate->name);
#if 0
	if (a_crate->do_pedestals) {
		struct Module **module_ref;

		VECTOR_FOREACH(module_ref, &a_tag->module_ref_vec) {
			struct Module *module;

			module = *module_ref;
			if (NULL != module->props) {
				push_log_level(module);
				module->props->zero_suppress(module, 1);
				pop_log_level(module);
			}
		}
		a_tag->do_pedestals = 0;
	}
#endif
	/* Latch all counters for next readout. */
	TAILQ_FOREACH(counter, &a_crate->counter_list, next) {
		counter->prev = counter->cur.value;
	}
	if (STATE_REINIT == a_crate->state) {
		/*
		 * If the readout failed, we must re-init to clear all modules
		 * to make sure upcoming triggers are not blocked by module
		 * dt.
		 */
		log_error(LOGL, "%s: had problems, re-initializing.",
		    a_crate->name);
		crate_deinit(a_crate);
		time_sleep(a_crate->reinit_sleep_s);
		crate_init(a_crate);
		/*
		 * If dt is already released, we might have accepted garbage
		 * data so we must re-init also in 'prepare' to be sure.
		 * TODO: 'clear' method for modules to avoid double-init?
		 */
		if (!crate_dt_is_on(a_crate)) {
			a_crate->state =  STATE_REINIT;
		}
	} else if (crate_dt_is_on(a_crate)) {
		struct Module **module_ref;
		int do_v1n90, do_pex;

		/*
		 * Re-config, early dt release has been inhibited so we should
		 * be safe here.
		 */
		do_v1n90 = 0;
		do_pex = 0;
		VECTOR_FOREACH(module_ref, &a_crate->module_configed_vec) {
			struct Module *module;

			module = *module_ref;
			do_v1n90 |=
			    KW_CAEN_V1190 == module->type ||
			    KW_CAEN_V1290 == module->type;
			do_pex |=
			    KW_GSI_CTDC == module->type ||
			    KW_GSI_FEBEX == module->type ||
			    KW_GSI_TAMEX == module->type;
		}
		THREAD_MUTEX_LOCK(&a_crate->mutex);
#if 0
		/* TODO: Why was this here? */
		if (do_pex) {
			/* TODO: And shouldn't we test if a pex exists? */
			gsi_pex_init(a_crate->gsi_pex.pex,
			    a_crate->gsi_pex.config);
		}
#else
		(void)do_pex;
#endif
		VECTOR_FOREACH(module_ref, &a_crate->module_configed_vec) {
			struct Module *module;

			module = *module_ref;
			LOGF(info)(LOGL, "%s[%u]=%s re-config.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			a_crate->module_init_id = module->id;
			if (!module->props->init_fast(a_crate, module)) {
				a_crate->state = STATE_REINIT;
			}
			module_init_id_mark(a_crate, module);
			/* TODO: Should we post-init? */
			if (!module->props->post_init(a_crate, module)) {
				a_crate->state = STATE_REINIT;
			}
			module_counter_latch(module);
		}
		module_init_id_clear(a_crate);
		VECTOR_FREE(&a_crate->module_configed_vec);
		if (do_v1n90) {
			if (!caen_v1n90_micro_init_fast(
			    &a_crate->module_list)) {
				a_crate->state = STATE_REINIT;
			}
		}
		thread_mutex_unlock(&a_crate->mutex);
	}
	LOGF(spam)(LOGL, "crate_readout_finalize(%s) }", a_crate->name);
}

void
crate_register_array_pack(struct PackerList *a_list, int a_crate_i, int
    a_module_j, int a_submodule_k)
{
	struct Crate *crate;
	struct Module *module;

	LOGF(debug)(LOGL, "crate_register_array_pack(cr=%d,mod=%d,sub=%d) {",
	    a_crate_i, a_module_j, a_submodule_k);
	crate = get_crate(a_crate_i);
	if (NULL == crate) {
		PACKER_LIST_PACK(*a_list, 16, -1);
		PACKER_LIST_PACK_LOC(*a_list);
		PACKER_LIST_PACK_STR(*a_list, "Crate not found");
		goto crate_register_array_pack_done;
	}
	module = get_module(crate, a_module_j);
	if (NULL == module) {
		PACKER_LIST_PACK(*a_list, 16, -1);
		PACKER_LIST_PACK_LOC(*a_list);
		PACKER_LIST_PACK_STR(*a_list, "Module not found");
		goto crate_register_array_pack_done;
	}
	THREAD_MUTEX_LOCK(&crate->mutex);
	module_register_list_pack(a_list, module, a_submodule_k);
	thread_mutex_unlock(&crate->mutex);
crate_register_array_pack_done:
	LOGF(debug)(LOGL, "crate_register_array_pack }");
}

void
crate_scaler_add(struct Crate *a_crate, char const *a_name, struct Module
    *a_module, void *a_data, ScalerGetCallback a_callback)
{
	struct CrateScaler *scaler;

	LOGF(debug)(LOGL, "crate_scaler_add(cr=%s,name=%s,modid=%u) {",
	    a_crate->name, a_name, a_module->id);
	CALLOC(scaler, 1);
	strlcpy_(scaler->name, a_name, sizeof scaler->name);
	scaler->module = a_module;
	scaler->data = a_data;
	scaler->get_counter = a_callback;
	TAILQ_INSERT_TAIL(&a_crate->scaler_list, scaler, next);
	LOGF(debug)(LOGL, "crate_scaler_add }");
}

void
crate_setup(void)
{
	config_auto_register(KW_CRATE, "crate.cfg");
	config_auto_register(KW_TAGS, "tags.cfg");
}

uint32_t
check_empty(struct Crate *a_crate)
{
	struct Module *module;
	uint32_t result;

	LOGF(spam)(LOGL, "check_empty(%s) {", a_crate->name);
	result = 0;
	/* TODO: Mutex is-locked check! */
/*
	if (!thread_mutex_is_locked(&a_crate->mutex)) {
		log_die(LOGL, "crate_empty(%s): Must be mutexed!",
		    a_crate->name);
	}
*/

	TAILQ_FOREACH(module, &a_crate->module_list, next) {
		uint32_t ret;

		if (NULL == module->props) {
			continue;
		}
		push_log_level(module);
		ret = module->props->check_empty(module);
		if (0 != ret) {
			log_error(LOGL, "%s:%u=%s: not empty.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			a_crate->state = STATE_REINIT;
			result |= ret;
		}
		pop_log_level(module);
	}

	LOGF(spam)(LOGL, "check_empty(%s=0x%08x) }", a_crate->name, result);
	return result;
}

int
crate_sync_get(struct Crate *a_crate, unsigned a_i, int *a_value)
{
	int ret;

	LOGF(spam)(LOGL, "crate_sync_get(%s:sync_n=%"PRIz",%u) {",
	    a_crate->name, a_crate->sync.num, a_i);
	if (a_i < a_crate->sync.num) {
		*a_value = a_crate->sync.array[a_i];
		ret = 1;
	} else {
		ret = 0;
	}
	LOGF(spam)(LOGL, "crate_sync_get }");
	return ret;
}

void
crate_sync_push(struct Crate *a_crate, int a_value)
{
	LOGF(spam)(LOGL, "crate_sync_push(%s:sync_n=%"PRIz",%d) {",
	    a_crate->name, a_crate->sync.num, a_value);
	if (a_crate->sync.num >= LENGTH(a_crate->sync.array)) {
		log_die(LOGL, "Too many sync values (%"PRIz"), "
		    "don't be greedy!", a_crate->sync.num);
	}
	a_crate->sync.array[a_crate->sync.num++] = a_value;
	LOGF(spam)(LOGL, "crate_sync_push }");
}

void
crate_tag_counter_increase(struct Crate *a_crate, struct CrateTag *a_tag,
    unsigned a_inc)
{
	struct CrateCounter **counter_ref;

	LOGF(spam)(LOGL, "crate_tag_counter_increase(%s:%s,%u) {",
	    a_crate->name, a_tag->name, a_inc);
	if (a_tag->gsi_pex_is_needed && 0 != a_inc) {
		gsi_pex_readout_prepare(a_crate->gsi_pex.pex);
	}
	VECTOR_FOREACH(counter_ref, &a_tag->counter_ref_vec) {
		struct CrateCounter *counter;

		counter = *counter_ref;
		counter->cur.value += a_inc;
	}
	LOGF(spam)(LOGL, "crate_tag_counter_increase(%s:%s) }", a_crate->name,
	    a_tag->name);
}

unsigned
crate_tag_get_event_max(struct CrateTag const *a_tag)
{
	return a_tag->event_max;
}

char const *
crate_tag_get_name(struct CrateTag const *a_tag)
{
	return a_tag->name;
}

int
crate_tag_gsi_pex_is_needed(struct CrateTag const *a_tag)
{
	return a_tag->gsi_pex_is_needed;
}

size_t
crate_tag_get_module_num(struct CrateTag const *a_tag)
{
	return a_tag->module_num;
}

#if 0
void
crate_tag_pedestal_prepare(struct CrateTag *a_tag)
{
	struct Module *module;

	LOGF(spam)(LOGL, "crate_tag_pedestal_prepare(%s) {", a_tag->name);
	/*
	 * TODO: Tag state?
	 */
	if (a_tag->do_pedestals) {
		log_die(LOGL, "crate_tag_pedestal_prepare(%s) called "
		    "twice before pedestal readout.", a_tag->name);
	}
	TAILQ_FOREACH(module, &a_tag->module_list, next) {
		if (NULL != module->props) {
			push_log_level(module);
			module->props->zero_suppress(module, 0);
			pop_log_level(module);
		}
	}
	a_tag->do_pedestals = 1;
	LOGF(spam)(LOGL, "crate_tag_pedestal_prepare(%s) }", a_tag->name);
}

void
crate_tag_pedestal_update(struct CrateTag *a_tag)
{
	struct Module *module;

	LOGF(spam)(LOGL, "crate_pedestal_update(%s) {", a_tag->name);
	assert_prepared(a_tag, "crate_tag_pedestal_update");
	TAILQ_FOREACH(module, &a_tag->module_list, next) {
		if (NULL != module->pedestal.array) {
			size_t i;

			for (i = 0; module->pedestal.array_len > i; ++i) {
				int dummy;

				dummy = module_pedestal_calculate(
				    &module->pedestal.array[i]);
				(void)dummy;
			}
			push_log_level(module);
			module->props->use_pedestals(module);
			pop_log_level(module);
		}
	}
	LOGF(spam)(LOGL, "crate_pedestal_update(%s) }", a_tag->name);
}
#endif

void
dt_release(struct Crate *a_crate)
{
	LOGF(spam)(LOGL, "dt_release(%s) {", a_crate->name);
	if (a_crate->dt_release.do_it &&
	    !a_crate->dt_release.do_inhibit &&
	    NULL != a_crate->dt_release.func) {
		LOGF(spam)(LOGL, "dt_release.func {");
		a_crate->dt_release.func(a_crate->dt_release.data);
		a_crate->dt_release.is_on = 0;
		LOGF(spam)(LOGL, "dt_release.func }");
	} else {
		a_crate->dt_release.is_on = 1;
		a_crate->dt_release.do_inhibit = 0;
	}
	LOGF(spam)(LOGL, "dt_release(%s,dt=%s) }", a_crate->name,
	    a_crate->dt_release.is_on ? "on" : "off");
}

struct CrateCounter *
get_counter(struct Crate *a_crate, char const *a_name)
{
	struct CrateCounter *counter;

	counter = crate_counter_get(a_crate, a_name);
	if (NULL == counter) {
		CALLOC(counter, 1);
		if (NULL != a_name) {
			strlcpy_(counter->name, a_name, sizeof counter->name);
		}
		TAILQ_INSERT_TAIL(&a_crate->counter_list, counter, next);
	}
	return counter;
}

struct Crate *
get_crate(unsigned a_crate_i)
{
	struct Crate *crate;
	unsigned i;

	crate = TAILQ_FIRST(&g_crate_list);
	for (i = 0; a_crate_i > i; ++i) {
		crate = TAILQ_NEXT(crate, next);
		if (TAILQ_END(&g_crate_list) == crate) {
			return NULL;
		}
	}
	return crate;
}

struct Module *
get_module(struct Crate *a_crate, unsigned a_module_j)
{
	struct Module *module;
	unsigned k;

	module = TAILQ_FIRST(&a_crate->module_list);
	for (k = 0; a_module_j > k; ++k) {
		module = TAILQ_NEXT(module, next);
		if (TAILQ_END(&a_crate->module_list) == module) {
			return NULL;
		}
	}
	return module;
}

void
module_counter_latch(struct Module *a_module)
{
	a_module->this_minus_crate = a_module->event_counter.value -
	    a_module->crate_counter->value;
	a_module->shadow.data_counter_value = a_module->event_counter.value;
	LOGF(verbose)(LOGL, "[%u]=%s this(0x%08x)-crate(0x%08x)=0x%08x.",
	    a_module->id, keyword_get_string(a_module->type),
	    a_module->event_counter.value, a_module->crate_counter->value,
	    a_module->this_minus_crate);
}

void
module_init_id_clear(struct Crate *a_crate)
{
	LOGF(spam)(LOGL, "module_init_id_clear {");
	while (!TAILQ_EMPTY(&a_crate->module_init_id_list)) {
		struct ModuleID *p;

		p = TAILQ_FIRST(&a_crate->module_init_id_list);
		TAILQ_REMOVE(&a_crate->module_init_id_list, p, next);
		FREE(p);
	}
	LOGF(spam)(LOGL, "module_init_id_clear }");
}

void
module_init_id_mark(struct Crate *a_crate, struct Module const *a_module)
{
	struct ModuleID *p;

	LOGF(debug)(LOGL, "module_init_id_mark {");
	TAILQ_FOREACH(p, &a_crate->module_init_id_list, next) {
		if (a_crate->module_init_id == p->id) {
			log_die(LOGL, "Module id=%u mapped to id=%u has "
			    "already been taken! Check your modules that "
			    "force their own ID!",
			    a_module->id, a_crate->module_init_id);
		}
	}
	MALLOC(p, 1);
	p->id = a_crate->module_init_id;
	TAILQ_INSERT_TAIL(&a_crate->module_init_id_list, p, next);
	LOGF(debug)(LOGL, "module_init_id_mark }");
}

void
module_insert(struct Crate *a_crate, struct TagRefVector *a_active_vec, struct
    Module *a_module)
{
	struct CrateTag **tag_ref;

	LOGF(debug)(LOGL, "module_insert {");
	TAILQ_INSERT_TAIL(&a_crate->module_list, a_module, next);
	VECTOR_FOREACH(tag_ref, a_active_vec) {
		struct CrateTag *tag;

		tag = *tag_ref;
		VECTOR_APPEND(&tag->module_ref_vec, a_module);
		if (KW_BARRIER != a_module->type) {
			++tag->module_num;
		}
	}
	LOGF(debug)(LOGL, "module_insert }");
}

void
pop_log_level(struct Module const *a_module)
{
	if (NULL != a_module->log_level) {
		log_level_pop();
	}
}

void
push_log_level(struct Module const *a_module)
{
	if (NULL != a_module->log_level) {
		struct LogLevel const *level;

		level = log_level_is_visible(a_module->log_level) ?
		    log_level_get() : a_module->log_level;
		log_level_push(level);
	}
}

uint32_t
read_module(struct Crate *a_crate, struct Module *a_module, struct EventBuffer
    *a_event_buffer)
{
	struct EventBuffer eb_orig;
	struct EventConstBuffer ceb;
	uint32_t result;

	LOGF(spam)(LOGL, "%s[%u]=%s: Crate = 0x%08x->0x%08x/%u",
	    a_crate->name, a_module->id, keyword_get_string(a_module->type),
	    a_module->crate_counter_prev, a_module->crate_counter->value,
	    bits_get_count(a_module->crate_counter->mask));
	if (!a_crate->is_free_running &&
	    0 == COUNTER_DIFF_RAW(*a_module->crate_counter,
	    a_module->crate_counter_prev)) {
		/*
		 * This module shouldn't have seen anything.
		 * We use the crate counter rather than the module counter,
		 * because some modules don't have a proper event counter, so
		 * we'll have to rely on the user code correctly increasing
		 * counters (via scalers or explicitly).
		 */
		return 0;
	}

	push_log_level(a_module);
	COPY(eb_orig, *a_event_buffer);
	result = a_module->props->readout(a_crate, a_module, a_event_buffer);
	EVENT_BUFFER_INVARIANT(*a_event_buffer, eb_orig);
	ceb.ptr = eb_orig.ptr;
	ceb.bytes = eb_orig.bytes - a_event_buffer->bytes;
	if (0 != result) {
		log_error(LOGL, "%s[%u]=%s readout error=0x%08x, dumping "
		    "data:", a_crate->name, a_module->id,
		    keyword_get_string(a_module->type), result);
	} else {
		result = a_module->props->parse_data(a_crate, a_module, &ceb,
		    0);
		if (0 != result) {
			log_error(LOGL, "%s[%u]=%s parse error=0x%08x,"
			    " dumping data:", a_crate->name, a_module->id,
			    keyword_get_string(a_module->type), result);
		}
	}
	pop_log_level(a_module);
	a_module->result |= result;
	COPY(a_module->eb_final, ceb);
	if (0 != result) {
		log_dump(LOGL, ceb.ptr, ceb.bytes);
		a_crate->state = STATE_REINIT;
	}

	a_module->crate_counter_prev = a_module->crate_counter->value;

	return result;
}

void
shadow_func(void *a_data)
{
	struct Crate *crate;
	struct Module *module;

	/*
	 * NOTE: Be careful with log blocks in here and in all
	 * module->readout_shadow implementations, other threads may change
	 * the global log level, so log indentation is NOT guaranteed.
	 */

	crate = a_data;

	/* Prepare modules. */
	TAILQ_FOREACH(module, &crate->module_list, next) {
		if (NULL == module->props ||
		    NULL == module->props->readout_shadow) {
			continue;
		}
		if (!thread_mutex_init(&module->shadow.buf_mutex)) {
			log_die(LOGL, "Could not initialize module mutex.");
		}
		module->shadow.buf_idx = 0;
		ZERO(module->shadow.buf);
	}

	/* Emptying is the sole purpose of my existence. */
	while (crate->shadow.is_running) {
		if (STATE_REINIT == crate->state) {
			goto next;
		}

		THREAD_MUTEX_LOCK(&crate->mutex);
		if (crate->shadow.do_buf_rebuild) {
			size_t ofs, step;

			ofs = 0;
			step = (crate->shadow.buf_bytes / (2 *
			    crate->shadow.module_readable_num) /
			    sizeof(uint32_t)) * sizeof(uint32_t);
			/* Prepare modules. */
			TAILQ_FOREACH(module, &crate->module_list, next) {
				size_t i;

				if (NULL == module->props ||
				    NULL == module->props->readout_shadow) {
					continue;
				}
				for (i = 0; i < LENGTH(module->shadow.buf);
				    ++i) {
					struct ModuleShadowBuffer *buf;

					buf = &module->shadow.buf[i];
					buf->store.ptr =
					    (uint8_t *)map_blt_dst_get(
						crate->shadow.dst) + ofs;
					buf->store.bytes = step;
					COPY(buf->eb, buf->store);
					ofs += step;
				}
			}
			crate->shadow.do_buf_rebuild = 0;
		}
		TAILQ_FOREACH(module, &crate->module_list, next) {
			struct ModuleShadowBuffer *buf;
			uint32_t ret;

			if (0 == module->event_max ||
			    NULL == module->props->readout_shadow ||
			    0 == module->shadow.buf[0].eb.bytes) {
				continue;
			}
			THREAD_MUTEX_LOCK(&module->shadow.buf_mutex);
			buf = &module->shadow.buf[module->shadow.buf_idx];
			ret = module->props->readout_shadow(crate, module,
			    &buf->eb);
			module->result |= ret;
			thread_mutex_unlock(&module->shadow.buf_mutex);
			if (0 != ret) {
				/*
				 * TODO: Since we reinit we must clear
				 * the shadow buffer due to reset
				 * counters which will kill parse_data
				 * (if they're properly written...),
				 * but we lose data :/
				 * Hmm...
				 */
				COPY(buf->eb, buf->store);
				crate->state = STATE_REINIT;
			}
			EVENT_BUFFER_INVARIANT(buf->store, buf->eb);
		}
		thread_mutex_unlock(&crate->mutex);

next:
		sched_yield();
	}
	TAILQ_FOREACH(module, &crate->module_list, next) {
		if (NULL == module->props ||
		    NULL == module->props->readout_shadow) {
			thread_mutex_clean(&module->shadow.buf_mutex);
		}
	}
}

uint32_t
shadow_merge_module(struct Crate *a_crate, struct Module *a_module, struct
    EventBuffer *a_event_buffer)
{
	struct ModuleShadowBuffer *old;
	size_t old_idx;
	uint32_t result, ret;
	unsigned bytes;

	LOGF(spam)(LOGL, "shadow_merge_module(%s[%u]=%s) {", a_crate->name,
	    a_module->id, keyword_get_string(a_module->type));
	result = 0;

	old_idx = 1 ^ a_module->shadow.buf_idx;
	old = &a_module->shadow.buf[old_idx];
	bytes = old->store.bytes - old->eb.bytes;
	LOGF(spam)(LOGL, "Shadow-buf: idx=%"PRIz" ptr=%p bytes=0x%08x",
	    old_idx, old->store.ptr, bytes);

	/* Copy into dst event buffer. */
	if (a_event_buffer->bytes < bytes) {
		log_error(LOGL, "%s[%u]=%s: Too much data! "
		    "Dst=0x%08"PRIzx" B < src=0x%08x B.",
		    a_crate->name, a_module->id,
		    keyword_get_string(a_module->type), a_event_buffer->bytes,
		    bytes);
		result |= CRATE_READOUT_FAIL_DATA_TOO_MUCH;
		goto shadow_merge_module_done;
	}
	memcpy_(a_event_buffer->ptr, old->store.ptr, bytes);
	a_module->eb_final.ptr = a_event_buffer->ptr;
	a_module->eb_final.bytes = bytes;
	EVENT_BUFFER_ADVANCE(*a_event_buffer, (uint8_t *)a_event_buffer->ptr +
	    bytes);

	/* Check the data. */
	ret = a_module->props->parse_data(a_crate, a_module,
	    &a_module->eb_final, 0);
	result |= ret;
	if (0 != ret) {
		log_error(LOGL, "%s:%u=%s parse error=0x%08x, dumping data:",
		    a_crate->name, a_module->id,
		    keyword_get_string(a_module->type), ret);
		log_dump(LOGL, a_module->eb_final.ptr,
		    a_module->eb_final.bytes);
	}

	a_crate->shadow.max_bytes = MAX(a_crate->shadow.max_bytes, bytes);

shadow_merge_module_done:
	LOGF(spam)(LOGL, "shadow_merge_module(%s[%u]=%s:%08x) }",
	    a_crate->name, a_module->id, keyword_get_string(a_module->type),
	    result);
	return result;
}

/* Compares the signature of two modules. */
int
signature_match(struct Module const *a_left, struct Module const *a_right)
{
	struct ModuleSignature const *sigarr_l;
	struct ModuleSignature const *sigarr_r;
	size_t sign_l, sign_r;
	size_t i_l, i_r;
	int do_match;

	a_left->props->get_signature(&sigarr_l, &sign_l);
	a_right->props->get_signature(&sigarr_r, &sign_r);

	do_match = 0;
	for (i_l = 0; i_l < sign_l; ++i_l) {
		struct ModuleSignature const *sig_l;

		sig_l = &sigarr_l[i_l];
		for (i_r = 0; i_r < sign_r; ++i_r) {
			struct ModuleSignature const *sig_r;
			uint32_t mask;

			sig_r = &sigarr_r[i_r];
			if (sig_l->id_mask == sig_r->id_mask) {
				continue;
			}
			mask = sig_l->fixed_mask & sig_r->fixed_mask;
			if ((mask & sig_l->fixed_value) !=
			    (mask & sig_r->fixed_value)) {
				continue;
			}
			do_match = 1;
		}
	}
	return do_match;
}

struct CrateTag *
tag_get(struct Crate *a_crate, char const *a_name)
{
	struct CrateTag *tag;

	/* Find or create the tag in the crate. */
	TAILQ_FOREACH(tag, &a_crate->tag_list, next) {
		if (0 == strcmp(tag->name, a_name)) {
			return tag;
		}
	}
	CALLOC(tag, 1);
	strlcpy_(tag->name, a_name, sizeof tag->name);
	TAILQ_INSERT_TAIL(&a_crate->tag_list, tag, next);
	return tag;
}

#if NCONF_mMAP_bCMVLC
void
crate_cmvlc_init(struct Crate *a_crate, struct cmvlc_stackcmdbuf *a_stack,
    int a_dt)
{
	struct Module *module;

	LOGF(info)(LOGL, "crate_cmvlc_init(%s,%d) {", a_crate->name, a_dt);

	TAILQ_FOREACH(module, &a_crate->module_list, next) {

		if (NULL == module->props ||
		    NULL == module->props->cmvlc_init) {
			log_error(LOGL, "%s[%u]=%s no cmvlc_init.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			/* This really should return an error! */
			continue;
		}
		push_log_level(module);
		module->props->cmvlc_init(module, a_stack, a_dt);
		pop_log_level(module);
		cmvlc_stackcmd_marker(a_stack,
		    (a_dt ? 0xfeed0000 : 0xabba0000) ^ (module->id + 1));
	}

	LOGF(info)(LOGL, "crate_cmvlc_init }");
}

/* TODO: also send along pointer and size of *full* MVLC sequence
 * buffer such that dump can show everything, also stuff that may have
 * been consumed before we got called.
 */
uint32_t
crate_cmvlc_fetch_dt(struct Crate *a_crate,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct Module *module;
	uint32_t result;
	const uint32_t *in_buffer;
	uint32_t in_remain;
	uint32_t used;
	uint32_t expect;

	LOGF(spam)(LOGL, "crate_cmvlc_fetch_dt(%s) {", a_crate->name);
	result = 0;
	/* Remember full buffer. */
	in_buffer = a_in_buffer;
	in_remain = a_in_remain;

	*a_in_used = 0;

	TAILQ_FOREACH(module, &a_crate->module_list, next) {

		if (NULL == module->props ||
		    NULL == module->props->cmvlc_fetch_dt) {
			log_error(LOGL, "%s[%u]=%s no cmvlc_fetch_dt.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			continue;
		}
		push_log_level(module);
		result |= module->props->cmvlc_fetch_dt(module,
		    a_in_buffer, a_in_remain, &used);
		pop_log_level(module);

		/* An error means that the packaging is out of sync.
		 * Do not try to decode further modules.
		 */
		if (0 != result) {
			log_error(LOGL, "%s[%u]=%s cmvlc fetch_dt"
			    " error=0x%08x,"
			    " dumping data:", a_crate->name, module->id,
			    keyword_get_string(module->type), result);
			log_dump(LOGL, in_buffer,
			    in_remain * sizeof (uint32_t));
			a_crate->state = STATE_REINIT;
			goto done;
		}

		a_in_buffer += used;
		a_in_remain -= used;
		*a_in_used += used;

		if (0 == a_in_remain) {
			log_error(LOGL, "%s[%u]=%s too few words for "
			    "post-module marker in cmvlc dt data.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto done;
		}

		expect = 0xfeed0000 ^ (module->id + 1);

		if (expect != *a_in_buffer) {
			log_error(LOGL, "%s[%u]=%s bad "
			    "post-module marker in cmvlc dt data, "
			    "expect 0x%08x != got 0x%08x.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type),
			    expect, *a_in_buffer);
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto done;
		}

		a_in_buffer++;
		a_in_remain--;
		(*a_in_used)++;
	}

done:
	LOGF(spam)(LOGL, "crate_cmvlc_fetch_dt }");
	return result;
}

/* TODO: also send along pointer and size of *full* MVLC sequence
 * buffer such that dump can show everything, also stuff that may have
 * been consumed before we got called.
 */
uint32_t
crate_cmvlc_fetch(struct Crate *a_crate, struct EventBuffer *a_event_buffer,
    const uint32_t *a_in_buffer, uint32_t a_in_remain, uint32_t *a_in_used)
{
	struct Module *module;
	struct EventBuffer eb_orig;
	struct EventConstBuffer ceb;
	uint32_t result;
	const uint32_t *in_buffer;
	uint32_t in_remain;
	uint32_t used;
	uint32_t expect;

	LOGF(spam)(LOGL, "crate_cmvlc_fetch(%s) {", a_crate->name);
	result = 0;
	/* Remember full buffer. */
	in_buffer = a_in_buffer;
	in_remain = a_in_remain;

	*a_in_used = 0;

	TAILQ_FOREACH(module, &a_crate->module_list, next) {

		if (NULL == module->props ||
		    NULL == module->props->cmvlc_fetch) {
			log_error(LOGL, "%s[%u]=%s no cmvlc_fetch.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			continue;
		}
		push_log_level(module);
		COPY(eb_orig, *a_event_buffer);
		result |= module->props->cmvlc_fetch(a_crate, module,
		    a_event_buffer, a_in_buffer, a_in_remain, &used);
		EVENT_BUFFER_INVARIANT(*a_event_buffer, eb_orig);
		ceb.ptr = eb_orig.ptr;
		ceb.bytes = eb_orig.bytes - a_event_buffer->bytes;
		COPY(module->eb_final, ceb);
		pop_log_level(module);

		/* An error means that the packaging is out of sync.
		 * Do not try to decode further modules.
		 */
		if (0 != result) {
			log_error(LOGL, "%s[%u]=%s cmvlc fetch error=0x%08x,"
			    " dumping data:", a_crate->name, module->id,
			    keyword_get_string(module->type), result);
			log_dump(LOGL, in_buffer,
			    in_remain * sizeof (uint32_t));
			a_crate->state = STATE_REINIT;
			goto done;
		}

		result = module->props->parse_data(a_crate, module, &ceb,
		    0);
		if (0 != result) {
			log_error(LOGL, "%s[%u]=%s parse error=0x%08x,"
			    " dumping data:", a_crate->name, module->id,
			    keyword_get_string(module->type), result);
		}

		module->crate_counter_prev = module->crate_counter->value;

		a_in_buffer += used;
		a_in_remain -= used;
		*a_in_used += used;

		if (0 == a_in_remain) {
			log_error(LOGL, "%s[%u]=%s too few words for "
			    "post-module marker in cmvlc data.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type));
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto done;
		}

		expect = 0xabba0000 ^ (module->id + 1);

		if (expect != *a_in_buffer) {
			log_error(LOGL, "%s[%u]=%s bad "
			    "post-module marker in cmvlc data, "
			    "expect 0x%08x != got 0x%08x.",
			    a_crate->name, module->id,
			    keyword_get_string(module->type),
			    expect, *a_in_buffer);
			result |= CRATE_READOUT_FAIL_ERROR_DRIVER;
			goto done;
		}

		a_in_buffer++;
		a_in_remain--;
		(*a_in_used)++;
	}

done:
	LOGF(spam)(LOGL, "crate_cmvlc_fetch }");
	return result;
}
#endif
