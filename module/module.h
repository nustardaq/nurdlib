/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2025
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

#ifndef MODULE_MODULE_H
#define MODULE_MODULE_H

#include <stdlib.h>
#include <nurdlib/base.h>
#include <nurdlib/log.h>
#include <util/assert.h>
#include <util/funcattr.h>
#include <util/queue.h>
#include <util/stdint.h>
#include <util/thread.h>

#define MODULE_CAST(type_kw, dst, src) do {\
	ASSERT(int, "d", type_kw, ==, (src)->type);\
	dst = (void *)src;\
} while (0)

#define MODULE_CREATE(var)\
	var = (void *)module_create_base(sizeof *var, &g_module_props_)

#define MODULE_CREATE_PEDESTALS(var, name, len) do {\
	struct Module *module_;\
	module_ = (void *)var;\
	module_->pedestal.array_len = len;\
	CALLOC(module_->pedestal.array, len);\
} while (0)

#define MODULE_CALLBACK_BIND(name, func) g_module_props_.func = name##_##func

#define MODULE_INTERFACE(name) \
struct ConfigBlock;\
struct Module	*name##_create_(struct Crate *, struct ConfigBlock *) \
	FUNC_RETURNS;\
void		name##_setup_(void)

#define MODULE_PROTOTYPES(name)\
static uint32_t		name##_check_empty(struct Module *);\
static void		name##_deinit(struct Module *);\
static void		name##_destroy(struct Module *);\
static struct Map	*name##_get_map(struct Module *);\
static void		name##_get_signature(struct ModuleSignature const **,\
    size_t *);\
static int		name##_init_fast(struct Crate *, struct Module *);\
static int		name##_init_slow(struct Crate *, struct Module *);\
static void		name##_memtest(struct Module *, enum Keyword);\
static uint32_t		name##_parse_data(struct Crate *, struct Module\
    *, struct EventConstBuffer const *, int);\
static uint32_t		name##_readout(struct Crate *, struct Module *, \
    struct EventBuffer *);\
static uint32_t		name##_readout_dt(struct Crate *, struct Module *);\
static struct ModuleProps g_module_props_

#define MODULE_SETUP(name, a_flags) \
	MODULE_CALLBACK_BIND(name, check_empty);\
	MODULE_CALLBACK_BIND(name, deinit);\
	MODULE_CALLBACK_BIND(name, destroy);\
	MODULE_CALLBACK_BIND(name, get_map);\
	MODULE_CALLBACK_BIND(name, get_signature);\
	MODULE_CALLBACK_BIND(name, init_fast);\
	MODULE_CALLBACK_BIND(name, init_slow);\
	MODULE_CALLBACK_BIND(name, memtest);\
	MODULE_CALLBACK_BIND(name, parse_data);\
	MODULE_CALLBACK_BIND(name, readout);\
	MODULE_CALLBACK_BIND(name, readout_dt);\
	g_module_props_.flags = a_flags

#define STRUCT_PADDING(start, next) uint8_t pad_##start##_##next[next - start]

#define MODULE_SIGNATURE_BEGIN do { \
	static const struct ModuleSignature s_sign_[] = {
#define MODULE_SIGNATURE(id_mask, fixed_mask, fixed_value) \
	{ id_mask, fixed_mask, fixed_value },
#define MODULE_SIGNATURE_END(array, num) \
	{0, 0, 0}}; \
	*array = s_sign_; \
	*num = LENGTH(s_sign_) - 1; \
} while (0);

enum {
	MODULE_FLAG_EARLY_DT = 1
};

struct ConfigBlock;
struct Crate;
struct Module;
struct Packer;
struct PackerList;
struct Pedestal;
struct cmvlc_stackcmdbuf;

struct ModuleSignature {
	uint32_t	id_mask;
	uint32_t	fixed_mask;
	uint32_t	fixed_value;
};

