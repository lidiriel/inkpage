# MIT License
#
# Copyright (c) 2018-2026 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.16)

# inkpage - a simple PDF editor for Linux, forked from PDF4QT 1.7.0.0.
# The PDF engine keeps its PDF4QT version and its PDF4QT_* option names.
# The version of inkpage is the git tag of the release (vX.Y.Z), see
# cmake/InkpageVersion.cmake and scripts/release.sh; INKPAGE_BASE_VERSION is
# used only without a git checkout or a tag.
set(INKPAGE_BASE_VERSION 0.2.0)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InkpageVersion.cmake)
inkpage_compute_version(${CMAKE_CURRENT_SOURCE_DIR} ${INKPAGE_BASE_VERSION})
message(STATUS "inkpage version: ${INKPAGE_FULL_VERSION}")
set(PDF4QT_VERSION 1.7.0.0)
message(STATUS "Build type - CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

project(inkpage VERSION ${INKPAGE_VERSION} LANGUAGES CXX)

option(PDF4QT_INSTALL_INCLUDE "Install include headers of the PDF libraries" OFF)
option(PDF4QT_FLATPAK_BUILD "Special flag for Flatpak" OFF)

add_compile_definitions(PDF4QT_PROJECT_VERSION="${PDF4QT_VERSION}")
add_compile_definitions(QT_NO_EMIT)

if(PDF4QT_FLATPAK_BUILD)
    add_compile_definitions(PDF4QT_FLATPAK_BUILD)
endif()

option(PDF4QT_BUILD_ONLY_CORE_LIBRARY "Build only core library" OFF)
option(INKPAGE_BUILD_DEV_TOOLS "Build the developer tools (CodeGenerator)" OFF)
option(INKPAGE_BUILD_DISABLED_PLUGINS "Build the deactivated plugins (AudioBook, Redact, Scanner, SoftProofing)" OFF)
option(PDF4QT_BUILD_TESTS "Build UnitTests" ON)
option(PDF4QT_ENABLE_COVERAGE "Instrument the code for the source based code coverage of clang (llvm-cov)" OFF)
option(PDF4QT_ENABLE_OCR "Build the OCR support with the Tesseract engine (requires Tesseract and Leptonica)" ON)
option(PDF4QT_OCR_REQUIRED "Release gate of the OCR support: fail the configuration instead of disabling OCR silently when the Tesseract engine is not found, when a built-in language model declared by a manifest (ocr/tesseract/<profile>/manifest.json) has no archive or is not extracted, or when a runtime library of the engine is missing in the installer. Turn it on for the distributed builds, which must ship the OCR engine and the declared built-in model set (OPS-01, LANG-01, LANG-02)" OFF)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core LinguistTools)

if(PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg Xml Network)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Xml PrintSupport Concurrent Network)
endif()

if(PDF4QT_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES en de cs es ko zh_CN zh_TW fr tr ru)

