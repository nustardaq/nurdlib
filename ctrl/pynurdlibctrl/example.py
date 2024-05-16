#!/usr/bin/python

# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2024
# Bastian Löher
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

from pynurdlibctrl import NurdlibCtrl
from pprint import pprint
import sys

if len(sys.argv) < 2:
    print ("Usage: " + sys.argv[0] + " <hostname>")
    exit(1);

nc = NurdlibCtrl(sys.argv[1])
crate_list = nc.crate_list()
print ("Crates: ")
pprint(crate_list)
print ("Info: " + str(nc.crate_info(0)))
print ("Config: ")
pprint (nc.config_dump())
pprint (nc.register_dump(0,0,1))
