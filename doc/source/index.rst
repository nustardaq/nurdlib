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


################################
NUstar ReaDout LIBrary - Nurdlib
################################

.. image:: nurdlib.png

A library for configuring, reading data from, and carefully checking data
collection modules commonly used in nuclear physics experiments.

This document will start with simple examples, then cover common use cases and
questions, and at last delve deeper into the inner workings of this library.


*****************
Acquire and build
*****************

.. code-block:: bash

    git clone https://github.com/nustardaq/nurdlib.git
    cd nurdlib
    make -j8 fuser_drasi

The above creates a debug build of the library, some binary tools, and an
f-user (user readout binary) linked to drasi :ref:`[1] <bib-drasi>`.


*******
Example
*******

The nurdlib f-user opens **nurdlib.cfg** by default, so the following would go
to such a file:

.. code-block:: ini

    CRATE("VME0") {
        # TRIDI with TRLO II gateware.
        GSI_TRIDI(0x03000000) {
            # Read timestamps.
            timestamp = true
        }
        # Caen TDC with default nurdlib settings.
        CAEN_V775(0x00010000) {}
    }

Run the drasi f-user:

.. code-block:: bash

    ./bin/m_read_meb.drasi \
        --label=Test \
        --triva=master \
        --buf=size=100Mi \
        --max-ev-size=0x1000 \
        --subev=crate=0,type=10,subtype=1,control=1,procid=13 \
        --server=trans \
        --server=stream

This should read out timestamps from a TRIDI on address 0x03000000, and a Caen
v775 TDC on address 0x00010000.


************
Introduction
************

Nurdlib is an amalgamation of readout codes and experiences from many
experiments and DAQ operators involved mainly in activities related to NUSTAR,
but also other setups and experiments.

All this support can make nurdlib rather opaque and difficult to grasp fully,
but it does offer a lot of error checking and recovery that is difficult to
get right for a new DAQ.

The library aims to be as agnostic as possible to the DAQ backend, so the user
will need to provide the "glue" code. Nurdlib does know about MBS and drasi,
and ships with a minimal f-user which handles single-event readout.


********
Features
********

* ASCII-based configuration of readout.
* Sane default configurations for modules.
* Logging and debugging facilities.
* Striving for DAQ framework and platform independence.
* Module tagging for mixed module readout.
* Online module integrity tests.
* Online data integrity checking.
* Single- and multi-event, and shadow readout support.
* Static and dynamic mapping of memory.
* Single cycle and block transfer support.
* TRLO II :ref:`[2] <bib-trlo2>` support.
* Strict ANSI C compliance and harsh GCC flags.
* Unit testing and gcov reporting.
* Code unification and readability has high priority.
* Mostly non-recursive GNU makefiles.
* Command-line monitoring and configuration tool.
* Command-line module access tool.


*****************
Supported modules
*****************

* CAEN
    + v560, v767a, v775, v785(n), v792, v820, v830, v895, v965, v1190, v1290,
      v1725
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


********
Building
********

The Nurdlib build system is based on non-recursive GNU makefiles and a custom
auto-configuration tool called **nconf**. All generated files are placed
inside Nurdlib in a directory of the form:

.. code-block:: bash

    build_<gcc machine>_<gcc version>_<build mode>

including the static library **libnurdlib.a**. The somewhat complicated build
directory name eliminates collisions between several builds across platforms
and build modes with the same source.

Testing and coverage
====================

Unit testing is done with the custom **ntest**, and coverage analysis with
**gcov**. Testing works in any build mode, but coverage is only available in
a specific build mode, since it requires instrumentation which reduces runtime
performance significantly. With the **cov** build mode the following targets
are available:

.. code-block:: bash

    make BUILD_MODE=cov test       # Run tests.
    make BUILD_MODE=cov cov        # Coverage summary.
    make BUILD_MODE=cov cov_files  # Coverage summary per file (gcov -n).
    make BUILD_MODE=cov cov_funcs  # Coverage summary per function (gcov -fn).
    make BUILD_MODE=cov cov_anno   # Annotated source in build_*/cov/ (gcov -lp).

Release mode
============

When you are done debugging, build in release mode for meep-meep speeds:

.. code-block:: bash

    make BUILD_MODE=release

Build for shared library use
============================

To be used as a shared object, which is needed for the Python control support,
the additional ``-fPIC`` compile flag is needed. This may have a performance
impact and is disabled by default. The ``pic`` build mode enables this flag and
will build the Python control:

.. code-block:: bash

    make BUILD_MODE=pic


*****
Utils
*****

nurdctrl
========

Inspect a running Nurdlib:

