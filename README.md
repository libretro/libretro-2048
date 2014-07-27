
Dependencies
============

libretro-2048 requires fontconfig and freetype to build (these depend on expat,
bzip, zlib and iconv).

Cross Compiling
===============

`make HOST=i686-w64-mingw32 CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++ platform=win`

Changes to cairo and pixman
===========================

* Cairo has been patched to compile with mingw32-w64, this was needed because
  the version in this repository is older than 1.12.

* Both cairo and pixman have been stripped of some source files and directories
  (tests, documentation, utilities).

