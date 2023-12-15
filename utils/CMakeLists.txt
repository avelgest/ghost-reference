target_sources(GhostReference
PRIVATE
    network_download.cpp
    zip_file.cpp
)

target_include_directories(GhostReference PUBLIC ${CMAKE_CURRENT_LIST_DIR})
