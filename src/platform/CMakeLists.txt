# SPDX-License-Identifier: GPL-3.0-or-later
if(WIN32)
    set(PLATFORM_SOURCES
        github_client.cpp
        installed_locator_win.cpp
        msi_installer_win.cpp
    )
else()
    set(PLATFORM_SOURCES
        github_client.cpp
        installed_locator_stub.cpp
        msi_installer_stub.cpp
    )
endif()

add_library(updater_platform STATIC
    ${PLATFORM_SOURCES}
)

target_include_directories(updater_platform PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(updater_platform PUBLIC
    updater_core
    Qt6::Core
    Qt6::Network
)

if(WIN32)
    target_link_libraries(updater_platform PRIVATE
        advapi32
        shell32
    )
endif()

updater_set_compile_options(updater_platform)
