# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2017, 2021-2022, 2024
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

NUstar ReaDout LIBrary - Nurdlib
================================

The definite library for readout of data collection modules commonly used in nuclear physics experiments. Made in Germany.

Example Config
--------------

.. code-block:: guess

    # Name and crate ID
    CRATE("VME0")
    {
            # Use multi-event readout
            multi_event = true
            # Type + address
            GSI_TRIDI(0x02000000) {}
            CAEN_V775(0x00010000)
            {
                    common_start = true
                    time_range = 1200 ns
            }
    }

Installation
------------

The short version for the impatient:

Install Nurdlib::

    git clone lx-pool.gsi.de:/u/htoernqv/repos/nurdlib.git
    cd nurdlib
    export NURDLIB_DEF_PATH=$(pwd)/cfg/default
    make

To run all tests::

    make test

Overview
--------

.. image:: nurdlib.png

Features
--------

* ASCII-based configuration of readout
* Sane default configuration for modules
* Logging and debugging facilities
* Simple API striving for safe usage
* DAQ framework and platform independence
* Module tagging for mixed module read-out
* Simple online module tests
* Online data integrity checking
* Multi-event support
* Static and dynamic mapping of memory
* Single cycle and block transfer
* TRLO II support
* Strict ANSI C compliance and harsh GCC flags
* Offline unit testing with coverage report
* Code unification and readability has high priority
* Command line monitoring and configuration tool
* External memory access and testing tools

Supported modules
-----------------

* CAEN
    + v775, v785, v792, v820, v830, v895, v965, v1190, v1290
* GSI
    + febex, sam, tacquila, tamex 2(+PADI) & 3 tridi(trlo2), triva, vetar, vftx2, vulom(trlo2), vuprom(TDC)
* Mesytec
    + madc32, mqdc32, mtdc32, mdpp16, vmmr8
* PNPI - cros3
* Struck SIS - sis3316
* Dummy module for testing
* anything we can get our hands on in good time

Building Nurdlib
----------------

The Nurdlib build system is based on non-recursive GNU makefiles and a custom auto-configuration tool called nconf. All generated files are placed inside Nurdlib in a directory of the form::

    build_<gcc machine>_<gcc version>_<build mode>/

including the static library libnurdlib.a. Link your DAQ binary with this library and you're done. The somewhat complicated build directory name eliminates collisions between several builds across platforms and build modes with the same sources.

Testing and coverage
--------------------

Unit testing is done with ntest, and coverage analysis with gcov. Testing works in any build mode, but coverage requires instrumentation which reduces runtime performance a lot, and is disabled by default. Nurdlib has coverage-specific build targets to report the results after the test target has executed at least once:

* cov - Small summary of all results.
* cov_files - File-level report (gcov -n).
* cov_funcs - Function-level report (gcov -fn).
* cov_anno - Annotated files placed in $BUILD_DIR/cov/ (gcov -lp).

Build in cov mode::

    make BUILD_MODE=cov test
    make BUILD_MODE=cov cov
    make BUILD_MODE=cov cov_files
    make BUILD_MODE=cov cov_funcs
    make BUILD_MODE=cov cov_anno

Release mode
------------

When you are done debugging, build in release mode::

    make BUILD_MODE=release

Build for shared library use
----------------------------

To be used as a shared object, which is needed for the Python control support, the additional ``-fPIC`` compile flag is needed. This usually may have a performance impact and is therefore disabled by default. The ``pic`` build mode enables this flag::

    make BUILD_MODE=pic

Utils
-----

Inspect a running Nurdlib::

    ./bin/nurdctrl --crates
    ./bin/nurdctrl --crate=0 --crate-info
    ./bin/nurdctrl --crate=0 --module=2 --register-dump

Dump the read/write module registers directly::

    ./bin/rwdump -a 0x02000000 -r 32

Online read/write with nurdctrl is not implemented to reduce the risk of
locking up a running DAQ (there are enough lurking dangers for a DAQ).

Development status
------------------

We believe in community support and openness when developing this library, and therefore publish an updated development status regularly `right here <http://web-docs.gsi.de/~land/nurdlib/old/dev.html>`_!

People
------

Haik Simon, Håkan T. Johansson, Alexandre Charpy, Bastian Löher, Michael Munch, Hans Törnqvist

References
----------

GSI Scientific report 2014: `MU-NUSTAR-NR-08.pdf <https://repository.gsi.de/record/183940/files/MU-NUSTAR-NR-08.pdf>`_.

Old documentation is `here <http://web-docs.gsi.de/~land/nurdlib/old/index.html>`_.
