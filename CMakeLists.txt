cmake_minimum_required(VERSION 3.28)

project(ghost-reference VERSION 0.1)

option(QT_ROOT_DIR "The directory that Qt is installed to.")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)


if (QT_ROOT_DIR)
    message(STATUS "Adding ${QT_ROOT_DIR} to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT_DIR}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Widgets)


# minizip-ng
FetchContent_Declare(
	minizip-ng
	GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
	GIT_SHALLOW true
)

set(MZ_ZLIB OFF)
set(MZ_BZIP2 OFF)
set(MZ_LZMA OFF)
set(MZ_ZSTD OFF)
set(MZ_LIBCOMP OFF)
set(MZ_PKCRYPT OFF)
set(MZ_WZAES OFF)
set(MZ_OPENSSL OFF)
set(MZ_LIBBSD OFF)
set(MZ_ICONV OFF)

FetchContent_MakeAvailable(minizip-ng)

include_directories(${minizip-ng_SOURCE_DIR})
link_directories(${minizip-ng_BINARY_DIR})

qt_standard_project_setup()

qt_add_executable(GhostReference 
                  main.cpp
                  app.cpp
                  preferences.cpp
                  reference_collection.cpp
                  reference_image.cpp
                  reference_loading.cpp
                  saving.cpp
                  )

add_subdirectory(utils)
add_subdirectory(widgets)

target_link_libraries(GhostReference PRIVATE Qt6::Widgets
                                     PRIVATE Qt6::Network
                                     PRIVATE MINIZIP::minizip)

