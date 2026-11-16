# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(tst_s001_cli
    tst_s001_cli.cpp
)

target_link_libraries(tst_s001_cli PRIVATE
    updater_cli_lib
    Qt6::Test
)

updater_set_compile_options(tst_s001_cli)

add_test(NAME S001_CliRunner COMMAND tst_s001_cli)
