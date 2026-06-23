# Include net source for tests that need it
if(WIN32)
    set(NET_SRC ${CMAKE_SOURCE_DIR}/src/net/net_win.c)
else()
    set(NET_SRC ${CMAKE_SOURCE_DIR}/src/net/net_posix.c)
endif()

set(CORE_SRC
    ${NET_SRC}
    ${CMAKE_SOURCE_DIR}/src/util/json.c
    ${CMAKE_SOURCE_DIR}/src/util/uuid.c
    ${CMAKE_SOURCE_DIR}/src/message/message.c
    ${CMAKE_SOURCE_DIR}/src/peer/peer.c
)

add_executable(test_net test_net.c ${NET_SRC})
target_link_libraries(test_net ${TEST_LIBS})
target_include_directories(test_net PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_net COMMAND test_net)

add_executable(test_message test_message.c ${CORE_SRC})
target_link_libraries(test_message ${TEST_LIBS})
target_include_directories(test_message PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_message COMMAND test_message)

add_executable(test_discovery test_discovery.c ${CORE_SRC} ${CMAKE_SOURCE_DIR}/src/discovery/discovery.c)
target_link_libraries(test_discovery ${TEST_LIBS})
target_include_directories(test_discovery PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_discovery COMMAND test_discovery)

add_executable(test_peer test_peer.c ${NET_SRC} ${CMAKE_SOURCE_DIR}/src/peer/peer.c)
target_link_libraries(test_peer ${TEST_LIBS})
target_include_directories(test_peer PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_peer COMMAND test_peer)

add_executable(test_simulator test_simulator.c ${CORE_SRC} ${CMAKE_SOURCE_DIR}/src/discovery/discovery.c ${CMAKE_SOURCE_DIR}/src/room/room.c ${CMAKE_SOURCE_DIR}/src/persistence/storage.c ${CMAKE_SOURCE_DIR}/src/crypto/crypto.c)
target_link_libraries(test_simulator ${TEST_LIBS} ${SODIUM_LIBRARY})
target_include_directories(test_simulator PRIVATE ${CMAKE_SOURCE_DIR}/src ${SODIUM_INCLUDE_DIR})
add_test(NAME test_simulator COMMAND test_simulator)