find_package(OpenSSL 3.0 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenJPEG CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(blend2d CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)

if(PDF4QT_ENABLE_OCR)
    find_package(Tesseract CONFIG)
    if(NOT Tesseract_FOUND)
        if(PDF4QT_OCR_REQUIRED)
            message(FATAL_ERROR "Tesseract was not found, but PDF4QT_OCR_REQUIRED is ON: the OCR support must be built. Install the tesseract package (vcpkg, the versions are pinned in vcpkg.json) or set PDF4QT_OCR_REQUIRED=OFF.")
        endif()
        message(WARNING "Tesseract was not found, OCR support is disabled. Install the tesseract package (vcpkg) or set PDF4QT_ENABLE_OCR=OFF.")
        set(PDF4QT_ENABLE_OCR OFF)
    endif()
elseif(PDF4QT_OCR_REQUIRED)
    message(FATAL_ERROR "PDF4QT_OCR_REQUIRED is ON, but the OCR support is turned off by PDF4QT_ENABLE_OCR=OFF.")
endif()

# Severity of a problem of the distributed OCR data: a release build must not ship an incomplete OCR
if(PDF4QT_OCR_REQUIRED)
    set(PDF4QT_OCR_PROBLEM_SEVERITY FATAL_ERROR)
else()
    set(PDF4QT_OCR_PROBLEM_SEVERITY WARNING)
endif()

# lcms usually doesn't provide a config file
find_library(LCMS2_LIBRARIES lcms2 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTORCC_OPTIONS "--threshold;0;--compress;9")

if (MSVC)
    add_compile_options(/bigobj /W4 /wd5054 /wd4127 /wd4702)
endif()

if(PDF4QT_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "The code coverage requires the clang compiler (clang or clang-cl).")
    endif()

    # Source based code coverage of clang - the profile is written by the instrumented
    # binaries when they exit, see the file named by the environment variable
    # LLVM_PROFILE_FILE. The profile runtime is linked automatically by the driver, but
    # the MSVC linker does not know its directory, so it is added explicitly.
    # The MC/DC coverage needs the conditions of the decisions instrumented as well
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping -fcoverage-mcdc)

    if(MSVC)
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} /clang:-print-resource-dir
                        OUTPUT_VARIABLE PDF4QT_CLANG_RESOURCE_DIR
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        link_directories("${PDF4QT_CLANG_RESOURCE_DIR}/lib/windows")
    else()
        add_link_options(-fprofile-instr-generate)
    endif()
endif()

if (MINGW)
    add_compile_options("-Wa,-mbig-obj")
endif()

if (UNIX AND NOT APPLE AND CMAKE_COMPILER_IS_GNUCXX)
    set(LINUX_GCC ON)
endif()

if (UNIX AND NOT APPLE)
    set(PDF4QT_LINUX ON)
endif()

if (LINUX_GCC)
    find_package(TBB REQUIRED)
endif()

option(PDF4QT_INSTALL_TO_USR "Install to usr directory" OFF)

if(PDF4QT_LINUX)
    # inkpage: the libraries are private libraries of the application (no headers,
    # no stable interface), they are installed in a directory of their own
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_LIBDIR}/inkpage)
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
else()
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
endif()

if(PDF4QT_INSTALL_TO_USR)
    set(PDF4QT_INSTALL_LIB_DIR usr/${PDF4QT_INSTALL_LIB_DIR_IMPL})
    set(PDF4QT_INSTALL_BIN_DIR usr/${PDF4QT_INSTALL_BIN_DIR_IMPL})
    set(PDF4QT_INSTALL_SHARE_DIR usr/${CMAKE_INSTALL_DATAROOTDIR})
    set(PDF4QT_INSTALL_INCLUDE_DIR usr/${CMAKE_INSTALL_INCLUDEDIR})
else()
    set(PDF4QT_INSTALL_LIB_DIR ${PDF4QT_INSTALL_LIB_DIR_IMPL})
    set(PDF4QT_INSTALL_BIN_DIR ${PDF4QT_INSTALL_BIN_DIR_IMPL})
    set(PDF4QT_INSTALL_SHARE_DIR ${CMAKE_INSTALL_DATAROOTDIR})
    set(PDF4QT_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(PDF4QT_LINUX)
    # The plugins are separated from the private libraries: every *.so file of the
    # plugin directory is loaded as a plugin
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/plugins)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_SHARE_DIR}/inkpage/translations)
else()
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/pdfplugins)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_BIN_DIR}/translations)
endif()

# Built-in OCR data (language models of the Tesseract engine)
if(PDF4QT_LINUX)
    set(PDF4QT_INSTALL_OCR_DIR ${PDF4QT_INSTALL_SHARE_DIR}/inkpage/ocr)
else()
    set(PDF4QT_INSTALL_OCR_DIR ${PDF4QT_INSTALL_BIN_DIR}/ocr)
endif()