struct ModuleProps {
	/*
	 * 'check_empty' checks if the given module buffers contain any data.
	 *  return 0 = empty, otherwise crate readout error bitmask.
	 */
	uint32_t	(*check_empty)(struct Module *) FUNC_RETURNS;
	/*
	 * 'deinit' undoes 'init', i.e. releases hardware resources.
	 */
	void	(*deinit)(struct Module *);
	/*
	 * 'destroy' undoes 'create_', i.e. releases soft resources.
	 */
	void	(*destroy)(struct Module *);
	/*
	 * 'get_submodule_config' returns a pointer to the config block used
	 * for a sub-module of a module, e.g. one clock TDC board (sub-module)
	 * in a clock TDC SFP chain (module).
	 */
	struct	ConfigBlock *(*get_submodule_config)(struct Module *,
	    unsigned);
	/*
	 * 'get_map' returns the map for simple accesses, eg single-cycle if
	 * this is a VME module.
	 */
	struct	Map *(*get_map)(struct Module *) FUNC_RETURNS;
	/*
	 * 'get_signature' returns the mask of the module ID, and the
	 * mask-value bitmasks of fixed bits of the first word in the module
	 * payload. Note that several such pairs can be returned.
	 * This is used by the crate to determine if two adjacent modules
	 * (depending on active tags) require a barrier to distinguish them.
	 */
	void	(*get_signature)(struct ModuleSignature const **, size_t *);
	/*
	 * 'init_fast' runs "fast" initialization of the module hardware,
	 * always after 'init_slow' but also after a nurdctrl re-config.
	 * Can run the full hw init if the hw is fast enough which allows for
	 * most flexibility, but painfully slow hw should have split constant
	 * and online re-configurable init steps.
	 *  return 0 = failed.
	 */
	int	(*init_fast)(struct Crate *, struct Module *) FUNC_RETURNS;
	/*
	 * 'init_slow' should do very slow init steps that are not needed
	 * after online re-configuring.
	 *  return 0 = failed.
	 */
	int	(*init_slow)(struct Crate *, struct Module *) FUNC_RETURNS;
	/*
	 * 'memtest' runs a memory test on the module memory (if supported).
	 * The argument specifies how thoroughly the modules should be
	 * tested.
	 */
	void	(*memtest)(struct Module *, enum Keyword);
	/*
	 * 'post_init' is run after the 'init' cycle for all modules has been
	 * completed, and does not need to be implemented by every module.
	 *  return 0 = failed.
	 */
	int	(*post_init)(struct Crate *, struct Module *) FUNC_RETURNS;
	/*
	 * 'parse_data' parses and verifies, to a reasonable degree for
	 * run-time, the contents of the given buffer of given number of
	 * bytes.
	 * Note that the content may be fragmented between calls, but must be
	 * in sequence. This should be expected for live-time readout, e.g.
	 * when doing shadow readout. The parsing code should be ready to keep
	 * state in the module struct between calls.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*parse_data)(struct Crate *, struct Module *, struct
	    EventConstBuffer const *, int) FUNC_RETURNS;
	/*
	 * 'readout' reads module buffers into the given buffer which is
	 * advanced accordingly.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*readout)(struct Crate *, struct Module *, struct
	    EventBuffer *) FUNC_RETURNS;
	/*
	 * 'readout_dt' reads data that must always be done inside dead-time,
	 * e.g. counters or status. Anything useful for 'readout' should be
	 * stored in the module-specific struct.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*readout_dt)(struct Crate *, struct Module *)
	    FUNC_RETURNS;
	/*
	 * 'readout_shadow' reads any available module buffer content into the
	 * given buffer, only used for shadow readout.
	 * NOTE: Do _NOT_ rely on log indentation! I.e. never end a log msg
	 * with '{' or '}'.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*readout_shadow)(struct Crate *, struct Module *,
	    struct EventBuffer *) FUNC_RETURNS;
	/*
	 * 'register_list_pack' does custom reading of hardware registers,
	 * when the built-in nurdlib approach doesn't cut it.
	 */
	int	(*register_list_pack)(struct Module *, struct PackerList *)
	    FUNC_RETURNS;
	/*
	 * 'sub_module_pack' packs information about sub-modules into the
	 * given packer.
	 */
	int	(*sub_module_pack)(struct Module *, struct PackerList *)
	    FUNC_RETURNS;
	/*
	 * 'use_pedestals' is called when the module pedestal array has been
	 * updated. This is called only for modules created with
	 * MODULE_CREATE_PEDESTALS.
	 */
	void	(*use_pedestals)(struct Module *);
	/*
	 * 'zero_suppress' controls zero-suppression for modules, optional.
	 * Generally a good idea to turn off for QDC:s to measure pedestals,
	 * and turn it on for physics.
	 */
	void	(*zero_suppress)(struct Module *, int);
	/*
	 * 'cmvlc_init' adds the VME command for MVLC sequencer
	 * readout.  Either commands that must happen while deadtime
	 * is set 'a_dt=1' or that can happen after deadtime release
	 * 'a_dt=0'.
	 */
	void    (*cmvlc_init)(struct Module *, struct cmvlc_stackcmdbuf *,
	    int);
	/*
	 * 'cmvlc_fetch_dt' gets the module data from the MVLC sequencer
	 * output that was read during dead-time.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*cmvlc_fetch_dt)(struct Module *,
	    const uint32_t *, uint32_t, uint32_t *) FUNC_RETURNS;
	/*
	 * 'cmvlc_fetch' gets the module data from the MVLC sequencer
	 * output into the given buffer which is advanced accordingly.
	 *  return = crate readout fail bits.
	 */
	uint32_t	(*cmvlc_fetch)(struct Crate *, struct Module *,
	    struct EventBuffer *, const uint32_t *, uint32_t, uint32_t *)
	    FUNC_RETURNS;

