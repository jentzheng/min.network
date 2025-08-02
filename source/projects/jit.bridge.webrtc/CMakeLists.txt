# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.
cmake_minimum_required(VERSION 3.24)


set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################
include_directories( 
	"${C74_INCLUDES}"
)

set( SOURCE_FILES
	${PROJECT_NAME}.cpp
)

add_library( 
	${PROJECT_NAME} 
	MODULE
	${SOURCE_FILES}
)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

#############################################################
# UNIT TEST
#############################################################
# include(${C74_MIN_API_DIR}/test/min-object-unittest.cmake)

#############################################################
# Socket.IO Client + LibDataChannel + Encoder
#############################################################
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
include_directories(${FFMPEG_INCLUDE_DIRS})

add_subdirectory(
    ../../socket.io-client-cpp
    ${CMAKE_BINARY_DIR}/socket.io-client-cpp-build
)

set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(
    ../../libdatachannel
    ${CMAKE_BINARY_DIR}/libdatachannel-build
    EXCLUDE_FROM_ALL
)

# link_directories(/opt/homebrew/lib)

# Wrap socketio, libdatachanel into a customo lib
add_library(webrtc_client STATIC 
    webrtc_client.cpp
    encoder_libav.cpp
    decoder_libav.cpp
)

target_compile_features(webrtc_client PUBLIC cxx_std_17)

target_include_directories(webrtc_client PUBLIC 
    ../../socket.io-client-cpp/src
    ../../libdatachannel/include
)

target_link_libraries(webrtc_client PRIVATE 
    sioclient
    Threads::Threads
    LibDataChannel::LibDataChannel
    ${FFMPEG_LDFLAGS}
)

# Max external
target_sources(${PROJECT_NAME} PRIVATE jit.bridge.webrtc.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    webrtc_client
)

# Debug executable
add_executable(webrtc_client_debug webrtc_client_debug.cpp)
target_link_libraries(webrtc_client_debug PRIVATE 
    webrtc_client   
)