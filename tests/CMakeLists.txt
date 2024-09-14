target_sources(tests 
PRIVATE
    tests_main.cpp
)

target_include_directories(tests PUBLIC ${CMAKE_CURRENT_LIST_DIR})