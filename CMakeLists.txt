cmake_minimum_required(VERSION 3.16)
project(Meshtalk VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(MESHTALK_ASAN "Enable AddressSanitizer" OFF)
option(MESHTALK_TLS "Enable TLS encryption (OpenSSL)" OFF)
option(MESHTALK_TESTS "Build tests and simulator" ON)
option(MESHTALK_PDCURSES "Use PDCurses (Windows)" OFF)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS ON)
elseif(APPLE)
    set(PLATFORM_MACOS ON)
elseif(UNIX)
    set(PLATFORM_LINUX ON)
endif()

# Compiler warnings
add_compile_options(
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# AddressSanitizer
if(MESHTALK_ASAN)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(WARNING "AddressSanitizer is not supported for this compiler")
    endif()
endif()

# Source groups
set(NET_SOURCES
    src/net_common.c
)

if(PLATFORM_WINDOWS)
    list(APPEND NET_SOURCES src/net_win.c)
else()
    list(APPEND NET_SOURCES src/net_unix.c)
endif()

set(CORE_SOURCES
    src/meshtalk.c
    src/discovery.c
    src/peer.c
    src/message.c
    src/dedup.c
    src/room.c
    src/presence.c
    src/persistence.c
    src/security.c
)

set(UI_SOURCES
    src/ui.c
)

set(ALL_SOURCES
    ${NET_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    src/main.c
)

# Headers
set(HEADERS
    include/net.h
    include/discovery.h
    include/peer.h
    include/message.h
    include/dedup.h
    include/room.h
    include/presence.h
    include/persistence.h
    include/ui.h
    include/security.h
    include/meshtalk.h
)

# Main executable
add_executable(meshtalk ${ALL_SOURCES} ${HEADERS})
target_include_directories(meshtalk PRIVATE include)

# Platform-specific linking
if(PLATFORM_WINDOWS)
    target_link_libraries(meshtalk PRIVATE ws2_32 iphlpapi)
elseif(PLATFORM_MACOS OR PLATFORM_LINUX)
    target_link_libraries(meshtalk PRIVATE pthread)
endif()

# NCurses
if(PLATFORM_WINDOWS)
    if(MESHTALK_PDCURSES)
        # PDCurses should be available in the system or as a subdirectory
        find_package(PDCurses)
        if(PDCurses_FOUND)
            target_include_directories(meshtalk PRIVATE ${PDCurses_INCLUDE_DIRS})
            target_link_libraries(meshtalk PRIVATE ${PDCurses_LIBRARIES})
            target_compile_definitions(meshtalk PRIVATE USE_PDCURSES)
        else()
            message(WARNING "PDCurses not found, using basic terminal mode")
        endif()
    endif()
else()
    find_package(Curses)
    if(CURSES_FOUND)
        target_include_directories(meshtalk PRIVATE ${CURSES_INCLUDE_DIRS})
        target_link_libraries(meshtalk PRIVATE ${CURSES_LIBRARIES})
        target_compile_definitions(meshtalk PRIVATE USE_NCURSES)
    else()
        message(WARNING "NCurses not found, using basic terminal mode")
    endif()
endif()

# TLS (OpenSSL)
if(MESHTALK_TLS)
    find_package(OpenSSL REQUIRED)
    target_include_directories(meshtalk PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(meshtalk PRIVATE ${OPENSSL_LIBRARIES})
    target_compile_definitions(meshtalk PRIVATE USE_TLS)
endif()

# Tests
if(MESHTALK_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
