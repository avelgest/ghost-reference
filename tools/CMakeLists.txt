target_sources(GhostReferenceLib
PRIVATE
    color_picker.cpp
    tool.cpp
)

target_include_directories(GhostReferenceLib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
