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
add_library(IMProtocol STATIC protocol/JsonDXX.cpp protocol/ProtocolDXX.cpp)
set_target_properties(IMProtocol PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(IMProtocol PUBLIC protocol)
target_link_libraries(IMProtocol PUBLIC json-c::json-c)
add_library(IMServerCore STATIC
    server/persistence/DatabaseDXX.cpp server/persistence/StatementDXX.cpp
    server/application/PasswordDXX.cpp server/application/AuthServiceDXX.cpp
    server/application/RequestDispatcherDXX.cpp server/network/TcpServerDXX.cpp)
set_target_properties(IMServerCore PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(IMServerCore PUBLIC server/persistence server/application server/network
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(IMServerCore PUBLIC IMProtocol SQLite::SQLite3 OpenSSL::Crypto Threads::Threads)
add_executable(IMServer server/main.cpp)
set_target_properties(IMServer PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_link_libraries(IMServer PRIVATE IMServerCore)
add_library(IMClientUi STATIC
    client/network/TcpClientDXX.cpp client/ui/LoginWindowDXX.cpp
    client/ui/RegisterWindowDXX.cpp client/ui/ServiceSelectWindowDXX.cpp)
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
