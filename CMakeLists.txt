cmake_minimum_required(VERSION 3.22.1)
project(ffmpeg_invoker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

# set(CMAKE_MODULE_PATH CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(FFMPEG_DIR ${FFMPEG_DIR})

find_path(FFMPEG_INCLUDE_DIR NAMES libavdevice PATHS ${FFMPEG_DIR} PATH_SUFFIXES include REQUIRED)
find_library(avcodec_LIB NAMES avcodec PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avdevice_LIB NAMES avdevice PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avfilter_LIB NAMES avfilter PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avformat_LIB NAMES avformat PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(avutil_LIB NAMES avutil PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(swresample_LIB NAMES swresample PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)
find_library(swscale_LIB NAMES swscale PATHS ${FFMPEG_DIR} PATH_SUFFIXES lib REQUIRED)

set(FFMPEG_INCLUDES_DIR ${FFMPEG_INCLUDE_DIR})
set(FFMPEG_LIBS ${avcodec_LIB} ${avdevice_LIB} ${avfilter_LIB} ${avformat_LIB} ${avutil_LIB} ${swresample_LIB} ${swscale_LIB})


add_executable(ffmpeg_codec ffmpeg_audio_decoder.cpp ffmpeg_audio_encoder.cpp main.cpp ffmpeg_audio_codec.cpp common.cpp)
target_include_directories(ffmpeg_codec PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(ffmpeg_codec PRIVATE ${FFMPEG_LIBS})
target_compile_definitions(ffmpeg_codec PRIVATE SPLEETER_ENABLE_PROGRESS_CALLBACK)

add_executable(transcode_aac transcode_aac.c)
target_include_directories(transcode_aac PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(transcode_aac PRIVATE ${FFMPEG_LIBS})

add_subdirectory(favutil)
add_executable(test_favutil test_favutil.cpp)
target_link_libraries(test_favutil PRIVATE favutil)

add_executable(decode_filter_mix_audio decode_filter_mix_audio.c)
target_include_directories(decode_filter_mix_audio PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(decode_filter_mix_audio PRIVATE ${FFMPEG_LIBS})

message(-----${FFMPEG_INCLUDE_DIR})
message(-----${FFMPEG_LIBS})