# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory(core)

if(UPDATER_BUILD_QT)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(cli)
endif()
