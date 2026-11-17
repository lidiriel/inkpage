# inkpage - a simple PDF editor for Linux
# Copyright (C) 2026 Ludovic Mercier
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Debian package of inkpage, created by "cpack -G DEB" in a build configured
# with CMAKE_INSTALL_PREFIX=/usr (see scripts/build-deb.sh).
#
# Blend2D and Clipper2 are not packaged by Debian: with INKPAGE_BUNDLE_DEPS they
# are installed with the private libraries of inkpage. The other dependencies are
# Debian packages, found by dpkg-shlibdeps.

option(INKPAGE_BUNDLE_DEPS "Install Blend2D and Clipper2 with the private libraries (for a package of a distribution, which does not provide them)" OFF)

if(INKPAGE_BUNDLE_DEPS)
    install(IMPORTED_RUNTIME_ARTIFACTS blend2d::blend2d Clipper2::Clipper2
            LIBRARY DESTINATION ${PDF4QT_INSTALL_LIB_DIR})

    # CPACK_STRIP_FILES strips the targets of the project only, the libraries
    # built by scripts/build-deps.sh keep their symbols
    if(CMAKE_STRIP)
        install(CODE "
            foreach(library \"$<TARGET_FILE_NAME:blend2d::blend2d>\" \"$<TARGET_FILE_NAME:Clipper2::Clipper2>\")
                execute_process(COMMAND \"${CMAKE_STRIP}\" --strip-unneeded --remove-section=.comment --remove-section=.note
                                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${PDF4QT_INSTALL_LIB_DIR}/\${library}\")
            endforeach()
        ")
    endif()
endif()

set(INKPAGE_MAINTAINER "Ludovic Mercier <ludovic@scilink.net>")

# The version of the package is the version of inkpage (git tag, see
# cmake/InkpageVersion.cmake). The files of the package carrying a version must
# agree with it:
# - a release (the tag vX.Y.Z) must be described by packaging/changelog and by the
#   releases of the metainfo (scripts/release.sh checks it before it creates the tag)
# - a development snapshot gets an entry of its own in the changelog of the package
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/changelog INKPAGE_CHANGELOG_FIRST_LINE LIMIT_COUNT 1)
string(REGEX MATCH "^inkpage \\(([^)]+)\\)" INKPAGE_CHANGELOG_MATCH "${INKPAGE_CHANGELOG_FIRST_LINE}")
set(INKPAGE_CHANGELOG_VERSION "${CMAKE_MATCH_1}")
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/Desktop/io.github.inkpage.metainfo.xml INKPAGE_METAINFO)
if(INKPAGE_IS_RELEASE)
    if(NOT INKPAGE_CHANGELOG_VERSION STREQUAL INKPAGE_VERSION)
        message(FATAL_ERROR "Release ${INKPAGE_VERSION} (git tag v${INKPAGE_VERSION}), but the last entry of packaging/changelog is ${INKPAGE_CHANGELOG_VERSION}: add the entry of the release.")
    endif()
    if(NOT INKPAGE_METAINFO MATCHES "<release version=\"${INKPAGE_VERSION}\"")
        message(FATAL_ERROR "Release ${INKPAGE_VERSION} (git tag v${INKPAGE_VERSION}) is missing in the releases of Desktop/io.github.inkpage.metainfo.xml.")
    endif()
endif()

# Date of the commit: the files generated from it do not depend on the time of the build
set(INKPAGE_COMMIT_DATE_RFC2822 "")
set(INKPAGE_COMMIT_TIME "")
if(GIT_EXECUTABLE AND INKPAGE_GIT_COMMIT)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" log -1 --format=%cD
                    OUTPUT_VARIABLE INKPAGE_COMMIT_DATE_RFC2822 ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" log -1 --format=%ct
                    OUTPUT_VARIABLE INKPAGE_COMMIT_TIME ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(INKPAGE_PACKAGING_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/packaging)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/packaging/changelog INKPAGE_CHANGELOG)
if(NOT INKPAGE_CHANGELOG_VERSION STREQUAL INKPAGE_FULL_VERSION)
    set(INKPAGE_SNAPSHOT_DATE "${INKPAGE_COMMIT_DATE_RFC2822}")
    if(NOT INKPAGE_SNAPSHOT_DATE)
        string(TIMESTAMP INKPAGE_SNAPSHOT_DATE "%a, %d %b %Y %H:%M:%S +0000" UTC)
    endif()
    set(INKPAGE_SNAPSHOT_COMMIT "")
    if(INKPAGE_GIT_COMMIT)
        set(INKPAGE_SNAPSHOT_COMMIT " (commit ${INKPAGE_GIT_COMMIT})")
    endif()
    string(PREPEND INKPAGE_CHANGELOG
           "inkpage (${INKPAGE_FULL_VERSION}) UNRELEASED; urgency=medium\n\n"
           "  * Development snapshot${INKPAGE_SNAPSHOT_COMMIT}, see the git history.\n\n"
           " -- ${INKPAGE_MAINTAINER}  ${INKPAGE_SNAPSHOT_DATE}\n\n")
