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

add_executable(inkpage
    main.cpp
    app.qrc
)

target_link_libraries(inkpage PRIVATE Pdf4QtLibCore Pdf4QtLibWidgets Pdf4QtLibGui Qt6::Core Qt6::Gui Qt6::Widgets)

# The version of the application (git tag), refreshed at every build: only main.cpp
# depends on the header, a new commit does not rebuild the libraries
set(INKPAGE_VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/inkpage_version.h)
add_custom_target(inkpage_version
                  COMMAND ${CMAKE_COMMAND} -DINKPAGE_SOURCE_DIR=${PROJECT_SOURCE_DIR}
                                           -DINKPAGE_BASE_VERSION=${INKPAGE_BASE_VERSION}
                                           -DINKPAGE_VERSION_HEADER=${INKPAGE_VERSION_HEADER}
                                           -P ${PROJECT_SOURCE_DIR}/cmake/InkpageVersion.cmake
                  BYPRODUCTS ${INKPAGE_VERSION_HEADER}
                  COMMENT "Version of inkpage"
                  VERBATIM)
add_dependencies(inkpage inkpage_version)
target_include_directories(inkpage PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

set_target_properties(inkpage PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)

install(TARGETS inkpage RUNTIME DESTINATION ${PDF4QT_INSTALL_BIN_DIR} LIBRARY DESTINATION ${PDF4QT_INSTALL_LIB_DIR})
