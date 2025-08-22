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
# LibDataChannel + Encoder + Decoder
#############################################################

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
	 libavdevice libavcodec libavformat libavutil libswscale)

add_library(FFmpeg INTERFACE IMPORTED GLOBAL)
target_link_libraries(FFmpeg INTERFACE
    PkgConfig::LIBAV
)

#TODO maybe use conan or vcpkg to install libdatachannel
set(CMAKE_PREFIX_PATH "../../libdatachannel/build-install")
find_package(LibDataChannel CONFIG REQUIRED)


add_library(webrtc_client STATIC
   ./webrtc_client.cpp
   ./videoencoder_libav.cpp
   ./videodecoder_libav.cpp
)

target_compile_features(webrtc_client PUBLIC cxx_std_17)
target_link_libraries(webrtc_client PUBLIC 
    FFmpeg
    Threads::Threads
    nlohmann_json::nlohmann_json
    LibDataChannel::LibDataChannel
)

# Max external
target_link_libraries(${PROJECT_NAME} PRIVATE
    webrtc_client
)

# Debug executable
add_executable(webrtc_client_debug 
    webrtc_client_debug.cpp
)

target_link_libraries(webrtc_client_debug PRIVATE 
    webrtc_client
)
