.. nurdlib, NUstar ReaDout LIBrary
..
.. Copyright (C) 2017, 2021-2022, 2025
.. Bastian Löher
.. Hans Toshihide Törnqvist
..
.. This library is free software; you can redistribute it and/or
.. modify it under the terms of the GNU Lesser General Public
.. License as published by the Free Software Foundation; either
.. version 2.1 of the License, or (at your option) any later version.
..
.. This library is distributed in the hope that it will be useful,
.. but WITHOUT ANY WARRANTY; without even the implied warranty of
.. MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
.. Lesser General Public License for more details.
..
.. You should have received a copy of the GNU Lesser General Public
.. License along with this library; if not, write to the Free Software
.. Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
.. MA  02110-1301  USA

NUstar ReaDout LIBrary - Nurdlib
================================

A library for configuring, reading data from, and carefully checking data
collection modules commonly used in nuclear physics experiments.

Example Config
--------------

.. code-block:: ini

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

Build Nurdlib::

    git clone https://github.com/nustardaq/nurdlib
    cd nurdlib
    make

Overview
--------

.. image:: nurdlib.png

Features
--------

* ASCII-based configuration of readout.
* Sane default configurations for modules.
* Logging and debugging facilities.
* Striving for DAQ framework and platform independence.
* Module tagging for mixed module readout.
* Online module integrity tests.
* Online data integrity checking.
* Multi-event and shadow readout support.
* Static and dynamic mapping of memory.
* Single cycle and block transfer.
* TRLO II support.
* Strict ANSI C compliance and harsh GCC flags.
* Unit testing with gcov reporting.
* Code unification and readability has high priority.
* Command line monitoring and configuration tool.
* Command line module access tool.

Supported modules
-----------------

* CAEN
    + v560, v767a, v775, v785(n), v792, v820, v830, v895, v965, v1190, v1290
* GSI
    + ctdc, febex, kilom, mppc, pexaria, sam, siderem, tacquila, tamex 2 & 3,
      tridi(trlo2), triva, vetar, vftx2, vulom(trlo2), vuprom(TDC)
* Mesytec
    + madc32, mqdc32, mtdc32, mdpp16/32 scp/qdc, vmmr8
* PNPI
    + cros3
* Struck SIS
    + 3316, 3801, 3820, 3820-scaler
* Dummy module for testing
* Anything we can get our hands on in good time

Building Nurdlib
----------------

The Nurdlib build system is based on non-recursive GNU makefiles and a custom
auto-configuration tool called nconf. All generated files are placed inside
Nurdlib in a directory of the form::

    build_<gcc machine>_<gcc version>_<build mode>

including the static library libnurdlib.a. The somewhat complicated build
directory name eliminates collisions between several builds across platforms
and build modes with the same sources.

Testing and coverage
--------------------

Unit testing is done with ntest, and coverage analysis with gcov. Testing works
in any build mode, but coverage requires instrumentation which reduces runtime
performance a lot and is disabled by default. Nurdlib has coverage-specific
build targets to report the results after the test target has executed at least
once:

* cov - Small summary of all results.
* cov_files - File-level report (gcov -n).
* cov_funcs - Function-level report (gcov -fn).
* cov_anno - Annotated files placed in $BUILD_DIR/cov/ (gcov -lp).

Build in cov mode::

    make BUILD_MODE=cov test
    make BUILD_MODE=cov cov        # Coverage summary.
    make BUILD_MODE=cov cov_files  # Coverage summary per file.
    make BUILD_MODE=cov cov_funcs  # Coverage summary per function.
    make BUILD_MODE=cov cov_anno   # Source annotation in build directory.

Release mode
------------

When you are done debugging, build in release mode for meep-meep speeds::

    make BUILD_MODE=release

Build for shared library use
----------------------------

To be used as a shared object, which is needed for the Python control support,
the additional ``-fPIC`` compile flag is needed. This may have a performance
impact and is disabled by default. The ``pic`` build mode enables this flag and
will build the Python control::

    make BUILD_MODE=pic

Utils
-----

Inspect a running Nurdlib::

    ./bin/nurdctrl -a somehost --config-dump
    ./bin/nurdctrl -a somehost --spec=print
    ./bin/nurdctrl -a somehost -s 0,2 -d
    ./bin/nurdctrl -a somehost -s 0,2 -m 0x02000000:32

Note that reading/writing from/to a module is really dangerous with a running
DAQ! Nurdlib does check the given addresses against known registers to prevent
mapping failures, but module logic can still be negatively impacted.

Dump read/write module registers directly::

    ./bin/rwdump -a 0x02000000 -r 32
    ./bin/rwdump -a 0x02000000 -w 32,0xc4c4d0d0
    ./bin/rwdump -a 0x02000000 -t MESYTEC_MADC32 -d

People
------

Haik Simon, Håkan T. Johansson, Alexandre Charpy, Bastian Löher, Michael Munch,
Oliver Papst, Stephane Pietri, Hans Törnqvist.

References
----------

GSI Scientific report 2014: `MU-NUSTAR-NR-08.pdf <https://repository.gsi.de/record/183940/files/MU-NUSTAR-NR-08.pdf>`_.
