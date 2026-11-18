# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)

project(sticky-notes
    VERSION 1.0.2
    DESCRIPTION "Markdown sticky notes for the KDE Plasma desktop"
    HOMEPAGE_URL "https://github.com/clankercode/plasma-markdown-sticky-note"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(STICKY_NOTES_BUILD_TESTS "Build unit tests" ON)
option(STICKY_NOTES_BUILD_APP "Build the application (OFF = core library tests only)" ON)
option(STICKY_NOTES_WITH_LAYERSHELL "Use LayerShellQt for desktop notes on Wayland (if found)" ON)

include(GNUInstallDirs)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets Network)
qt_standard_project_setup(REQUIRES 6.6)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW) # extra qmldir per QML subdirectory (Qt >= 6.8)
endif()

set(HAVE_LAYERSHELL OFF)
if(STICKY_NOTES_WITH_LAYERSHELL)
    find_package(LayerShellQt 6 QUIET)
    if(LayerShellQt_FOUND)
        set(HAVE_LAYERSHELL ON)
        message(STATUS "LayerShellQt found: desktop notes will use wlr-layer-shell on Wayland")
    else()
        message(STATUS "LayerShellQt not found: desktop notes fall back to regular frameless windows")
    endif()
endif()

set(STICKY_NOTES_APP_ID "code.clanker.StickyNotes")

# Core (UI-independent) sources, compiled into both the app and the unit tests.
set(STICKY_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/core/frontmatter.cpp
    ${PROJECT_SOURCE_DIR}/src/core/frontmatter.h
    ${PROJECT_SOURCE_DIR}/src/core/markdownutil.cpp
    ${PROJECT_SOURCE_DIR}/src/core/markdownutil.h
    ${PROJECT_SOURCE_DIR}/src/core/note.cpp
    ${PROJECT_SOURCE_DIR}/src/core/note.h
    ${PROJECT_SOURCE_DIR}/src/core/notelistmodel.cpp
    ${PROJECT_SOURCE_DIR}/src/core/notelistmodel.h
    ${PROJECT_SOURCE_DIR}/src/core/notestore.cpp
    ${PROJECT_SOURCE_DIR}/src/core/notestore.h
    ${PROJECT_SOURCE_DIR}/src/core/placementmath.h)

if(STICKY_NOTES_BUILD_APP)
    add_subdirectory(src)
endif()

if(STICKY_NOTES_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)
    add_subdirectory(tests)
endif()

if(NOT STICKY_NOTES_BUILD_APP)
    return()
endif()

# ---- install data files -----------------------------------------------------
configure_file(data/${STICKY_NOTES_APP_ID}.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/${STICKY_NOTES_APP_ID}.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${STICKY_NOTES_APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${STICKY_NOTES_APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/icons/${STICKY_NOTES_APP_ID}.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
