cmake_minimum_required(VERSION 3.16)

project(oop-project-Joyce VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindowdxx.cpp
        mainwindowdxx.h
        mainwindowdxx.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(oop-project-Joyce
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET oop-project-Joyce APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(oop-project-Joyce SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(oop-project-Joyce
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(oop-project-Joyce PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.oop-project-Joyce)
endif()
set_target_properties(oop-project-Joyce PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS oop-project-Joyce
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(oop-project-Joyce)
endif()

# MVP 1: the original template target and its sources are retained above.
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network)
list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/json-c /usr/local/opt/json-c
    /opt/homebrew/opt/openssl@3 /usr/local/opt/openssl@3)
find_package(json-c CONFIG REQUIRED)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/database/schema.sql" MVP1_SCHEMA)
configure_file(database/SchemaDXX.h.in generated/SchemaDXX.h @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/database/migrations/002_friends.sql" MVP2_MIGRATION)
configure_file(database/MigrationDXX.h.in generated/MigrationDXX.h @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/database/migrations/003_groups.sql" MVP3_MIGRATION)
configure_file(database/MigrationV3DXX.h.in generated/MigrationV3DXX.h @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/database/migrations/004_chat.sql" MVP4_MIGRATION)
configure_file(database/MigrationV4DXX.h.in generated/MigrationV4DXX.h @ONLY)
add_library(IMProtocol STATIC protocol/JsonDXX.cpp protocol/ProtocolDXX.cpp)
set_target_properties(IMProtocol PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(IMProtocol PUBLIC protocol)
target_link_libraries(IMProtocol PUBLIC json-c::json-c)
add_library(IMGroupDomain STATIC
    core/groups/GroupContextDXX.cpp core/groups/GroupPolicyDXX.cpp
    core/groups/QQGroupPolicyDXX.cpp core/groups/WeChatGroupPolicyDXX.cpp
    core/groups/GroupPolicyFactoryDXX.cpp core/groups/GroupMemberDXX.cpp
    core/groups/SubgroupDXX.cpp core/groups/GroupDXX.cpp)
set_target_properties(IMGroupDomain PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(IMGroupDomain PUBLIC core/groups)
add_library(IMServerCore STATIC
    server/persistence/DatabaseDXX.cpp server/persistence/StatementDXX.cpp
    server/application/PasswordDXX.cpp server/application/AuthServiceDXX.cpp
    server/application/BusinessErrorDXX.cpp server/application/SocialRepositoryDXX.cpp
    server/application/AccountServiceDXX.cpp server/persistence/MutationRepositoryDXX.cpp
    server/application/FriendServiceDXX.cpp server/application/RecommendationServiceDXX.cpp
    server/application/GroupServiceDXX.cpp server/persistence/GroupRepositoryDXX.cpp
    server/application/MessageServiceDXX.cpp server/persistence/MessageRepositoryDXX.cpp
    server/application/RequestDispatcherDXX.cpp server/network/TcpServerDXX.cpp)
set_target_properties(IMServerCore PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(IMServerCore PUBLIC server/persistence server/application server/network
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(IMServerCore PUBLIC IMProtocol IMGroupDomain SQLite::SQLite3 OpenSSL::Crypto Threads::Threads)
add_executable(IMServer server/main.cpp)
set_target_properties(IMServer PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_link_libraries(IMServer PRIVATE IMServerCore)
add_library(IMClientUi STATIC
    client/network/TcpClientDXX.cpp client/ui/LoginWindowDXX.cpp
    client/ui/RegisterWindowDXX.cpp client/ui/ServiceSelectWindowDXX.cpp
    client/ui/RequestPanelDXX.cpp client/ui/FriendPageDXX.cpp
    client/ui/FriendRequestPageDXX.cpp client/ui/RecommendationPageDXX.cpp
    client/ui/AccountPageDXX.cpp client/ui/WorkspaceDXX.cpp
    client/ui/GroupPageDXX.cpp client/ui/GroupPolicyDialogDXX.cpp client/ui/SubgroupPageDXX.cpp
    client/ui/ChatPageDXX.cpp client/ui/MessageModelDXX.cpp)
target_include_directories(IMClientUi PUBLIC client/network client/ui)
target_link_libraries(IMClientUi PUBLIC IMProtocol Qt6::Widgets Qt6::Network)
qt_add_executable(IMClient client/main.cpp)
target_link_libraries(IMClient PRIVATE IMClientUi)
set_target_properties(IMClient PROPERTIES MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER edu.joyce.implatform.mvp1
    MACOSX_BUNDLE_BUNDLE_NAME IMClient)
foreach(target IMProtocol IMServerCore IMServer IMClientUi IMClient)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(ProtocolTestsDXX tests/protocol_main.cpp)
    target_link_libraries(ProtocolTestsDXX PRIVATE IMProtocol)
    add_test(NAME protocol COMMAND ProtocolTestsDXX)
    add_test(NAME acceptance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance.py" --server $<TARGET_FILE:IMServer>)
    set_tests_properties(acceptance PROPERTIES TIMEOUT 90)
    add_executable(ClientUiTestsDXX tests/client_ui_main.cpp)
    target_link_libraries(ClientUiTestsDXX PRIVATE IMClientUi Qt6::Test)
    add_test(NAME client_ui COMMAND ClientUiTestsDXX $<TARGET_FILE:IMServer> $<TARGET_FILE:IMClient>)
    set_tests_properties(client_ui PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING)
    add_test(NAME mvp2_acceptance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mvp2_acceptance.py" --server $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp2_acceptance PROPERTIES TIMEOUT 240)
endif()

if(BUILD_TESTING)
    add_executable(Mvp2UiTestsDXX tests/mvp2_ui_main.cpp)
    target_link_libraries(Mvp2UiTestsDXX PRIVATE IMClientUi Qt6::Test)
    add_test(NAME mvp2_ui COMMAND Mvp2UiTestsDXX $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp2_ui PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING)
    add_executable(GroupPolicyTestsDXX tests/group_policy_main.cpp)
    target_include_directories(GroupPolicyTestsDXX PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(GroupPolicyTestsDXX PRIVATE IMGroupDomain)
    add_test(NAME group_policy COMMAND GroupPolicyTestsDXX)
    add_test(NAME group_database COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/group_database.py")
    add_test(NAME mvp3_acceptance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mvp3_acceptance.py" --server $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp3_acceptance PROPERTIES TIMEOUT 360)
endif()

if(BUILD_TESTING)
    add_executable(Mvp3UiTestsDXX tests/mvp3_ui_main.cpp)
    target_link_libraries(Mvp3UiTestsDXX PRIVATE IMClientUi Qt6::Test)
    add_test(NAME mvp3_ui COMMAND Mvp3UiTestsDXX $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp3_ui PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING)
    add_test(NAME mvp4_database COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mvp4_database.py")
    add_test(NAME mvp4_acceptance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mvp4_acceptance.py" --server $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp4_acceptance PROPERTIES TIMEOUT 420)
endif()

if(BUILD_TESTING)
    add_executable(Mvp4UiTestsDXX tests/mvp4_ui_main.cpp)
    target_link_libraries(Mvp4UiTestsDXX PRIVATE IMClientUi Qt6::Test)
    add_test(NAME mvp4_ui COMMAND Mvp4UiTestsDXX $<TARGET_FILE:IMServer> $<TARGET_FILE:IMClient>)
    set_tests_properties(mvp4_ui PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING)
    add_test(NAME mvp4_receipts COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/mvp4_receipts.py" --server $<TARGET_FILE:IMServer>)
    set_tests_properties(mvp4_receipts PROPERTIES TIMEOUT 90)
endif()
