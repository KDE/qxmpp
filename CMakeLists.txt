# SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
#
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.16)
project(qxmpp VERSION 1.14.4)

set(SO_VERSION 8)

# C/C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt
set(QT_VERSION_MAJOR 6)
find_package(Qt6 6.4.0 REQUIRED COMPONENTS Core Network Xml)

# OpenSSL 3.0+ (optional, for encryption features)
# OpenSSL 3.0+ uses the Apache-2.0 license which is compatible with LGPL-2.1-or-later
# only via the "or later" clause. Binaries built with this option are effectively LGPL-3.0+.
find_package(OpenSSL 3.0 QUIET)

include(GNUInstallDirs)

# deprecated options
if(DEFINED BUILD_TESTS)
    set(BUILD_TESTING ${BUILD_TESTS})
endif()

option(BUILD_SHARED "Build shared library." ON)
option(BUILD_TESTING "Build tests." ON)
option(BUILD_DOCUMENTATION "Build HTML API documentation." OFF)
option(BUILD_DOCBOOK "Build DOCBOOK documentation." OFF)
option(BUILD_EXAMPLES "Build examples." ON)
option(BUILD_OMEMO "Build the OMEMO module." OFF)
option(WITH_GSTREAMER "Build with GStreamer support for Jingle." OFF)
option(WITH_PKGCONFIG "Use PkgConfig when building." ON)
option(WITH_ENCRYPTION "Build with OpenSSL for OMEMO or encrypted file sharing. Resulting binaries are LGPL-3.0+." ${OPENSSL_FOUND})
option(ENABLE_ASAN "Build with address sanitizer." OFF)

if(WITH_PKGCONFIG)
    find_package(PkgConfig REQUIRED)
else()
    if(BUILD_OMEMO)
        message(FATAL_ERROR "BUILD_OMEMO requires WITH_PKGCONFIG=ON (needed for libomemo-c)")
    endif()

    if(WITH_GSTREAMER)
        message(FATAL_ERROR "WITH_GSTREAMER requires WITH_PKGCONFIG=ON")
    endif()
endif()

set(QXMPP_TARGET QXmppQt${QT_VERSION_MAJOR})
set(QXMPPOMEMO_TARGET QXmppOmemoQt${QT_VERSION_MAJOR})

add_definitions(
    -DQT_DISABLE_DEPRECATED_BEFORE=0x060900
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_NO_CAST_TO_ASCII
    -DQT_NO_CAST_FROM_BYTEARRAY
    -DQT_USE_QSTRINGBUILDER
    -DQT_NO_FOREACH
    -DQT_NO_KEYWORDS
)

# Address sanitizer
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

if(BUILD_OMEMO)
    add_definitions(-DBUILD_OMEMO)

    # libomemo-c
    pkg_check_modules(OmemoC REQUIRED IMPORTED_TARGET libomemo-c)

    # OpenSSL
    if(NOT WITH_ENCRYPTION)
        message(FATAL_ERROR "OMEMO requires OpenSSL (WITH_ENCRYPTION=ON)")
    endif()
endif()

if(WITH_ENCRYPTION)
    add_definitions(-DWITH_ENCRYPTION)
endif()

if(WITH_GSTREAMER)
    pkg_check_modules(GStreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.20)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_DOCUMENTATION OR BUILD_DOCBOOK)
    add_subdirectory(docs)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(CMakePackageConfigHelpers)

# Normal QXmppQt5/6 package
configure_package_config_file(
    QXmppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}Config.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${QXMPP_TARGET}"
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}Config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}ConfigVersion.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${QXMPP_TARGET}"
    COMPONENT Devel
)

# Generate QXmppQt5/6.pc
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/qxmpp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}.pc @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${QXMPP_TARGET}.pc
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT Devel
)


