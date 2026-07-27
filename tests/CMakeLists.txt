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
    add_executable(tst_${TEST_NAME} ${TEST_NAME}.cpp $<TARGET_OBJECTS:testutils> ${ARGN})
    add_test(tst_${TEST_NAME} tst_${TEST_NAME})
    target_link_libraries(tst_${TEST_NAME} Qt6::Test ${QXMPP_TARGET}_internal)
    target_compile_definitions(tst_${TEST_NAME} PRIVATE QXMPP_STATIC_DEFINE)

    if(${TEST_NAME} MATCHES "^Omemo")
        target_link_libraries(tst_${TEST_NAME} ${QXMPPOMEMO_TARGET}_internal)
        target_compile_definitions(tst_${TEST_NAME} PRIVATE QXMPPOMEMO_STATIC_DEFINE)
    endif()
endmacro()

add_simple_test(Calls)
if(WITH_GSTREAMER)
    target_compile_definitions(tst_Calls PRIVATE WITH_GSTREAMER)
endif()
add_simple_test(ClientBase)
if(Qt6Gui_FOUND)
    target_link_libraries(tst_ClientBase Qt::Gui)
endif()
add_simple_test(CoreUtils resources/extended.qrc)
add_simple_test(DataCore)
add_simple_test(DataJingle)
add_simple_test(DataMisc)
add_simple_test(FileSharing resources/base.qrc)
add_simple_test(IceConnection)
add_simple_test(Iq)
add_simple_test(ManagersAccounts)
add_simple_test(ManagersMessages)
add_simple_test(ManagersUserInfo)
add_simple_test(Message)
add_simple_test(Mix)
add_simple_test(MixManager)
add_simple_test(Muc)
add_simple_test(Network)
add_simple_test(PubSub)
add_simple_test(PubSubManager)
add_simple_test(Sasl)
add_simple_test(Trust)

if(BUILD_OMEMO)
    add_simple_test(OmemoData)
    add_simple_test(OmemoManager)
endif()
