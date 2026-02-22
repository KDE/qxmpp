# SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
#
# SPDX-License-Identifier: CC0-1.0

find_package(Doxygen REQUIRED)

if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
endif()

if(BUILD_DOCUMENTATION)
    set(HTML "YES")
else()
    set(HTML "NO")
endif()

if(BUILD_DOCBOOK)
    set(DOCBOOK "YES")
else()
    set(DOCBOOK "NO")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    @ONLY
)

if(BUILD_DOCUMENTATION OR BUILD_DOCBOOK)
    add_custom_target(
        doc ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generate API documentation with Doxygen"
        VERBATIM
    )
endif()

if(BUILD_DOCUMENTATION)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

if(BUILD_DOCBOOK)
   install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docbook DESTINATION ${CMAKE_INSTALL_DATADIR}/help/en/qxmpp)
endif()
