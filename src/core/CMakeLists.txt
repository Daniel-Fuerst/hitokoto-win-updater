# SPDX-License-Identifier: GPL-3.0-or-later
add_library(updater_core STATIC
    version.cpp
    json.cpp
    github_release.cpp
    sha256.cpp
    install_info.cpp
    update_plan.cpp
    cli_options.cpp
)

target_include_directories(updater_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

updater_set_compile_options(updater_core)
