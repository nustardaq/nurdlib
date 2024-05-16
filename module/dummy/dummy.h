/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2019, 2024
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

#ifndef MODULE_DUMMY_H
#define MODULE_DUMMY_H

/* Needed for MODULE_INTERFACE macro. */
#include <module/module.h>

/* This prototypes two interface functions for a module type:
 *  create_ = Creates one instance of a module.
 *  setup_  = Sets up properties for the module type, shared between all
 *            instances.
 */
MODULE_INTERFACE(dummy);

#endif
