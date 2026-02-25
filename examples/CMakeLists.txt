# SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
#
# SPDX-License-Identifier: CC0-1.0

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)

macro(add_simple_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} example_${EXAMPLE_NAME}/example_${EXAMPLE_NAME}.cpp)
    target_link_libraries(${EXAMPLE_NAME} ${QXMPP_TARGET})
endmacro()

add_simple_example(0_connected)
add_simple_example(1_echoClient)
add_simple_example(2_rosterHandling)
add_simple_example(3_transferHandling)
add_simple_example(7_archiveHandling)
add_simple_example(8_server)

# VCard
add_simple_example(9_vCard)
target_link_libraries(9_vCard Qt${QT_VERSION_MAJOR}::Gui)

# MUC Bot
add_executable(muc_bot muc_bot/muc_bot.cpp)
target_link_libraries(muc_bot ${QXMPP_TARGET})

# AV Calls
if(WITH_GSTREAMER)
    add_executable(audio_call audio_call/AudioCall.cpp)
    target_link_libraries(audio_call PUBLIC ${QXMPP_TARGET} PkgConfig::GStreamer)
endif()
