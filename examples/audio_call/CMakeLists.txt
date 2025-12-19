# SPDX-FileCopyrightText: 2025 Linus Jahn <lnj@kaidan.im>
#
# SPDX-License-Identifier: CC0-1.0

add_executable(audio_call AudioCall.cpp)

target_link_libraries(audio_call
    PUBLIC
    ${QXMPP_TARGET}
    PkgConfig::GStreamer
)
