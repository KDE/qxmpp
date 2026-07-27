# SPDX-FileCopyrightText: 2017 Niels Ole Salscheider <niels_ole@salscheider-online.de>
#
# SPDX-License-Identifier: CC0-1.0

include_directories(.)

find_package(Qt6 REQUIRED COMPONENTS Test)

# optional QtGui support (for testing header-only usage)
find_package(Qt6 COMPONENTS Gui QUIET)

add_library(testutils OBJECT util.h TestClient.h TestClient.cpp TestPasswordChecker.h TestPasswordChecker.cpp)
target_link_libraries(testutils PUBLIC Qt6::Test ${QXMPP_TARGET}_internal)

macro(add_simple_test TEST_NAME)
    add_executable(tst_${TEST_NAME} tst_${TEST_NAME}.cpp $<TARGET_OBJECTS:testutils> ${ARGN})
    add_test(tst_${TEST_NAME} tst_${TEST_NAME})
    target_link_libraries(tst_${TEST_NAME} Qt6::Test ${QXMPP_TARGET}_internal)
    target_compile_definitions(tst_${TEST_NAME} PRIVATE QXMPP_STATIC_DEFINE)

    if(${TEST_NAME} MATCHES "^QXmppOmemo")
        target_link_libraries(tst_${TEST_NAME} ${QXMPPOMEMO_TARGET}_internal)
        target_compile_definitions(tst_${TEST_NAME} PRIVATE QXMPPOMEMO_STATIC_DEFINE)
    endif()
endmacro()

add_simple_test(QXmppCalls)
if(WITH_GSTREAMER)
    target_compile_definitions(tst_QXmppCalls PRIVATE WITH_GSTREAMER)
endif()
add_simple_test(QXmppClientBase)
if(Qt6Gui_FOUND)
    target_link_libraries(tst_QXmppClientBase Qt::Gui)
endif()
add_simple_test(QXmppCoreUtils resources/extended.qrc)
add_simple_test(QXmppDataCore)
add_simple_test(QXmppFileSharing resources/base.qrc)
add_simple_test(QXmppIceConnection)
add_simple_test(QXmppIq)
add_simple_test(QXmppJingleData)
add_simple_test(QXmppManagersAccounts)
add_simple_test(QXmppManagersMessages)
add_simple_test(QXmppManagersUserInfo)
add_simple_test(QXmppMessage)
add_simple_test(QXmppMiscData)
add_simple_test(QXmppMix)
add_simple_test(QXmppMixManager)
add_simple_test(QXmppMuc)
add_simple_test(QXmppNetwork)
add_simple_test(QXmppPubSub)
add_simple_test(QXmppPubSubManager)
add_simple_test(QXmppSasl)
add_simple_test(QXmppTrust)

if(BUILD_OMEMO)
    add_simple_test(QXmppOmemoData)
    add_simple_test(QXmppOmemoManager)
endif()
