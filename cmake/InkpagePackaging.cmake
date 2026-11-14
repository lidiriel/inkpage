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

# Files required by the Debian policy: copyright and changelog of the package
set(INKPAGE_INSTALL_DOC_DIR ${PDF4QT_INSTALL_SHARE_DIR}/doc/inkpage)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/copyright DESTINATION ${INKPAGE_INSTALL_DOC_DIR})

find_program(INKPAGE_GZIP gzip)
if(INKPAGE_GZIP)
    set(INKPAGE_MAN_PAGE_GZ ${CMAKE_CURRENT_BINARY_DIR}/packaging/inkpage.1.gz)
    add_custom_command(OUTPUT ${INKPAGE_MAN_PAGE_GZ}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/packaging
                       COMMAND ${INKPAGE_GZIP} -9 -n -c ${CMAKE_CURRENT_SOURCE_DIR}/packaging/inkpage.1 > ${INKPAGE_MAN_PAGE_GZ}
                       DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/inkpage.1
                       VERBATIM)
    add_custom_target(inkpage_man_page ALL DEPENDS ${INKPAGE_MAN_PAGE_GZ})
    install(FILES ${INKPAGE_MAN_PAGE_GZ} DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/man/man1)

    # -n: no name and time stamp, the compressed file is reproducible
    set(INKPAGE_CHANGELOG_GZ ${CMAKE_CURRENT_BINARY_DIR}/packaging/changelog.gz)
    add_custom_command(OUTPUT ${INKPAGE_CHANGELOG_GZ}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/packaging
                       COMMAND ${INKPAGE_GZIP} -9 -n -c ${CMAKE_CURRENT_SOURCE_DIR}/packaging/changelog > ${INKPAGE_CHANGELOG_GZ}
                       DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/changelog
                       VERBATIM)
    add_custom_target(inkpage_changelog ALL DEPENDS ${INKPAGE_CHANGELOG_GZ})
    install(FILES ${INKPAGE_CHANGELOG_GZ} DESTINATION ${INKPAGE_INSTALL_DOC_DIR})
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "inkpage")
set(CPACK_PACKAGE_VERSION ${INKPAGE_VERSION})
set(CPACK_PACKAGE_VENDOR "Ludovic Mercier")
set(CPACK_PACKAGE_CONTACT "Ludovic Mercier <ludovic@scilink.net>")
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

It is based on PDF4QT by Jakub Melka.")

# The dependencies are found from the libraries used by the binaries. The private
# libraries are found in the package itself.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
    ${CMAKE_BINARY_DIR}/_CPack_Packages/Linux/DEB/inkpage-${INKPAGE_VERSION}-Linux/${CMAKE_INSTALL_PREFIX}/${PDF4QT_INSTALL_LIB_DIR})
# The Qt plugins loaded at run time are not seen by dpkg-shlibdeps
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-svg-plugins, qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins")

# Directories are owned by the package with the usual permissions
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

include(CPack)