.. code-block:: bash

    ./bin/nurdctrl -a localhost --config-dump
    ./bin/nurdctrl -a localhost --spec=print
    ./bin/nurdctrl -a localhost -s 0,2 -d
    ./bin/nurdctrl -a localhost -s 0,2 -m 0x02000000:32

Note that reading/writing from/to a module is really dangerous with a running
DAQ! Nurdlib does check the given addresses against known registers to prevent
mapping failures, but module logic can still be negatively impacted.

rwdump
======

Dump read/write module registers directly:

.. code-block:: bash

    ./bin/rwdump -a 0x02000000 -r 32
    ./bin/rwdump -a 0x02000000 -w 32,0xc4c4d0d0
    ./bin/rwdump -a 0x02000000 -t MESYTEC_MADC32 -d


*****************
Glue code example
*****************

.. code-block:: c

    #include <nurdlib.h>

    struct Crate *g_crate;
    struct CrateTag *g_tag;

    void
    backend_setup(void)
    {
        /*
         * arg1 = log-callback, default prints on stdio.
         * arg2 = config file.
         * arg3 = init-callback.
         * arg4 = deinit-callback.
         */
        g_crate = nurdlib_setup(NULL, "nurdlib.cfg", NULL, NULL);
        /* Module tag, default gets all untagged modules. */
        g_tag = crate_get_tag_by_name(g_crate, NULL);
    }

    void
    backend_shutdown(void)
    {
        nurdlib_shutdown(&g_crate);
    }

    void
    backend_readout(void *a_buf, size_t *a_buf_bytes)
    {
        struct EventBuffer eb;
        uint32_t ret;

        eb.ptr = a_buf;
        eb.bytes = *a_buf_bytes;

        /* Expect 1 trigger more since last readout. */
        crate_tag_counter_increase(g_crate, g_tag, 1);

        /*
         * Perform accesses that must be done inside dead-time, e.g. event
         * counters and event-buffer status.
         */
        ret = crate_readout_dt(g_crate);
        if (0 != ret) log_error("crate_readout_dt failed with 0x%x.", ret);

        /*
         * Perform accesses that can be done outside of dead-time, e.g.
         * empty event-buffers.
         */
        ret = crate_readout(g_crate, &event_buffer);
        if (0 != ret) log_error("crate_readout failed with 0x%x.", ret);

        /*
         * Manage logic to finish this readout and prepare for the next.
         */
        crate_readout_finalize(g_crate);

        *a_buf_bytes -= eb.bytes;
    }


*********
Functions
*********

Tags
====

Tags group modules so different sets of modules can be read out each time.
Internally this lets nurdlib know how to treat the event counters of every
module, since they may advance differently.

Let's look at an example:

.. code-block:: ini

    CRATE("MyCrate") {
        CAEN_V775(0x00010000) {} # Let's call this module A.
        TAGS("1")                # The following modules are tagged "1".
        CAEN_V775(0x00020000) {} # Module B.
        TAGS("2")
        CAEN_V775(0x00030000) {} # Module C.
        TAGS("1", "2")           # The following are tagged both "1" and "2".
        CAEN_V775(0x00040000) {} # Module D.
    }

Tabulate this in both directions:

====== ======= = ======= =======
Module Tags        Tag   Modules
====== ======= = ======= =======
   A   Default   Default    A
   B      1         1      B,D
   C      2         2      C,D
   D     1,2
====== ======= = ======= =======

The user code then has to say how the event counters have advanced for every
tag, let's say there is only 1 event per readout:

.. code-block:: c

    tag[0] = crate_get_tag_by_name(crate, "Default");
    tag[1] = crate_get_tag_by_name(crate, "1");
    tag[2] = crate_get_tag_by_name(crate, "2");
    ...
    crate_tag_counter_increase(crate, tag[readout_type], 1);

===========
Multi-event
===========

In multi-event mode, the readout happens after any number of events between 1
and some number N. Nurdlib must know the exact number in order to correctly
compare event counters, and this is typically done with a scaler channel,
e.g.:

.. code-block:: ini

    CRATE("Multi-event") {
        TAGS("1") {
            scaler_name = "my_scaler"
        }
        CAEN_V775(0x00020000) {}
        TAGS("2")
        CAEN_V830(0x00030000) {
            SCALER("my_scaler") {
                channel = 0
            }
        }
    }

Tag "1" will listen to the first channel of the V830 scaler module to count
the number of triggers that were sent to the V775. The V830 is however
expected to be latched in single-event mode together with tag "2".

Exactly how a module type defines a scaler channel depends on its
implementation. Below follows the available options as of writing:

