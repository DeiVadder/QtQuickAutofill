# SPDX-License-Identifier: MIT
qt_add_library(QtQuickAutofill STATIC)

qt_add_qml_module(QtQuickAutofill
    URI QtQuickAutofill
    VERSION 1.0
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
