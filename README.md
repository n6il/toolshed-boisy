# ToolShed

ToolShed is a cornucopia of tools and source code for the Tandy Color Computer and Dragon micro.

The repository contains:
- assemblers to perform cross-development from Windows, Linux, and macOS
- os9 and decb tools for copying files to/from host file systems to disk images
- source code for various CoCo and Dragon ROMs
- source code to HDB-DOS, DriveWire DOS, and SuperDOS
- other miscellaneous tools

**NOTE:** while the venerable 6809 cross-assembler, mamou, is part of the repository, it is only kept for historical value. Everyone should really be using William Astle's excellent LWTOOLS which contains the *lwasm* 6809 assembler and *lwlink* linker. [Download the latest version of the source here.](http://lwtools.projects.l-w.ca)

## Building on Windows

The recommended build environment is MingW32 or MingW64 (http://mingw.org/), MSYS2 (http://msys2.github.io/), or the WSL subsystem (for Windows 10+) (https://en.wikipedia.org/wiki/Windows_Subsystem_for_Linux).

The easiest way to install MingW is using a mingw-get-inst.*.exe from http://mingw.org/wiki/Getting_Started

Inside MingW, make sure you have "make" installed. There are several options, but the simpler mingw-make should be good enough:
```
$ mingw-get install mingw-make
```

Enter the unpackaged toolshed directory and run:
```
$ make -C build/unix install CC=gcc
```

## Building on Linux and macOS

To build cocofuse you will need to have FUSE libraries and header files installed. On Debian-based systems:
```
$ sudo apt-get install libfuse-dev
```

Enter the unpackaged toolshed directory and run:
```
$ make -C build/unix install
```

## Building HDB-DOS and DriveWire DOS

It is recommended to have lwtools installed (http://lwtools.projects.l-w.ca/).  You will also need "makewav" from Toolshed installed to build WAV files.  See hdbdos/README.txt and the makefiles for different build options.

To build all default flavors:
```
$ make -C dwdos
$ make -C hdbdos
$ make -C superdos
```

Instead of lwtools the deprecated mamou can still be used (YMMV):
```
$ make -C dwdos AS="mamou -r -q"
$ make -C hdbdos AS="mamou -r -q"
```

Note that superdos still builds with mamou by default.