.. code-block:: ini

    CAEN_V8{2,3}0:
        SCALER("my_scaler") { channel = 0..31 }
    GSI_{TRIDI,VULOM4,RFX1}:
        SCALER("my_scaler") { type = accept_pulse }
        SCALER("my_scaler") { type = accept_trig, channel = 0..31 }
        SCALER("my_scaler") { type = ecl, channel = 0..31 }
        SCALER("my_scaler") { type = master_start }
        SCALER("my_scaler") { type = nim, channel = 0..31 }


*********************
Environment variables
*********************

Many environment variables control the build process, but a few also control
run-time behaviour.

Run-time variables
==================

* **NURDLIB_DEF_PATH**: Points to the directory where default configuration
  files reside. This variable will override the default
  **../nurdlib/cfg/default/** path.
* **NTEST_COLOR**: The tests output by ntest prints ANSI color escape codes
  by default. Setting **NTEST_COLOR=0** will disable these codes.
* **NTEST_BAIL**: Abort on first failed test.

The configuration files can also use environment variables when including
files:

.. code-block:: ini

    CRATE("MyCrate") {
        include "$CFG_PATH/my_config.cfg"
    }

Build variables
===============

The typical compilation and linking flags are:

* **CPPFLAGS**: Pre-compiler flags, such as -I and -D. These are prepended to
  the nurdlib pre-compiler flags so these can override search paths, e.g.
  **-Iuser_header_path**.

* **CFLAGS**: Compilation flags, such as -W. These are appended to the
  nurdlib compilation flags to override compilation switches, e.g.
  **-Wno-crazy-warning**.

* **LDFLAGS**: Linking flags, such as -L. These are prepended to the nurdlib
  linking flags to override search paths, e.g. **-Luser_library_path**.

* **LIBS**: Libraries, such as -l and direct paths to static and dynamic
  libraries. These are appended to the nurdlib libraries to cover for missing
  libraries, e.g. **-luser_library** or **/path/userlib.so.1.2.3**.

Build modes set with **BUILD_MODE**:

* **cov**: Coverage compiled and linked in. This will insert instrumentation
  into the code and the final binaries will run much slower.

* **gprof**: -pg added to compilation and linking for profiling.

* **pic**: Position independent code, necessary to build the Python
  online support.

* **release**: Optimized build.

Several programs can be overridden:

* **AR**: Static linker.
* **BISON**: Bison-like parser generator.
* **CCACHE**: Ccache for fast recompilations.
* **CPPCHECK**: Static analysis tool for C code.
* **FLEX**: Lexical analyser generator for Bison input.
* **INKSCAPE**: To convert SVG to PNG for this documentation.
* **LATEX**: Latex to DVI converter.
* **PYTHON**: Python support for online communication .
* **SED**: GNU sed.
* **SPHINX_BUILD**: To build this documentation.

Drasi related flags:

* **DRASI_PATH**: Path to drasi, where **$DRASI_PATH/bin/drasi-config.sh**
  must exist.

TRLO II related flags:

