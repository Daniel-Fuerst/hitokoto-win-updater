# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(updater_core_tests
    test_s001_core.cpp
)

target_link_libraries(updater_core_tests PRIVATE
    updater_core
    GTest::gtest_main
)

updater_set_compile_options(updater_core_tests)

include(GoogleTest)
gtest_discover_tests(updater_core_tests)
