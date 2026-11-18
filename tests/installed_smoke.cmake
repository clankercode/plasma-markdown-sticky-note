# SPDX-License-Identifier: MIT
# Installs the build into a scratch prefix, copies nothing else, and launches the installed binary
# from an unrelated working directory. Fails if the app can't load its QML or exits non-zero.
if(NOT BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR not set")
endif()
set(scratch "${BUILD_DIR}/tst_installed")
file(REMOVE_RECURSE "${scratch}")
file(MAKE_DIRECTORY "${scratch}/notes" "${scratch}/config" "${scratch}/data" "${scratch}/cwd")

execute_process(COMMAND ${CMAKE_COMMAND} --install "${BUILD_DIR}" --prefix "${scratch}/prefix"
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "install failed: ${err}")
endif()

set(bin "${scratch}/prefix/bin/sticky-notes")
if(NOT EXISTS "${bin}")
    message(FATAL_ERROR "installed binary missing: ${bin}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        QT_QPA_PLATFORM=offscreen
        STICKY_NOTES_QUIT_AFTER_FIRST_FRAME=1
        XDG_CONFIG_HOME=${scratch}/config
        XDG_DATA_HOME=${scratch}/data
        QML_IMPORT_PATH=
        QML2_IMPORT_PATH=
        "${bin}" --notes-dir "${scratch}/notes"
    WORKING_DIRECTORY "${scratch}/cwd"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
    TIMEOUT 30)
message("${out}${err}")
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "installed binary exited with ${rc}")
endif()
if(err MATCHES "failed to load component|contains no type|is not installed|QQmlApplicationEngine failed")
    message(FATAL_ERROR "installed binary could not load its QML")
endif()
if(NOT err MATCHES "First notebook frame")
    message(FATAL_ERROR "installed binary never rendered a frame")
endif()
