# SPDX-License-Identifier: MIT
# Each test compiles the core sources directly (no separate library needed).
function(sticky_add_test name)
    qt_add_executable(${name} ${name}.cpp ${STICKY_CORE_SOURCES})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src/core)
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test)
    target_compile_definitions(${name} PRIVATE QT_USE_QSTRINGBUILDER)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

sticky_add_test(tst_frontmatter)
sticky_add_test(tst_markdownutil)
sticky_add_test(tst_note)
sticky_add_test(tst_notestore)
sticky_add_test(tst_notelistmodel)

if(STICKY_NOTES_BUILD_APP)
    qt_add_executable(tst_application tst_application.cpp)
    target_link_libraries(tst_application PRIVATE Qt6::Core Qt6::Test)
    target_compile_definitions(tst_application PRIVATE STICKY_NOTES_BINARY="$<TARGET_FILE:sticky-notes>")
    add_dependencies(tst_application sticky-notes)
    add_test(NAME tst_application COMMAND tst_application)
    set_tests_properties(tst_application PROPERTIES TIMEOUT 30)
endif()

if(STICKY_NOTES_BUILD_APP)
    qt_add_executable(tst_ui tst_ui.cpp ${STICKY_CORE_SOURCES}
        ${PROJECT_SOURCE_DIR}/src/app/appcontroller.cpp
        ${PROJECT_SOURCE_DIR}/src/app/appcontroller.h
        ${PROJECT_SOURCE_DIR}/src/app/desktopplacement.cpp
        ${PROJECT_SOURCE_DIR}/src/app/desktopplacement.h
        ${PROJECT_SOURCE_DIR}/src/app/markdownstyler.cpp
        ${PROJECT_SOURCE_DIR}/src/app/markdownstyler.h
        ${PROJECT_SOURCE_DIR}/src/resources.qrc)
    include(${PROJECT_SOURCE_DIR}/src/qml/qmlfiles.cmake)
    list(TRANSFORM STICKY_QML_FILES PREPEND "${PROJECT_SOURCE_DIR}/src/")
    foreach(qml_file IN LISTS STICKY_QML_FILES)
        file(RELATIVE_PATH qml_alias "${PROJECT_SOURCE_DIR}/src" "${qml_file}")
        set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
    endforeach()
    set_source_files_properties(${PROJECT_SOURCE_DIR}/src/qml/Theme.qml ${PROJECT_SOURCE_DIR}/src/qml/Glyphs.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_qml_module(tst_ui URI StickyNotes VERSION 1.0 QML_FILES ${STICKY_QML_FILES}
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/StickyNotes)
    set_property(TARGET tst_ui PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    target_include_directories(tst_ui PRIVATE ${PROJECT_SOURCE_DIR}/src/core ${PROJECT_SOURCE_DIR}/src/app)
    target_link_libraries(tst_ui PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Test)
    target_compile_definitions(tst_ui PRIVATE STICKY_NOTES_VERSION="${PROJECT_VERSION}" STICKY_NOTES_APP_ID="${STICKY_NOTES_APP_ID}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tst_ui PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME tst_ui COMMAND tst_ui)
    set_tests_properties(tst_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Regression test: the INSTALLED binary must start on its own, with no build tree next to it.
# (1.0.0 shipped a binary that only worked from the build dir: "Module StickyNotes contains no type named Main".)
if(STICKY_NOTES_BUILD_APP)
    add_test(NAME tst_installed
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${PROJECT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_smoke.cmake)
    set_tests_properties(tst_installed PROPERTIES TIMEOUT 60)
endif()
