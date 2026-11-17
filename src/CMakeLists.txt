# SPDX-License-Identifier: MIT
qt_add_executable(sticky-notes
    app/main.cpp
    app/appcontroller.cpp
    app/appcontroller.h
    app/desktopplacement.cpp
    app/desktopplacement.h
    app/markdownstyler.cpp
    app/markdownstyler.h
    app/singleinstance.cpp
    app/singleinstance.h
    ${STICKY_CORE_SOURCES}
    resources.qrc
)

# QML files are listed in qml/qmlfiles.cmake (sets STICKY_QML_FILES, paths relative to src/).
include(${CMAKE_CURRENT_SOURCE_DIR}/qml/qmlfiles.cmake)

qt_add_qml_module(sticky-notes
    URI StickyNotes
    VERSION 1.0
    QML_FILES ${STICKY_QML_FILES}
    NO_RESOURCE_TARGET_PATH
)

# Import scanning is needed to link static QML plugins. Shared Qt discovers its
# plugins at runtime; distribution builds can omit their CMake targets entirely.
get_target_property(sticky_qt_core_type Qt6::Core TYPE)
if(sticky_qt_core_type STREQUAL "SHARED_LIBRARY")
    set_property(TARGET sticky-notes PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
endif()

target_include_directories(sticky-notes PRIVATE core app)

target_compile_definitions(sticky-notes PRIVATE
    STICKY_NOTES_VERSION="${PROJECT_VERSION}"
    STICKY_NOTES_APP_ID="${STICKY_NOTES_APP_ID}"
    QT_NO_URL_CAST_FROM_STRING
    QT_USE_QSTRINGBUILDER)

target_link_libraries(sticky-notes PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network)

if(HAVE_LAYERSHELL)
    target_link_libraries(sticky-notes PRIVATE LayerShellQt::Interface)
    target_compile_definitions(sticky-notes PRIVATE HAVE_LAYERSHELL=1)

    # Qt 6.9+ exposes the Wayland display through its public native interface.
    # Probe rather than require it: older Qt / non-Wayland builds remain supported.
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WaylandClient QUIET IMPORTED_TARGET wayland-client)
        if(WaylandClient_FOUND)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_LIBRARIES Qt6::Gui)
            check_cxx_source_compiles("#include <QtGui/qguiapplication_platform.h>
                int main() { QNativeInterface::QWaylandApplication *p = nullptr; (void)p; }"
                STICKY_HAS_WAYLAND_NATIVE_INTERFACE)
            unset(CMAKE_REQUIRED_LIBRARIES)
            if(STICKY_HAS_WAYLAND_NATIVE_INTERFACE)
                target_link_libraries(sticky-notes PRIVATE PkgConfig::WaylandClient)
                target_compile_definitions(sticky-notes PRIVATE HAVE_WAYLAND_REGISTRY=1)
            endif()
        endif()
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sticky-notes PRIVATE -Wall -Wextra)
endif()

install(TARGETS sticky-notes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
