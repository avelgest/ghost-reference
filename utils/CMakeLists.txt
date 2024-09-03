target_sources(GhostReference
PRIVATE
    image.cpp
    network_download.cpp
    window_utils.cpp
    zip_file.cpp
)

target_include_directories(GhostReference PUBLIC ${CMAKE_CURRENT_LIST_DIR})
