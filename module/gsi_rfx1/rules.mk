# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2021-2022, 2024
# Michael Munch
# Hans Toshihide Törnqvist
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

NAME:=gsi_rfx1
DIR_$(NAME):=module/$(NAME)
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)

include gmake/c.mk
include gmake/close.mk

TRLOII_TYPE:=RFX1
include module/trloii/reglist.mk