# License texts of inkpage and of the third-party libraries, shown by the About dialog
if(PDF4QT_LINUX)
    set(INKPAGE_INSTALL_LICENSES_DIR ${PDF4QT_INSTALL_SHARE_DIR}/inkpage/licenses)
else()
    set(INKPAGE_INSTALL_LICENSES_DIR ${PDF4QT_INSTALL_BIN_DIR}/licenses)
endif()

# The installed binaries find the private libraries relative to their own location:
# the application (bin), the libraries (lib/inkpage) and the plugins (lib/inkpage/plugins)
if(PDF4QT_LINUX)
    cmake_path(RELATIVE_PATH PDF4QT_INSTALL_LIB_DIR
               BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
               OUTPUT_VARIABLE INKPAGE_LIB_RELATIVE_PATH)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/${INKPAGE_LIB_RELATIVE_PATH};$ORIGIN;$ORIGIN/..")
endif()

# Relative paths are used by the config
cmake_path(RELATIVE_PATH PDF4QT_PLUGINS_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE PDF4QT_PLUGINS_RELATIVE_PATH)
cmake_path(RELATIVE_PATH PDF4QT_INSTALL_TRANS_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE PDF4QT_TRANSLATIONS_RELATIVE_PATH)
cmake_path(RELATIVE_PATH PDF4QT_INSTALL_OCR_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE PDF4QT_OCR_DATA_RELATIVE_PATH)
cmake_path(RELATIVE_PATH INKPAGE_INSTALL_LICENSES_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE INKPAGE_LICENSES_RELATIVE_PATH)
configure_file(config.h.cmake config.h)

