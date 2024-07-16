add_library(
        favutil
        STATIC
        common.cpp
        waveform.cpp
)
target_include_directories(favutil PUBLIC ${FFMPEG_INCLUDES_DIR})
target_link_libraries(favutil PUBLIC ${FFMPEG_LIBS})