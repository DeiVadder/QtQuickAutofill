# SPDX-License-Identifier: MIT
find_package(Qt6 REQUIRED COMPONENTS QuickControls2)

qt_add_executable(autofill-example main.cpp)

qt_add_qml_module(autofill-example
    URI AutofillExample
    VERSION 1.0
    QML_FILES Main.qml
)

target_link_libraries(autofill-example PRIVATE Qt6::Quick Qt6::QuickControls2 QtQuickAutofillplugin)

if(ANDROID)
    # Merges the library's Java class into the APK (and your own package source dir, if you have one)
    qtquickautofill_configure_android(autofill-example)
endif()

if(IOS)
    set_target_properties(autofill-example PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.deivadder.qtquickautofill.example"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.github.deivadder.qtquickautofill.example")
endif()
