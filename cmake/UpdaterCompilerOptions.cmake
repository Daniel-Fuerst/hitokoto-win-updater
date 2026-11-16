# SPDX-License-Identifier: GPL-3.0-or-later
# Warnings and sanitizers, applied per target through updater_set_compile_options().

function(updater_set_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        if(UPDATER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
        if(UPDATER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(UPDATER_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "UPDATER_SANITIZE is supported with GCC and Clang only")
    endif()
    add_compile_options(-fsanitize=${UPDATER_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${UPDATER_SANITIZE})
endif()