# The license texts are copied into the build tree (so the application can be run
# from the build directory) and installed from there
set(INKPAGE_LICENSES_BUILD_DIR ${CMAKE_BINARY_DIR}/${INKPAGE_INSTALL_LICENSES_DIR})
configure_file(LICENSE ${INKPAGE_LICENSES_BUILD_DIR}/inkpage_LICENSE.txt COPYONLY)
configure_file(PDF4QT_LICENSE ${INKPAGE_LICENSES_BUILD_DIR}/PDF4QT_LICENSE.txt COPYONLY)
file(GLOB INKPAGE_3RDPARTY_LICENSES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty_licenses/*)
foreach(INKPAGE_LICENSE_FILE IN LISTS INKPAGE_3RDPARTY_LICENSES)
    get_filename_component(INKPAGE_LICENSE_NAME ${INKPAGE_LICENSE_FILE} NAME)
    configure_file(${INKPAGE_LICENSE_FILE} ${INKPAGE_LICENSES_BUILD_DIR}/${INKPAGE_LICENSE_NAME} COPYONLY)
endforeach()
install(DIRECTORY ${INKPAGE_LICENSES_BUILD_DIR}/ DESTINATION ${INKPAGE_INSTALL_LICENSES_DIR})

add_subdirectory(Pdf4QtLibCore)

if(PDF4QT_ENABLE_OCR)
    add_subdirectory(Pdf4QtOcrTesseract)
endif()

# Built-in OCR models are stored in the repository as xz compressed archives, one
# model per archive (ocr/tesseract/<profile>/tessdata/<language>.tar.xz, script models
# in tessdata/script/<Name>.tar.xz), because a traineddata file compresses to about
# a third of its size. They are extracted into the build tree (so the applications can
# be run from the build directory) and installed from there with the applications.
# Nothing is downloaded during a build. A build without the OCR engine does not carry
# the models.
#
# The manifest ocr/tesseract/<profile>/manifest.json declares the built-in model set
# of the profile. Every model of a manifest must be distributed (LANG-02), so the
# archives are verified against the manifests and the extracted models are verified
# after the extraction. With PDF4QT_OCR_REQUIRED a problem fails the configuration.
# PDF4QT_OCR_DISTRIBUTED_FILES lists every distributed OCR data file relative to the
# OCR data directory (manifests, license texts, models); the installer is generated
# from it.
if(PDF4QT_ENABLE_OCR)
    set(PDF4QT_OCR_BUILD_DATA_DIR ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_OCR_DIR})
    set(PDF4QT_OCR_DISTRIBUTED_FILES "")
    set(PDF4QT_OCR_MISSING_ARCHIVES "")
    set(PDF4QT_OCR_MODELS_WITHOUT_ARCHIVE "")

    file(GLOB PDF4QT_OCR_MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/ocr/tesseract/*/manifest.json)
    list(SORT PDF4QT_OCR_MANIFESTS)
    if(NOT PDF4QT_OCR_MANIFESTS)
        message(${PDF4QT_OCR_PROBLEM_SEVERITY} "No manifest of a built-in OCR model set was found in ocr/tesseract/<profile>/manifest.json. OCR will work only with downloaded or imported languages.")
    endif()

    foreach(PDF4QT_OCR_MANIFEST IN LISTS PDF4QT_OCR_MANIFESTS)
        cmake_path(GET PDF4QT_OCR_MANIFEST PARENT_PATH PDF4QT_OCR_PROFILE_DIR)
        cmake_path(GET PDF4QT_OCR_PROFILE_DIR FILENAME PDF4QT_OCR_PROFILE)
        file(READ ${PDF4QT_OCR_MANIFEST} PDF4QT_OCR_MANIFEST_JSON)
        list(APPEND PDF4QT_OCR_DISTRIBUTED_FILES tesseract/${PDF4QT_OCR_PROFILE}/manifest.json)

        # License of the model set (the license of the models is not the license of the engine)
        string(JSON PDF4QT_OCR_LICENSE_FILE ERROR_VARIABLE PDF4QT_OCR_JSON_ERROR GET "${PDF4QT_OCR_MANIFEST_JSON}" licenseFile)
        if(PDF4QT_OCR_JSON_ERROR OR NOT EXISTS ${PDF4QT_OCR_PROFILE_DIR}/${PDF4QT_OCR_LICENSE_FILE})
            message(${PDF4QT_OCR_PROBLEM_SEVERITY} "The manifest ocr/tesseract/${PDF4QT_OCR_PROFILE}/manifest.json does not declare an existing license file of the built-in models (licenseFile).")
        else()
            list(APPEND PDF4QT_OCR_DISTRIBUTED_FILES tesseract/${PDF4QT_OCR_PROFILE}/${PDF4QT_OCR_LICENSE_FILE})
        endif()

        string(JSON PDF4QT_OCR_MODEL_COUNT ERROR_VARIABLE PDF4QT_OCR_JSON_ERROR LENGTH "${PDF4QT_OCR_MANIFEST_JSON}" models)
        if(PDF4QT_OCR_JSON_ERROR)
            message(FATAL_ERROR "The manifest ocr/tesseract/${PDF4QT_OCR_PROFILE}/manifest.json has no list of models: ${PDF4QT_OCR_JSON_ERROR}")
        endif()
        if(PDF4QT_OCR_MODEL_COUNT GREATER 0)
            math(EXPR PDF4QT_OCR_LAST_MODEL_INDEX "${PDF4QT_OCR_MODEL_COUNT} - 1")
            foreach(PDF4QT_OCR_MODEL_INDEX RANGE ${PDF4QT_OCR_LAST_MODEL_INDEX})
                string(JSON PDF4QT_OCR_LANGUAGE GET "${PDF4QT_OCR_MANIFEST_JSON}" models ${PDF4QT_OCR_MODEL_INDEX} language)
                string(JSON PDF4QT_OCR_MODEL_FILE_NAME ERROR_VARIABLE PDF4QT_OCR_JSON_ERROR GET "${PDF4QT_OCR_MANIFEST_JSON}" models ${PDF4QT_OCR_MODEL_INDEX} fileName)
                if(PDF4QT_OCR_JSON_ERROR)
                    set(PDF4QT_OCR_MODEL_FILE_NAME ${PDF4QT_OCR_LANGUAGE}.traineddata)
                endif()

                # Language "ces" -> tessdata/ces.tar.xz, language "script/Latin" -> tessdata/script/Latin.tar.xz
                set(PDF4QT_OCR_MODEL tesseract/${PDF4QT_OCR_PROFILE}/tessdata/${PDF4QT_OCR_MODEL_FILE_NAME})
                set(PDF4QT_OCR_ARCHIVE tesseract/${PDF4QT_OCR_PROFILE}/tessdata/${PDF4QT_OCR_LANGUAGE}.tar.xz)
                list(APPEND PDF4QT_OCR_DISTRIBUTED_FILES ${PDF4QT_OCR_MODEL})
                if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ocr/${PDF4QT_OCR_ARCHIVE})
                    list(APPEND PDF4QT_OCR_MISSING_ARCHIVES ocr/${PDF4QT_OCR_ARCHIVE})
                    list(APPEND PDF4QT_OCR_MODELS_WITHOUT_ARCHIVE ${PDF4QT_OCR_MODEL})
                endif()
            endforeach()
        endif()
    endforeach()

    if(PDF4QT_OCR_MISSING_ARCHIVES)
        list(JOIN PDF4QT_OCR_MISSING_ARCHIVES "\n  " PDF4QT_OCR_MISSING_ARCHIVES_TEXT)
        message(${PDF4QT_OCR_PROBLEM_SEVERITY} "Archives of built-in OCR language models declared by the manifests are missing:\n  ${PDF4QT_OCR_MISSING_ARCHIVES_TEXT}\nThe distributed built-in model set would be incomplete. Run \"python ocr/tools/generate_catalog.py --fetch-builtin\" to restore them.")
    endif()

    file(GLOB PDF4QT_OCR_BUILTIN_ARCHIVES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/ocr
         ${CMAKE_CURRENT_SOURCE_DIR}/ocr/tesseract/*/tessdata/*.tar.xz
         ${CMAKE_CURRENT_SOURCE_DIR}/ocr/tesseract/*/tessdata/script/*.tar.xz)
    if(NOT PDF4QT_OCR_BUILTIN_ARCHIVES)
        message(${PDF4QT_OCR_PROBLEM_SEVERITY} "Built-in OCR language models are missing in ocr/tesseract/fast/tessdata. OCR will work only with downloaded or imported languages, and the OCR tests using Tesseract will be skipped. Run \"python ocr/tools/generate_catalog.py --fetch-builtin\" to restore them.")
    endif()

    # The archives themselves are not a part of the distribution
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ocr/tesseract DESTINATION ${PDF4QT_OCR_BUILD_DATA_DIR}
         PATTERN "*.tar.xz" EXCLUDE)

    # A model is extracted again only when its archive has changed. The archives are
    # written with a fixed time stamp, so the time of the extracted file cannot be used.
    set(PDF4QT_OCR_STAMP_DIR ${CMAKE_BINARY_DIR}/CMakeFiles/pdf4qt_ocr_models)
    file(MAKE_DIRECTORY ${PDF4QT_OCR_STAMP_DIR})
    foreach(PDF4QT_OCR_ARCHIVE IN LISTS PDF4QT_OCR_BUILTIN_ARCHIVES)
        set(PDF4QT_OCR_ARCHIVE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ocr/${PDF4QT_OCR_ARCHIVE})
        string(REPLACE "/" "_" PDF4QT_OCR_STAMP_NAME ${PDF4QT_OCR_ARCHIVE})
        set(PDF4QT_OCR_STAMP_FILE ${PDF4QT_OCR_STAMP_DIR}/${PDF4QT_OCR_STAMP_NAME}.stamp)
        string(REPLACE ".tar.xz" ".traineddata" PDF4QT_OCR_MODEL ${PDF4QT_OCR_ARCHIVE})
        set(PDF4QT_OCR_MODEL_FILE ${PDF4QT_OCR_BUILD_DATA_DIR}/${PDF4QT_OCR_MODEL})

        if(EXISTS ${PDF4QT_OCR_MODEL_FILE} AND EXISTS ${PDF4QT_OCR_STAMP_FILE}
           AND NOT ${PDF4QT_OCR_ARCHIVE_FILE} IS_NEWER_THAN ${PDF4QT_OCR_STAMP_FILE})
            continue()
        endif()

        cmake_path(GET PDF4QT_OCR_MODEL_FILE PARENT_PATH PDF4QT_OCR_MODEL_DIR)
        file(MAKE_DIRECTORY ${PDF4QT_OCR_MODEL_DIR})
        file(ARCHIVE_EXTRACT INPUT ${PDF4QT_OCR_ARCHIVE_FILE} DESTINATION ${PDF4QT_OCR_MODEL_DIR})
        if(NOT EXISTS ${PDF4QT_OCR_MODEL_FILE})
            message(FATAL_ERROR "Archive ocr/${PDF4QT_OCR_ARCHIVE} does not hold the language model ${PDF4QT_OCR_MODEL}.")
        endif()
        file(TOUCH ${PDF4QT_OCR_STAMP_FILE})
    endforeach()

    # License texts of the distributed OCR components (OPS-02). They are installed with
    # the OCR data, the About dialog points to them.
    set(PDF4QT_OCR_LICENSE_FILES Tesseract_LICENSE.txt Leptonica_license.txt tessdata_LICENSE.txt)
    foreach(PDF4QT_OCR_LICENSE IN LISTS PDF4QT_OCR_LICENSE_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty_licenses/${PDF4QT_OCR_LICENSE} DESTINATION ${PDF4QT_OCR_BUILD_DATA_DIR}/licenses)
        list(APPEND PDF4QT_OCR_DISTRIBUTED_FILES licenses/${PDF4QT_OCR_LICENSE})
    endforeach()

    # Every declared file must exist in the build tree, from where it is installed
    set(PDF4QT_OCR_MISSING_FILES "")
    foreach(PDF4QT_OCR_FILE IN LISTS PDF4QT_OCR_DISTRIBUTED_FILES)
        if(NOT EXISTS ${PDF4QT_OCR_BUILD_DATA_DIR}/${PDF4QT_OCR_FILE} AND NOT PDF4QT_OCR_FILE IN_LIST PDF4QT_OCR_MODELS_WITHOUT_ARCHIVE)
            list(APPEND PDF4QT_OCR_MISSING_FILES ${PDF4QT_OCR_FILE})
        endif()
    endforeach()
    if(PDF4QT_OCR_MISSING_FILES)
        list(JOIN PDF4QT_OCR_MISSING_FILES "\n  " PDF4QT_OCR_MISSING_FILES_TEXT)
        message(${PDF4QT_OCR_PROBLEM_SEVERITY} "Distributed OCR data files declared by the manifests are missing in ${PDF4QT_OCR_BUILD_DATA_DIR}:\n  ${PDF4QT_OCR_MISSING_FILES_TEXT}")
    endif()

    install(DIRECTORY ${PDF4QT_OCR_BUILD_DATA_DIR}/tesseract DESTINATION ${PDF4QT_INSTALL_OCR_DIR})
    install(DIRECTORY ${PDF4QT_OCR_BUILD_DATA_DIR}/licenses DESTINATION ${PDF4QT_INSTALL_OCR_DIR})
endif()

if(PDF4QT_INSTALL_INCLUDE)
    # Install public headers from Pdf4QtLibCore/include
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Pdf4QtLibCore/sources/
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibCore
            FILES_MATCHING PATTERN "*.h")

    # Install generated export header
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pdf4qtlibcore_export.h
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibCore)

    # Install public headers from Pdf4QtLibWidgets/include
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Pdf4QtLibWidgets/sources/
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibWidgets
            FILES_MATCHING PATTERN "*.h")

    # Install generated export header
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pdf4qtlibwidgets_export.h
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibWidgets)
endif()

if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    add_subdirectory(Pdf4QtLibWidgets)
    if(INKPAGE_BUILD_DEV_TOOLS)
        add_subdirectory(CodeGenerator)
    endif()
    add_subdirectory(Pdf4QtLibGui)
    add_subdirectory(Pdf4QtEditorPlugins)
    add_subdirectory(InkPage)
endif()

if(PDF4QT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(UnitTests)
endif()

# The translations belong to the applications, which are not built with the core library alone
if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)

qt_collect_translation_source_targets(i18n_targets)
message("CMAKE_TRANSLATION_TARGETS = " ${i18n_targets})

set(PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_TRANS_DIR})

message("PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY" ${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY})
message("PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY" ${PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY})

# The .ts files and the output directory of the .qm files are given in the way, which works
# with Qt 6.8 too (TS_OUTPUT_DIRECTORY, QM_OUTPUT_DIRECTORY and MERGE_QT_TRANSLATIONS exist since
# Qt 6.9; older versions read unknown keywords as values of the previous argument)
set(PDF4QT_TS_FILES)
foreach(PDF4QT_LANGUAGE IN LISTS QT_I18N_TRANSLATED_LANGUAGES)
    list(APPEND PDF4QT_TS_FILES ${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY}/PDF4QT_${PDF4QT_LANGUAGE}.ts)
endforeach()
set_source_files_properties(${PDF4QT_TS_FILES} PROPERTIES OUTPUT_LOCATION ${PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY})

set(PDF4QT_MERGE_QT_TRANSLATIONS)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.9)
    set(PDF4QT_MERGE_QT_TRANSLATIONS MERGE_QT_TRANSLATIONS)
endif()

qt_add_translations(TARGETS inkpage
                    SOURCE_TARGETS ${i18n_targets}
                    TS_FILES ${PDF4QT_TS_FILES}
                    ${PDF4QT_MERGE_QT_TRANSLATIONS}
                    LUPDATE_OPTIONS -no-obsolete
                    QM_FILES_OUTPUT_VARIABLE PDF4QT_QM_FILES)

# Keep the .ts files with CRLF line endings
set(PDF4QT_LUPDATE_TARGET "${PROJECT_NAME}_lupdate")
if(TARGET ${PDF4QT_LUPDATE_TARGET})
    add_custom_command(TARGET ${PDF4QT_LUPDATE_TARGET}
                       POST_BUILD
                       COMMAND ${CMAKE_COMMAND}
                               -DTS_OUTPUT_DIRECTORY=${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY}
                               -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeTsLineEndings.cmake
                       COMMENT "Normalizing TS files to CRLF")
endif()

endif()

message("CMAKE_PREFIX_PATH = " ${CMAKE_PREFIX_PATH})
message("CMAKE_TOOLCHAIN_FILE = " ${CMAKE_TOOLCHAIN_FILE})

configure_file(version.txt.in version.txt)

if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    install(FILES ${PDF4QT_QM_FILES}
            DESTINATION ${PDF4QT_INSTALL_TRANS_DIR})

    install(FILES Desktop/io.github.inkpage.desktop
            DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/applications)
    install(FILES Desktop/io.github.inkpage.svg
            DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/icons/hicolor/scalable/apps)
    install(FILES Desktop/128x128/io.github.inkpage.png
            DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/icons/hicolor/128x128/apps)
    install(FILES Desktop/io.github.inkpage.metainfo.xml
            DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/metainfo)
endif()

# Debian package (cpack -G DEB), see scripts/build-deb.sh
if(PDF4QT_LINUX AND NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    include(cmake/InkpagePackaging.cmake)
endif()

#macro(dump_variables)
#    message(STATUS "dump_variables------------------------------------------{")
#    get_cmake_property(_variableNames VARIABLES)
#    foreach (_variableName ${_variableNames})
#        message(STATUS "${_variableName}=${${_variableName}}")
#    endforeach()
#    message(STATUS "dump_variables------------------------------------------}")
#endmacro()
#
#dump_variables()

