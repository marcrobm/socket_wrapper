SET(BUILD_TESTS ON CACHE BOOL "Build tests")
# This is the makefile for the eznetwork library which provides a wrapper around bare c network communication
cmake_minimum_required(VERSION 3.9)
project(socket_wrapper
        VERSION 0.1.2
        DESCRIPTION "a wrapper around sockets for tcp communications")

set(CMAKE_CXX_STANDARD 14)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SOURCE_FILES
        src/Stream.cpp
        src/BufferedStream.cpp
        src/SocketException.cpp
        src/StreamFactory.cpp
        src/Listener.cpp
        src/BaseTypes.cpp
        src/ConditionalBufferedStream.cpp
        src/UdpDatagram.cpp
        src/Utils.cpp)
set(HEADER_FILES
        include/socket_wrapper/Stream.h
        include/socket_wrapper/StreamFactory.h
        include/socket_wrapper/SocketException.h
        include/socket_wrapper/Listener.h
        include/socket_wrapper/BaseTypes.h
        include/socket_wrapper/BufferedStream.h
        include/socket_wrapper/ConditionalBufferedStream.h
        include/socket_wrapper/UdpDatagram.h)

############################# openssl #############################################################
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message("found openssl")
    include_directories(${OPENSSL_INCLUDE_DIR})
    # set c flags so program knows about openssl
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENSSL_FOUND")
    add_library(socket_wrapper STATIC ${SOURCE_FILES})
    target_link_libraries(socket_wrapper ${OPENSSL_LIBRARIES})
else()
    message("openssl not found")
    add_library(socket_wrapper STATIC ${SOURCE_FILES})
endif()

install(TARGETS socket_wrapper DESTINATION /usr/lib) # the install directory can be changed to not require root
install(FILES ${HEADER_FILES} DESTINATION include)

############################## google test ########################################################
if(BUILD_TESTS)
    message("building tests")
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
else()
    message("not building tests")
endif()
