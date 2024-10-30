/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2021, 2023-2024
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

#ifndef NURDLIB_CTRL_H
#define NURDLIB_CTRL_H

#include <nurdlib/config.h>
#include <nurdlib/keyword.h>
#include <util/funcattr.h>
#include <util/queue.h>
#include <util/stdint.h>

struct Packer;

enum CtrlCommand {
	VL_CTRL_ONLINE = 0,
	VL_CTRL_CRATE_ARRAY,
	VL_CTRL_CRATE_INFO,
	VL_CTRL_REGISTER_LIST,
	VL_CTRL_CONFIG,
	VL_CTRL_CONFIG_DUMP,
	VL_CTRL_GOC_READ,
	VL_CTRL_GOC_WRITE,
	VL_CTRL_MODULE_ACCESS
};
struct CtrlClient;
struct CtrlServer;
TAILQ_HEAD(CtrlConfigScalarList, CtrlConfigScalar);
struct CtrlConfigScalar {
	enum	ConfigScalarType	type;
	unsigned	vector_index;
	union {
		struct {
			double	d;
			unsigned	unit;
		} d;
		struct {
			int32_t	i;
			unsigned	unit;
		} i32;
		enum	Keyword keyword;
		struct {
			unsigned	first;
			unsigned	last;
		} range;
		char	*str;
	} value;
	TAILQ_ENTRY(CtrlConfigScalar)	next;
};
TAILQ_HEAD(CtrlConfigList, CtrlConfig);
struct CtrlConfig {
	enum	ConfigType type;
	enum	Keyword name;
	int	is_touched;
	struct	CtrlConfigScalarList scalar_list;
	struct	CtrlConfigList child_list;
	TAILQ_ENTRY(CtrlConfig)	next;
};
struct CtrlCrate {
	char	*name;
	size_t	module_num;
	struct	CtrlModule *module_array;
};
struct CtrlCrateArray {
	size_t	num;
	struct	CtrlCrate *array;
};
struct CtrlCrateInfo {
	uint16_t	event_max_override;
	uint8_t	dt_release;
	uint16_t	acvt;
	struct {
		uint32_t	buf_bytes;
		uint32_t	max_bytes;
	} shadow;
};
struct CtrlModule {
	enum	Keyword type;
	size_t	submodule_num;
	struct	CtrlSubModule *submodule_array;
};
struct CtrlSubModule {
	enum	Keyword type;
};
struct CtrlRegister {
	struct	RegisterEntryClient const *entry;
	uint32_t	*value;
};
struct CtrlRegisterCustom {
	uint16_t	name;
	uint16_t	value;
};
struct CtrlRegisterArray {
	enum	Keyword module_type;
	unsigned	num;
	struct	CtrlRegister *reg_array;
	struct {
		unsigned	num;
		struct	CtrlRegisterCustom *reg_array;
	} custom;
};
struct CtrlModuleAccess {
	uint32_t	ofs;
	unsigned	bits;
	unsigned	do_read;
	uint32_t	value;
};
struct DatagramArray {
	size_t	num;
	struct	UDPDatagram	*array;
};

#define CTRL_DEFAULT_PORT 23546

/* The client does not run in a separate thread. */
void			ctrl_client_config(struct CtrlClient *, int, int, int,
    struct ConfigBlock *);
int			ctrl_client_config_dump(struct CtrlClient *, struct
    CtrlConfigList *) FUNC_RETURNS;
struct CtrlClient	*ctrl_client_create(char const *, uint16_t)
	FUNC_RETURNS;
void			ctrl_client_free(struct CtrlClient **);
void			ctrl_client_crate_array_free(struct CtrlCrateArray *);
int			ctrl_client_crate_array_get(struct CtrlClient *,
    struct CtrlCrateArray *);
int			ctrl_client_crate_info_get(struct CtrlClient *, struct
    CtrlCrateInfo *, int);
int			ctrl_client_goc_read(struct CtrlClient *, uint8_t,
    uint8_t, uint16_t, uint32_t, uint16_t, uint32_t *) FUNC_RETURNS;
void			ctrl_client_goc_write(struct CtrlClient *, uint8_t,
    uint8_t, uint16_t, uint32_t, uint16_t, uint32_t);
int			ctrl_client_is_online(struct CtrlClient *)
	FUNC_RETURNS;
int			ctrl_client_module_access_get(struct CtrlClient *,
    int, int, int, struct CtrlModuleAccess *, size_t) FUNC_RETURNS;
void			ctrl_client_register_array_free(struct
    CtrlRegisterArray *);
int			ctrl_client_register_array_get(struct CtrlClient *,
    struct CtrlRegisterArray *, int, int, int) FUNC_RETURNS;
void			ctrl_client_register_array_print(struct
    CtrlRegisterArray *);
int			ctrl_client_register_array_unpack(struct
    CtrlRegisterArray *, struct DatagramArray *, size_t *, struct Packer *)
FUNC_RETURNS;
void			ctrl_config_dump_free(struct CtrlConfigList *);
void			ctrl_config_dump_print(struct CtrlConfigList const *,
    unsigned);

/* The server does however. */
struct CtrlServer	*ctrl_server_create(void) FUNC_RETURNS;
void			ctrl_server_free(struct CtrlServer **);

#endif
