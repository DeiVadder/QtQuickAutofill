# SPDX-License-Identifier: MIT
find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_autofill tst_autofill.cpp)
target_link_libraries(tst_autofill PRIVATE Qt6::Test Qt6::Quick QtQuickAutofill QtQuickAutofillplugin)

add_test(NAME tst_autofill COMMAND tst_autofill)
set_tests_properties(tst_autofill PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
