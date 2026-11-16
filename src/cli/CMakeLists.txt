# SPDX-License-Identifier: GPL-3.0-or-later
add_library(updater_cli_lib STATIC
    updater_runner.cpp
)

target_include_directories(updater_cli_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(updater_cli_lib PUBLIC
    updater_core
    updater_platform
)

updater_set_compile_options(updater_cli_lib)

add_executable(hitokoto-win-updater-cli
    main.cpp
)

target_link_libraries(hitokoto-win-updater-cli PRIVATE
    updater_cli_lib
    Qt6::Core
)

target_compile_definitions(hitokoto-win-updater-cli PRIVATE
    UPDATER_VERSION_STRING="${UPDATER_VERSION_FULL}"
)

updater_set_compile_options(hitokoto-win-updater-cli)

install(TARGETS hitokoto-win-updater-cli RUNTIME DESTINATION bin)
