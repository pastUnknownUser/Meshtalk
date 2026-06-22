cmake_minimum_required(VERSION 3.16)
project(meshtalk VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TLS "Enable OpenSSL TLS support" OFF)
option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_E2EE "Enable end-to-end encryption via libsodium" ON)

if(ENABLE_ASAN)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    elseif(MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN)
    set(PLATFORM_SOURCES
        src/net/net_win.c
        src/tui/tui_win.c
    )
    set(PLATFORM_LIBS ws2_32)
    set(TEST_LIBS ws2_32)
else()
    set(PLATFORM_SOURCES
        src/net/net_posix.c
        src/tui/tui_posix.c
    )
    set(PLATFORM_LIBS pthread m)
    set(TEST_LIBS pthread m)
    find_package(Curses QUIET)
    if(CURSES_FOUND)
        set(PLATFORM_LIBS ${PLATFORM_LIBS} ${CURSES_LIBRARIES})
        add_definitions(-DHAVE_NCURSES)
    endif()
endif()

if(ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    set(PLATFORM_LIBS ${PLATFORM_LIBS} OpenSSL::SSL OpenSSL::Crypto)
    add_definitions(-DHAVE_TLS)
endif()

if(ENABLE_E2EE)
    find_library(SODIUM_LIBRARY sodium
        HINTS /opt/homebrew/opt/libsodium/lib /usr/local/lib /usr/lib
    )
    find_path(SODIUM_INCLUDE_DIR sodium.h
        HINTS /opt/homebrew/opt/libsodium/include /usr/local/include /usr/include
    )
    if(SODIUM_LIBRARY AND SODIUM_INCLUDE_DIR)
        set(PLATFORM_LIBS ${PLATFORM_LIBS} ${SODIUM_LIBRARY})
        include_directories(${SODIUM_INCLUDE_DIR})
        add_definitions(-DHAVE_SODIUM)
        message(STATUS "Found libsodium: ${SODIUM_LIBRARY}")
    else()
        message(WARNING "libsodium not found; E2EE disabled. Install with: brew install libsodium")
    endif()
endif()

set(SOURCES
    src/main.c
    src/util/uuid.c
    src/util/json.c
    src/message/message.c
    src/peer/peer.c
    src/discovery/discovery.c
    src/room/room.c
    src/persistence/storage.c
    src/crypto/crypto.c
    ${PLATFORM_SOURCES}
)

set(HEADERS
    src/common.h
    src/net/net.h
    src/util/uuid.h
    src/util/json.h
    src/message/message.h
    src/peer/peer.h
    src/discovery/discovery.h
    src/room/room.h
    src/persistence/storage.h
    src/crypto/crypto.h
    src/tui/tui.h
)

add_executable(meshtalk ${SOURCES} ${HEADERS})
target_include_directories(meshtalk PRIVATE src)
target_link_libraries(meshtalk ${PLATFORM_LIBS})

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
