target_sources(GhostReferenceLib
PRIVATE
    image.cpp
    network_download.cpp
    window_utils.cpp
    zip_file.cpp
)

target_include_directories(GhostReferenceLib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