endif()
# Copied only when it changes, so the package files are not rebuilt otherwise
file(WRITE ${INKPAGE_PACKAGING_BUILD_DIR}/changelog.new "${INKPAGE_CHANGELOG}")
configure_file(${INKPAGE_PACKAGING_BUILD_DIR}/changelog.new ${INKPAGE_PACKAGING_BUILD_DIR}/changelog COPYONLY)

# Man page: version of inkpage, month of the commit
if(INKPAGE_COMMIT_TIME)
    set(INKPAGE_SAVED_SOURCE_DATE_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
    set(ENV{SOURCE_DATE_EPOCH} ${INKPAGE_COMMIT_TIME})
    string(TIMESTAMP INKPAGE_MAN_DATE "%B %Y" UTC)
    set(ENV{SOURCE_DATE_EPOCH} "${INKPAGE_SAVED_SOURCE_DATE_EPOCH}")
else()
    string(TIMESTAMP INKPAGE_MAN_DATE "%B %Y" UTC)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packaging/inkpage.1.in ${INKPAGE_PACKAGING_BUILD_DIR}/inkpage.1 @ONLY)

# Files required by the Debian policy: copyright and changelog of the package
set(INKPAGE_INSTALL_DOC_DIR ${PDF4QT_INSTALL_SHARE_DIR}/doc/inkpage)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/copyright DESTINATION ${INKPAGE_INSTALL_DOC_DIR})

find_program(INKPAGE_GZIP gzip)
if(INKPAGE_GZIP)
    set(INKPAGE_MAN_PAGE_GZ ${INKPAGE_PACKAGING_BUILD_DIR}/inkpage.1.gz)
    add_custom_command(OUTPUT ${INKPAGE_MAN_PAGE_GZ}
                       COMMAND ${INKPAGE_GZIP} -9 -n -c ${INKPAGE_PACKAGING_BUILD_DIR}/inkpage.1 > ${INKPAGE_MAN_PAGE_GZ}
                       DEPENDS ${INKPAGE_PACKAGING_BUILD_DIR}/inkpage.1
                       VERBATIM)
    add_custom_target(inkpage_man_page ALL DEPENDS ${INKPAGE_MAN_PAGE_GZ})
    install(FILES ${INKPAGE_MAN_PAGE_GZ} DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/man/man1)

    # -n: no name and time stamp, the compressed file is reproducible
    set(INKPAGE_CHANGELOG_GZ ${INKPAGE_PACKAGING_BUILD_DIR}/changelog.gz)
    add_custom_command(OUTPUT ${INKPAGE_CHANGELOG_GZ}
                       COMMAND ${INKPAGE_GZIP} -9 -n -c ${INKPAGE_PACKAGING_BUILD_DIR}/changelog > ${INKPAGE_CHANGELOG_GZ}
                       DEPENDS ${INKPAGE_PACKAGING_BUILD_DIR}/changelog
                       VERBATIM)
    add_custom_target(inkpage_changelog ALL DEPENDS ${INKPAGE_CHANGELOG_GZ})
    install(FILES ${INKPAGE_CHANGELOG_GZ} DESTINATION ${INKPAGE_INSTALL_DOC_DIR})
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "inkpage")
set(CPACK_PACKAGE_VERSION ${INKPAGE_FULL_VERSION})
set(CPACK_DEBIAN_PACKAGE_VERSION ${INKPAGE_FULL_VERSION})
set(CPACK_PACKAGE_VENDOR "Ludovic Mercier")
set(CPACK_PACKAGE_CONTACT "${INKPAGE_MAINTAINER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "simple PDF editor")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "inkpage")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/lidiriel/inkpage")
# Long description: CPack adds the summary as the first line and formats the text
# (indentation, an empty line becomes the paragraph separator " .")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "inkpage edits PDF documents: annotations (inline text, images, shapes,
freehand drawing, highlighter, sticky notes), page content and pages
(add, insert, duplicate, delete, move, ruled backgrounds).

The text of scanned documents is recognized with Tesseract (OCR): the
recognized text is reviewed and written as an invisible, searchable text
layer. The language models are the tesseract-ocr-* packages.

It is based on PDF4QT by Jakub Melka.")

# The dependencies are found from the libraries used by the binaries. The private
# libraries are found in the package itself.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
    ${CMAKE_BINARY_DIR}/_CPack_Packages/Linux/DEB/inkpage-${INKPAGE_FULL_VERSION}-Linux/${CMAKE_INSTALL_PREFIX}/${PDF4QT_INSTALL_LIB_DIR})
# The Qt plugins loaded at run time are not seen by dpkg-shlibdeps
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-svg-plugins, qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins")

# Language models of the OCR (the engine libraries are found by dpkg-shlibdeps).
# OCR is optional, so the models are recommended: English, the orientation data,
# and French; the other languages are the other tesseract-ocr-* packages.
if(PDF4QT_ENABLE_OCR)
    string(APPEND CPACK_DEBIAN_PACKAGE_RECOMMENDS ", tesseract-ocr-eng, tesseract-ocr-osd, tesseract-ocr-fra")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "tesseract-ocr-all")
endif()

# Directories are owned by the package with the usual permissions
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

include(CPack)
