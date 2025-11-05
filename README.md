# nurdlib

This is nurdlib, the NUstar ReaDout LIBrary, a big piece of code to configure,
read out, and verify readout hardware and its data, typically in a way that's
common for nuclear physics experiments such as those performed by the NUSTAR
collaboration.


## Building

```
make
make BUILD_MODE=release
```

gives you a debug and release build, respectively, of the readout library in a
directory named **build_\*_{debug,release}**, plus some programs available in
**bin/**.


If MBS is available through the typical GSI environment variables, or drasi is
in a directory adjacent to nurdlib, or **DRASI_PATH** (see fuser/rules.mk) is
set appropriately, one of:

```
make fuser_mbs
make fuser_drasi
```

creates **bin/m_read_meb.{mbs,drasi}** that is sufficient to run a simple
readout.


## More documentation

Install Sphinx and the rtd theme, then:

```
make doc
```

creates Sphinx webpage documentation, which can also be
[found online](https://nustardaq.github.io/nurdlib/).


## Licence

Licenced with LGPL 2.1, a copy can be found in the file COPYING.
