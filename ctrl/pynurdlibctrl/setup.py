# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2021, 2024
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

from distutils.core import setup, Extension
import os
import subprocess
import sys
import platform

print("Using python " + platform.python_version())

nurdlib_path = '../../'

build_dir = subprocess.Popen(['make', '-s', '-C', nurdlib_path, '-f', 'gmake/build_dir_echo.mk'],
    stdout=subprocess.PIPE,universal_newlines=True).communicate()[0].rstrip('\r\n')
print ("Using BUILD_DIR=" + build_dir)

pynurdlibctrl = Extension('pynurdlibctrl',
        include_dirs = [nurdlib_path,
            nurdlib_path + 'include',
            nurdlib_path + build_dir],
        extra_objects = [nurdlib_path + build_dir + '/libnurdlib.a'],
        sources = ['pynurdlibctrl.c'])

setup (name = 'PyNurdlibCtrl',
        version = '1.0',
        description = 'Control client for nurdlib',
        ext_modules = [pynurdlibctrl])
