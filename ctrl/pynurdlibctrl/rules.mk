# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2021, 2023-2024
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

NAME:=pynurdlibctrl
PYDIR:=ctrl/$(NAME)

python_version_full:=$(wordlist 2,4,$(subst ., ,$(shell $(PYTHON) -V 2>&1)))
python_version_major:=$(word 1,$(python_version_full))
python_version_minor:=$(word 2,$(python_version_full))

SETUP_PY_INSTALL:=CFLAGS="-std=c99" $(PYTHON) setup.py build -b ../../$(BUILD_DIR)/$(PYDIR) install --user

ifeq ($(python_version_major),2)
 PY_LT_2_7:=$(shell echo "$(python_version_minor) < 7" | bc)
 ifeq ($(PY_LT_2_7),1)
  SETUP_PY_INSTALL:=echo "Cannot run setup.py with python older 2.7"
 endif
endif

.PHONY: $(NAME)_install
$(NAME)_install: $(BUILD_DIR)/./libnurdlib.a
	$(QUIET)echo "PY    $@";\
	cd $(PYDIR) && $(SETUP_PY_INSTALL)
