
        
add_executable(test_socket_wrapper "")
target_sources(test_socket_wrapper
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/test.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
        )

target_link_libraries(test_socket_wrapper PUBLIC
        gtest_main
        socket_wrapper
        )
target_include_directories(test_socket_wrapper PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
include(GoogleTest)

#gtest_discover_tests(test_utils)
