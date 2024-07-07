cmake_minimum_required(VERSION 3.22.1)
project(test_ffmpeg VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

# set(CMAKE_MODULE_PATH CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/7.0.1)

find_path(FFMPEG_INCLUDE_DIR NAMES libavdevice PATHS ${FFMPEG_DIR} PATH_SUFFIXES include REQUIRED)
find_library(avcodec_LIB NAMES avcodec PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avdevice_LIB NAMES avdevice PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avfilter_LIB NAMES avfilter PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avformat_LIB NAMES avformat PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avutil_LIB NAMES avutil PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(swresample_LIB NAMES swresample PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(swscale_LIB NAMES swscale PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)

add_executable(test_ffmpeg ffmpeg_audio_decoder.cpp ffmpeg_audio_encoder.cpp main.cpp ffmpeg_audio_adapter.cpp)
target_include_directories(test_ffmpeg PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(test_ffmpeg PRIVATE ${avcodec_LIB} ${avdevice_LIB} ${avfilter_LIB} ${avformat_LIB} ${avutil_LIB} ${swresample_LIB} ${swscale_LIB})
target_compile_definitions(test_ffmpeg PRIVATE SPLEETER_ENABLE_PROGRESS_CALLBACK)

add_executable(transcode_aac transcode_aac.c)
target_include_directories(transcode_aac PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(transcode_aac PRIVATE ${avcodec_LIB} ${avdevice_LIB} ${avfilter_LIB} ${avformat_LIB} ${avutil_LIB} ${swresample_LIB} ${swscale_LIB})

message(-----${FFMPEG_INCLUDE_DIR})
message(-----${avcodec_LIB})
message(-----${avdevice_LIB})
message(-----${avfilter_LIB})
message(-----${avformat_LIB})
message(-----${avutil_LIB})
message(-----${swresample_LIB})
message(-----${swscale_LIB})