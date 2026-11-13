# SPDX-License-Identifier: MIT
qt_add_library(QtQuickAutofill STATIC)

# The module directory has to end in the URI, otherwise qmllint, qmlls and qmlcachegen do not find it
if(QT_QML_OUTPUT_DIRECTORY)
    set(qtquickautofill_qml_dir "${QT_QML_OUTPUT_DIRECTORY}/QtQuickAutofill")
else()
    set(qtquickautofill_qml_dir "${CMAKE_BINARY_DIR}/QtQuickAutofill")
endif()

qt_add_qml_module(QtQuickAutofill
    URI QtQuickAutofill
    VERSION 1.0
    OUTPUT_DIRECTORY "${qtquickautofill_qml_dir}"
    SOURCES
        autofill.h autofill.cpp
        autofillbackend.h autofillbackend.cpp
)

target_include_directories(QtQuickAutofill PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QtQuickAutofill PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

if(ANDROID)
    target_sources(QtQuickAutofill PRIVATE autofillbackend_android.cpp)
elseif(IOS)
    enable_language(OBJCXX)
    target_sources(QtQuickAutofill PRIVATE autofillbackend_ios.mm)
    target_link_libraries(QtQuickAutofill PUBLIC "-framework UIKit")
endif()
