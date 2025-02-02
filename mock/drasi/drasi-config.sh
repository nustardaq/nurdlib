#!/bin/bash

# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2024-2025
# Hans Toshihide Törnqvist
# Håkan T Johansson
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

if echo $* | grep -q -- --cflags
then
  echo
elif echo $* | grep -q -- --libs
then
  echo mock/drasi/f_user_daq.o \
       mock/drasi/lwroc_message_internal.o \
       mock/drasi/lwroc_thread_util.o \
       mock/drasi/lwroc_track_timestamp.o \
       mock/drasi/lwroc_triva_state.o \
       mock/drasi/lmd/lwroc_lmd_ev_sev.o
else
  exit 1
fi
