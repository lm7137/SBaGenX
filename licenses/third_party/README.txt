Third-party licenses used by SBaGenX
====================================

Retrieved on: 2026-02-17

This directory stores upstream license texts for libraries used by SBaGenX
(either bundled in source form, built as optional static decode libs, or
loaded at runtime for output encoding).

Files and source locations:

1) dr_flac (from dr_libs) - bundled header for FLAC decode
   License file: dr_libs-LICENSE.txt
   Source: https://github.com/mackron/dr_libs

2) libsndfile - optional runtime output encoding (.ogg/.flac)
   License file: libsndfile-COPYING.txt
   Source: https://github.com/libsndfile/libsndfile

3) libmp3lame - optional runtime output encoding (.mp3)
   License file: libmp3lame-COPYING.txt
   Source: https://github.com/lameproject/lame

4) libogg - optional static decode dependency
   License file: libogg-COPYING.txt
   Source: https://github.com/xiph/ogg

5) Tremor / libvorbisidec - optional static OGG decode support
   License file: tremor-COPYING.txt
   Source archive:
   https://launchpadlibrarian.net/35151187/libvorbisidec_1.0.2+svn16259.orig.tar.gz

6) libvorbis - referenced Xiph/Vorbis license text
   License file: libvorbis-COPYING.txt
   Source: https://github.com/xiph/vorbis

7) FLAC - referenced FLAC/Xiph license text
   License file: flac-COPYING.Xiph.txt
   Source: https://github.com/xiph/flac

8) libmad - optional static MP3 decode support
   License files: libmad-COPYING.txt, libmad-COPYRIGHT.txt
   Source archive:
   https://downloads.sourceforge.net/project/mad/libmad/0.15.1b/libmad-0.15.1b.tar.gz

9) stb_image_write (from stb) - bundled PNG writer used for -G
   sigmoid graph output
   License file: stb-LICENSE.txt
   Source: https://github.com/nothings/stb

10) mpg123 - runtime dependency for bundled Windows libsndfile DLL
    License file: mpg123-COPYING.txt
    Source: https://github.com/madebr/mpg123

11) opus - runtime dependency for bundled Windows libsndfile DLL
    License file: opus-COPYING.txt
    Source: https://github.com/xiph/opus

12) libwinpthread (mingw-w64) - runtime dependency for bundled Windows
    codec DLLs
    License file: libwinpthread-COPYING.txt
    Source: https://github.com/mingw-w64/mingw-w64

13) GCC runtime libraries (mingw-w64) - runtime dependency for bundled
    Windows codec DLLs
    License files: gcc-libs-COPYING.RUNTIME.txt,
    gcc-libs-COPYING3.txt, gcc-libs-COPYING.LIB.txt
    Source: https://gcc.gnu.org/