	/* Bitmask of "MODULE_BIT_*". */
	unsigned	flags;
};

TAILQ_HEAD(ModuleList, Module);
struct ModuleShadowBuffer {
	struct	EventBuffer store;
	struct	EventBuffer eb;
};
struct Module {
	enum	Keyword type;
	struct	ModuleProps const *props;
	unsigned	id;
	unsigned	event_max;
	struct	ConfigBlock *config;
	struct	LogLevel const *log_level;
	struct {
		struct	Mutex buf_mutex;
		size_t	buf_idx;
		struct	ModuleShadowBuffer buf[2];
		uint32_t	data_counter_value;
	} shadow;
	struct {
		size_t	array_len;
		struct	Pedestal *array;
	} pedestal;
	/* Trigger/event counter of module. */
	struct	Counter event_counter;
	/* Counter reference owned by crate. */
	struct	Counter const *crate_counter;
	uint32_t	crate_counter_prev;
	/* Diff of crate_counter - event_counter at init. */
	uint32_t	this_minus_crate;
	/* Accumulated result bits since last readout. */
	uint32_t	result;
	/* Will hold the final location of the event data. */
	struct	EventConstBuffer eb_final;
	TAILQ_ENTRY(Module)	next;
};
struct ModuleListEntry {
	enum	Keyword type;
	void	(*setup)(void);
	struct	Module *(*create)(struct Crate *, struct ConfigBlock *)
	    FUNC_RETURNS;
	char	const *auto_cfg;
};

void					module_access_pack(struct PackerList
    *, struct Packer *, struct Module *, int);
struct Module				*module_create(struct Crate *, enum
    Keyword, struct ConfigBlock *) FUNC_RETURNS;
struct Module				*module_create_base(size_t, struct
    ModuleProps const *) FUNC_RETURNS;
void					module_free(struct Module **);
struct RegisterEntryClient const	*module_register_list_client_get(enum
    Keyword) FUNC_RETURNS;
void					module_register_list_pack(struct
    PackerList *, struct Module *, int);
void					module_setup(void);

/* Common module tools. */

/* Must be 2^n. */
#define PEDESTAL_BUF_LEN 128

#define MODULE_COUNTER_DIFF(module)\
	COUNTER_DIFF(*(module).crate_counter, (module).event_counter,\
	    (module).this_minus_crate)

#define MODULE_SCALER_PARSE(crate, block, module, creator) do {\
		struct ConfigBlock *block_;\
		for (block_ = config_get_block(block, KW_SCALER);\
		    NULL != block_;\
		    block_ = config_get_block_next(block_, KW_SCALER)) {\
			char const *name_;\
			name_ = config_get_block_param_string(block_, 0);\
			creator(crate, block_, name_, module);\
		}\
	} while (0)

struct ModuleGate {
	double	time_after_trigger_ns;
	double	width_ns;
};
struct Pedestal {
	uint16_t	buf[PEDESTAL_BUF_LEN];
	size_t	buf_idx;
	unsigned	threshold;
};

void		module_gate_get(struct ModuleGate *, struct ConfigBlock *,
    double, double, double, double);
void		module_parse_error(LOGL_ARGS, struct EventConstBuffer const *,
    void const *, char const *, ...) FUNC_PRINTF(5, 6);
void		module_pedestal_add(struct Pedestal *, uint16_t);
int		module_pedestal_calculate(struct Pedestal *) FUNC_RETURNS;
enum Keyword	module_get_type(struct Module const *);

#endif
