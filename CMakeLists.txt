option(BUILD_TESTS ON)
# This is the makefile for the eznetwork library which provides a wrapper around bare c network communication
cmake_minimum_required(VERSION 3.9)
project(socket_wrapper
        VERSION 0.1.2
        DESCRIPTION "a wrapper around sockets for tcp communications")

set(CMAKE_CXX_STANDARD 14)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(socket_wrapper STATIC
        src/Stream.cpp
        src/BufferedStream.cpp
        src/SocketException.cpp
        src/StreamFactory.cpp
        src/ListenerBase.cpp
        src/BaseTypes.cpp
        src/ConditionalBufferedStream.cpp
        src/UdpDatagram.cpp
        src/Utils.cpp)
# the install directory can be changed to not require root privileges
install(TARGETS socket_wrapper DESTINATION /usr/lib)
install(FILES
        include/socket_wrapper/Stream.h
        include/socket_wrapper/StreamFactory.h
        include/socket_wrapper/SocketException.h
        include/socket_wrapper/Listener.h
        include/socket_wrapper/BaseTypes.h
        include/socket_wrapper/BufferedStream.h
        include/socket_wrapper/ConditionalBufferedStream.h
        include/socket_wrapper/UdpDatagram.h
        DESTINATION include)

############################## google test ########################################################
if(BUILD_TESTS)
include(FetchContent)
FetchContent_Declare(googletest
        GIT_REPOSITORY "https://github.com/google/googletest"
        GIT_PROGRESS TRUE
        GIT_SHALLOW TRUE
        GIT_TAG release-1.8.0
        DOWNLOAD_EXTRACT_TIMESTAMP true)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_subdirectory(test)
endif()