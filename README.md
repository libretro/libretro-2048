
Dependencies
============

libretro-2048 has two backends: cairo and non-cairo.
Builds with the cairo backend have nicer fonts but require fontconfig and
freetype to build (these depend on expat, bzip, zlib and iconv).
By default, it builds with the non-cairo backend. To compile with cairo:
`make -f Makefile.libretro WANT_CAIRO=1`

Cross Compiling
===============

`make -f Makefile.libretro [HOST=i686-w64-mingw32 WANT_CAIRO=1 USE_STATIC_LIBS=1] CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++ platform=win`

Changes to cairo and pixman
===========================

* Cairo has been patched to compile with mingw32-w64, this was needed because
  the version in this repository is older than 1.12.

* Both cairo and pixman have been stripped of some source files and directories
  (tests, documentation, utilities).