* **TRLOII_PATH**: Path to TRLO II directory.
* **{TRIDI,VULOM4,RFX1}_FW**: By default, nurdlib chooses TRLO II
  firmware versions by looking at the directories
  **$TRLOII_PATH/trloctrl/fw_\*_\*/**, but this decision can be overridden
  with these env-vars.


************
Build system
************

Early versions of nurdlib supported CES RIO2 systems with very old versions of
GNU make. This platform has not been tested in a long time, but the makefiles
probably support rather old versions of make.

The master makefile in the project root directory includes rules.mk files in
sub-directories. This way all dependencies and references end up in the same
space and can produce a more complete build graph.

Early in the build process, an **MD5** sum is calculated from source files
which have an impact on the online communication. The sum stored in the
library and the client must match at runtime to be sure that the data handling
in the readout and client processes are compatible.

Auto-configuration of flags and libraries is done by **nconf**. This step is
performed early in the build, following the usual make dependency rules.

If you would like to build only a sub-set of all available module types, you
can specify them in this file. There is a list of modules near the top of the
main makefile which can be copied and modified.

.. code-block:: bash

    echo "MODULE_LIST=caen_v775" > local.mk

Note that some modules depend on other modules and there is no guard on such
dependencies, it is completely manual!

nconf
=====

nconf scans through a file looking for **if NCONF_m<module>_b<branch>**,
groups all switches with the same **module**, tries to build, link, and
execute a test program for every **branch**, and whichever branch succeeds
first within a module is chosen.

To only use the results of nconf of another project, for example to use
nurdlib decisions in your own DAQ build:

.. code-block:: ini

    NCONF_ARGS = $(NURDLIB_PATH)/$(BUILD_DIR)/nconf.args

If you then access the four variables **NCONF_CPPFLAGS*, **NCONF_CFLAGS*,
**NCONF_LDFLAGS*, **NCONF_LIBS**, you will get the flags that were chosen
during the nurdlib build process.

If you'd like to use nconf yourself, first:

.. code-block:: ini

    NCONF_H = all files to be nconfed
    NCONF_ARGS = $(BUILD_DIR)/nconf.args
    NCONF_PREV = $(NURDLIB_PATH)/$(BUILD_DIR)/nconf.args
    include $(NURDLIB_PATH)/nconf/nconf.mk

This will create files in **$(BUILD_DIR)/nconf\***.

**$(BUILD_DIR)/nconfing/** will contain intermediary files used when nconf is
running its tests.

**$(BUILD_DIR)/nconf/** will contain the final header, arguments, and log
files. The header defines the branch for each module and the arguments file
contains the accumulation of the previous nconf:ed arguments and those for the
current file.

**$(BUILD_DIR)/nconf.args** will contain all the arguments from all files.


ntest
=====

Nurdlib has a rigorous set of unit tests that anybody can run to verify a
large fraction of the code. New features and critical bugs should be
accompanied by testing code. This way, new code should not break existing
tested code, and old bugs should be detected to the extent of the tests, just
the typical testing approach. Note that this testing only applies to internal
logic and utilities, and can not test hardware or external user
implementations!

.. code-block:: bash

    make test


Control interface
=================

While running, nurdlib can provide information on crate and module
configuration and allow the user to talk to configured modules. This control
interface is served by a UDP server inside the library. Since it does eat some
resources it can be disabled.

In order to talk to this control server, use **bin/nurdctrl**. The help text
explains the available commands, but here are a examples:

.. code-block:: bash

    ./nurdctrl -c

This lists configured crates in the nurdlib running on localhost on the
default port.

.. code-block:: bash

    ./nurdctrl -s print

Prints a simple graph view of the crate and modules, with indices used to
refer to specific parts.

.. code-block:: bash

    ./nurdctrl -a192.168.0.100:10000 -s0,1 -d

This dumps the hardware registers in module 1 (zero-based indexing) in crate
0, with nurdlib running on the host 192.168.0.100 on port 10000.

To control the UDP server, set the global **control_port** config, which is
documented in **cfg/default/global.cfg**:

.. code-block:: ini

    control_port = -1  # Use the default port.
    control_port = 0   # Disable the server.
    control_port = n   # Use the given port.

Configuration parser
====================

The config parser was written by hand to save memory for systems with limited
resources, so there's no handy list of grammar rules for the parsing. This
section aims to explain most of the grammar.

.. code-block:: ini

    top:              item_list
    item_list:        item_list item
    item:             block OR config

    block:            block_header { item_list }
    block_header:     name OR name(block_param_list)
    block_param_list: block_param_list block_param
    block_param:      scalar

    config:           name = value
    value:            scalar OR vector
    vector:           ( scalar_list )
    scalar_list:      scalar_list scalar
    scalar:           literal_string OR range OR value OR value unit
    range:            integer .. integer
    value:            integer OR double
    unit:             mV OR uV etc...


**********
Guidelines
**********

Nurdlib aims to follow certain guidelines to be safe and somewhat
maintainable:

* Monotonic counters: Any progressing value is not reset, unless the hardware
  enforces it. Software counters are absolute, and progression is calculated
  as differences. This approach carries more information than resetting
  values.

* Resource freeing: Always follows a certain pattern, which makes sure that
  pointers cannot escape:

.. code-block:: c

    void myobj_free(struct MyObject **a_obj) {
        struct MyObject *obj = *a_obj;
        if (obj) {
            ...
            FREE(*a_obj);
        }
    }

* Goto is not completely evil: Liberal use can make code difficult to follow,
  but is used for a lot of error handling in larger functions:

.. code-block:: c

    void func(void) {
        if (!cond) goto fail;
        return success;
    fail:
        log_die(LOGL, "Well, crap.");
    }

* OpenBSD KNF-like style: There are many exceptions to this style, but
  indentation, not typedef-ing struct's, snake-case variables are some of
  the most important.


******
People
******

Haik Simon, Håkan T. Johansson, Alexandre Charpy, Bastian Löher, Michael
Munch, Oliver Papst, Stephane Pietri, Hans Törnqvist.

**********
References
**********

GSI Scientific report 2014: `MU-NUSTAR-NR-08.pdf <https://repository.gsi.de/record/183940/files/MU-NUSTAR-NR-08.pdf>`_.

.. _bib-drasi:

drasi: http://fy.chalmers.se/~f96hajo/drasi/doc/

.. _bib-trlo2:

TRLO II: https://fy.chalmers.se/~f96hajo/trloii/
