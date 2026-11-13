# inkpage

**inkpage** is a simple PDF editor for Linux, built with Qt 6. It is a fork of
[PDF4QT](https://github.com/JakubMelka/PDF4QT) by Jakub Melka.

Copyright (C) 2026 Ludovic Mercier. Based on PDF4QT, copyright (c) 2018-2026
Jakub Melka and Contributors.

inkpage is free software, released under the GNU General Public License
version 3 or later. It is provided without any warranty of any kind.

## 1. HISTORY OF THE FORK

PDF4QT is a complete PDF suite: a rendering library and several applications
(viewer, editor, page manipulator, document comparison, command line tool),
for Windows and Linux.

inkpage started in November 2026 from the PDF4QT 1.7.0.0 release, with the
goal of a lighter editor for Linux only:

- Only the editor application is kept. The viewer, PageMaster, Diff,
  LaunchPad, PdfTool, the JBIG2 viewer, the example generator and the
  Windows packaging (Wix installer) were removed.
- The text-to-speech player (Qt TextToSpeech) was removed.
- The Dimensions and Output Preview plugins were removed; the Audio Book,
  Redact and Soft Proofing plugins are kept in the sources but not built.
- The build uses the distribution packages instead of vcpkg.
- New features: a page management toolbar (blank pages, duplicate, delete,
  move, page backgrounds) and toolbars that can be moved from the top to the
  right side of the window.

The PDF engine, the page widget and the plugins come from PDF4QT. Their
source folders keep the upstream names (`Pdf4QtLibCore`, `Pdf4QtLibWidgets`,
`Pdf4QtLibGui`, ...) so that upstream fixes can be merged more easily. The
original PDF4QT README and release notes are kept in `PDF4QT_README.md` and
`PDF4QT_RELEASES.txt`.

## 2. ACKNOWLEDGEMENTS

This software is based in part on the work of the Independent JPEG Group.

Portions of this software are copyright © 2019 The FreeType
Project (www.freetype.org). All rights reserved.

The inkpage icon is "Widget Layout Page Paper Office" by efeaco, from the
Duotone Interface Icons collection on SVG Repo
(https://www.svgrepo.com/svg/381941/widget-layout-page-paper-office),
licensed under the Creative Commons Attribution License.

## 3. LEGAL ISSUES

inkpage is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See the `LICENSE` file.

Copyright (C) 2026 Ludovic Mercier.

inkpage is derived from PDF4QT 1.7.0.0, copyright (c) 2018-2026 Jakub Melka
and Contributors. PDF4QT was originally licensed under the GNU Lesser General
Public License version 3 (LGPLv3) and has been relicensed under the MIT
License by its original author since April 27, 2025. The MIT License allows
the PDF4QT code to be used in a GPL program, on condition that its copyright
and permission notice are kept: they are in the `PDF4QT_LICENSE` file, and in
the header of the source files that come from PDF4QT. These files remain
available under the MIT License as well; the files written for inkpage carry
a GPL header.

This software also uses several third-party libraries, and users must comply
with the licenses of those third-party components. Their license texts are in
the `3rdparty_licenses` folder. All of them can be used in a GPLv3 program.
FreeType is dual-licensed under the FreeType License (FTL) or the GNU GPL
version 2 or later; inkpage uses it under the terms of the GPL.

## 4. FEATURES

New in inkpage:

- [x] page management toolbar and menu: add a blank page after the current
      page, insert a blank page before it, duplicate, delete, move up/down
- [x] page backgrounds (plain, ruled) for the current page or all pages,
      drawn under the page content
- [x] toolbars on the top or on the right side of the window
      (View > Toolbar Position)

From PDF4QT (the list is not complete):

- [x] multithreading support
- [x] encryption
- [x] color management
- [x] optional content handling
- [x] text layout analysis
- [x] signature validation
- [x] annotations
- [x] form filling
- [x] editation
- [x] file attachments
- [x] optimalization (compressing documents)
- [x] internal structure inspector
- [x] static XFA support (readonly, simple XFA only)
- [x] electronically/digitally sign documents
- [x] public key security encryption
- [ ] OCR - text recognition of scanned documents (Tesseract), not built yet

## 5. THIRD PARTY LIBRARIES

Several third-party libraries are used.

1. Qt 6, https://www.qt.io/, LGPL license used
2. libjpeg, see https://www.ijg.org/, IJG license
3. libpng, http://www.libpng.org/, libpng license (PNG Reference Library License)
4. FreeType, see https://www.freetype.org/index.html, GPLv2+ option of its FTL/GPL dual license
5. OpenJPEG, implementing Jpeg2000, see https://www.openjpeg.org/, 2-clause BSD license
6. OpenSSL, https://www.openssl.org/, Apache 2.0 license
7. LittleCMS, http://www.littlecms.com/, MIT license
8. zlib, https://zlib.net/, zlib license
9. oneTBB, https://github.com/uxlfoundation/oneTBB, Apache 2.0 license
10. Blend2D, https://blend2d.com/, zlib license (with AsmJit, https://asmjit.com/, zlib license)
11. Clipper2, https://github.com/AngusJohnson/Clipper2, Boost Software License 1.0

Optional, for the OCR feature (`PDF4QT_ENABLE_OCR`, currently off):

12. Tesseract OCR, https://github.com/tesseract-ocr/tesseract, Apache 2.0 license
13. Leptonica, http://www.leptonica.org/, BSD 2-clause license (dependency of Tesseract)
14. Tesseract language models (tessdata_fast / tessdata_best), https://github.com/tesseract-ocr/tessdata_fast, Apache 2.0 license

## 6. COMPILING

inkpage is built on Linux with CMake, a C++20 compiler and the packages of
the distribution. It is developed on Debian 13 (GCC 14, Qt 6.8.2).

### Dependencies

Install the development packages (Debian names):

    sudo apt install build-essential cmake ninja-build git \
        qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools \
        liblcms2-dev libopenjp2-7-dev libtbb-dev libssl-dev \
        libfreetype-dev libjpeg-dev libpng-dev zlib1g-dev

Blend2D and Clipper2 are not packaged by Debian. Their sources are on
GitHub; inkpage is tested with these versions (the ones used by PDF4QT
1.7.0.0):

| Library                    | Repository                                | Commit    |
| -------------------------- | ----------------------------------------- | --------- |
| AsmJit (needed by Blend2D) | https://github.com/asmjit/asmjit          | `c878602` |
| Blend2D                    | https://github.com/blend2d/blend2d        | `6dbc2ce` |
| Clipper2                   | https://github.com/AngusJohnson/Clipper2  | `21ebba0` |

The script `scripts/build-deps.sh` clones them at these commits, builds
them and installs them into `<DEPS_DIR>/install` (`DEPS_DIR` defaults to
`../deps`, next to the inkpage repository). It needs `git`, `cmake` and
`ninja`:

    DEPS=$HOME/inkpage-deps
    scripts/build-deps.sh $DEPS

To build them by hand instead:

    DEPS=$HOME/inkpage-deps
    git clone https://github.com/asmjit/asmjit
    git -C asmjit checkout c87860217e43e2a06060fcaae5b468f6a55b9963
    git clone https://github.com/blend2d/blend2d
    git -C blend2d checkout 6dbc2cefbc996379e07104e34519a440b49b15d7
    git clone https://github.com/AngusJohnson/Clipper2
    git -C Clipper2 checkout 21ebba05db8894f0c7217ad35ea518080f324946

    cmake -S blend2d -B build-blend2d -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=$DEPS/install -DBLEND2D_STATIC=OFF \
          -DASMJIT_DIR=$PWD/asmjit
    cmake --build build-blend2d && cmake --install build-blend2d

    cmake -S Clipper2/CPP -B build-clipper2 -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=$DEPS/install -DBUILD_SHARED_LIBS=ON \
          -DCLIPPER2_EXAMPLES=OFF -DCLIPPER2_TESTS=OFF -DCLIPPER2_UTILS=ON
    cmake --build build-clipper2 && cmake --install build-clipper2

The sources (`src/`) and build trees (`build/`) are only needed to build the
libraries and can be deleted. inkpage needs `$DEPS/install` to build, and at
run time, since the libraries are loaded from `$DEPS/install/lib`.

### Building inkpage

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_PREFIX_PATH=$DEPS/install \
          -DCMAKE_BUILD_RPATH=$DEPS/install/lib \
          -DPDF4QT_ENABLE_OCR=OFF
    cmake --build build

The application is `build/bin/inkpage`, the plugins are in
`build/lib/inkpage`. Run the unit tests with `ctest --test-dir build`.

### CMake options

|                  Option      |     Description                                          |
| ---------------------------- | -------------------------------------------------------- |
| `CMAKE_BUILD_TYPE`           | Release or Debug                                         |
| `CMAKE_PREFIX_PATH`          | Prefix where Blend2D and Clipper2 are installed          |
| `PDF4QT_BUILD_TESTS`         | Build the unit tests, ON by default                      |
| `PDF4QT_ENABLE_OCR`          | Build the OCR feature (Tesseract), set it to OFF for now |
| `INKPAGE_BUILD_DEV_TOOLS`    | Build the developer tools (CodeGenerator), OFF by default|
| `INKPAGE_BUILD_DISABLED_PLUGINS` | Build the deactivated plugins (AudioBook, Redact, SoftProofing), OFF by default |

## 7. DISCLAIMER

inkpage is a personal project built on the work of Jakub Melka, who wrote
PDF4QT in his free time. Bugs in inkpage should be reported to inkpage, not
to the PDF4QT project. If you find PDF4QT useful, consider supporting its
author: [:heart: Sponsor](https://github.com/sponsors/JakubMelka).
