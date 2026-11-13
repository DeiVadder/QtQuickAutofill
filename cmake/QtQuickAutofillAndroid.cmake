# SPDX-License-Identifier: MIT
#
# qtquickautofill_configure_android(<target> [PACKAGE_SOURCE_DIR <dir>])
#
# The Android part of QtQuickAutofill is a Java class that has to end up in the APK. Qt only takes Java sources from
# the target's QT_ANDROID_PACKAGE_SOURCE_DIR, and a target can have only one. This function merges your own package
# source directory (manifest, res, Java, ...) with the library's Java sources into a directory in the build tree and
# points the target at it. Your source tree is not modified.
#
# Call it after qt_add_executable(). Without PACKAGE_SOURCE_DIR it uses the target's current
# QT_ANDROID_PACKAGE_SOURCE_DIR, if any. Re-run CMake after changing files in your package source directory.

set(QTQUICKAUTOFILL_ANDROID_JAVA_DIR "${CMAKE_CURRENT_LIST_DIR}/../src/android" CACHE INTERNAL "")

function(qtquickautofill_configure_android target)
    if(NOT ANDROID)
        return()
    endif()
    cmake_parse_arguments(arg "" "PACKAGE_SOURCE_DIR" "" ${ARGN})

    set(user_dir "${arg_PACKAGE_SOURCE_DIR}")
    if(NOT user_dir)
        get_target_property(user_dir ${target} QT_ANDROID_PACKAGE_SOURCE_DIR)
    endif()

    set(merged_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_qtquickautofill_android")
    file(REMOVE_RECURSE "${merged_dir}")
    file(MAKE_DIRECTORY "${merged_dir}/src")

    if(user_dir AND EXISTS "${user_dir}")
        file(COPY "${user_dir}/" DESTINATION "${merged_dir}")
        file(GLOB_RECURSE user_files "${user_dir}/*")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${user_files})
    endif()

    file(COPY "${QTQUICKAUTOFILL_ANDROID_JAVA_DIR}/" DESTINATION "${merged_dir}/src")
    file(GLOB_RECURSE lib_files "${QTQUICKAUTOFILL_ANDROID_JAVA_DIR}/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${lib_files})

    set_target_properties(${target} PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR "${merged_dir}")
endfunction()
