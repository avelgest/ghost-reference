cmake_minimum_required(VERSION 3.28)

project(ghost-reference 
        VERSION 0.5.0.0
        LANGUAGES CXX)

option(BUILD_TESTS "Build tests [ON/OFF].")
option(QT_ROOT_DIR "The directory that Qt is installed to (contains bin, include, lib etc).")
option(SHOW_WINDOWS_CONSOLE "Show a console on Windows [ON/OFF].")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)


if (QT_ROOT_DIR)
    message(STATUS "Adding ${QT_ROOT_DIR} to CMAKE_PREFIX_PATH")
    list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT_DIR}")
endif()

set(REQUIRED_QT_COMPONENTS Core Gui Network Widgets)

find_package(Qt6 REQUIRED COMPONENTS ${REQUIRED_QT_COMPONENTS})


# minizip-ng
FetchContent_Declare(
	minizip-ng
	GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
	GIT_SHALLOW true
    EXCLUDE_FROM_ALL
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

# QHotkey
# The current version adds sets cpack params that causes configuring to fail.
# So use an older version for now. 
FetchContent_Declare(
    qhotkey
    GIT_REPOSITORY https://github.com/Skycoder42/QHotkey.git
    GIT_TAG 94efd5f
    EXCLUDE_FROM_ALL
)

set(QHOTKEY_EXAMPLES OFF)
set(QHOTKEY_INSTALL ON)

FetchContent_MakeAvailable(qhotkey)

include_directories(${qhotkey_SOURCE_DIR})
link_directories(${qhotkey_BINARY_DIR})

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG b514bdc
    EXCLUDE_FROM_ALL
)

set(CMAKE_AUTORCC ON)

add_compile_definitions(
    GHOST_REF_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
    GHOST_REF_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
    GHOST_REF_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
)

qt_standard_project_setup()

qt_add_library(GhostReferenceLib STATIC
                app.cpp
                global_hotkeys.cpp
                preferences.cpp
                reference_collection.cpp
                reference_image.cpp
                reference_loading.cpp
                saving.cpp
                system_tray_icon.cpp
                undo_stack.cpp
)

qt_add_executable(GhostReference
                  resources/resources.qrc
                  ghost_reference.rc
                  main.cpp
                  )

add_subdirectory(tools)
add_subdirectory(utils)
add_subdirectory(widgets)

target_link_libraries(GhostReferenceLib PRIVATE Qt6::Core
                                        PRIVATE Qt6::Gui
                                        PRIVATE Qt6::Widgets
                                        PRIVATE Qt6::Network
                                        PRIVATE MINIZIP::minizip
                                        PRIVATE qhotkey
)

target_link_libraries(GhostReference PRIVATE GhostReferenceLib)

if (NOT SHOW_WINDOWS_CONSOLE)
    set_target_properties(GhostReference PROPERTIES
                          WIN32_EXECUTABLE ON
    )
endif()

if (BUILD_TESTS)
    qt_add_executable(tests tests/tests_main.cpp)

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
    target_link_libraries(tests PRIVATE Qt6::Core
                                PRIVATE Qt6::Gui
                                PRIVATE Qt6::Widgets
                                PRIVATE GTest::gtest
                                PRIVATE GhostReferenceLib
    )
    include(GoogleTest)
    gtest_discover_tests(tests 
                         DISCOVERY_MODE PRE_TEST)
endif()

install(TARGETS GhostReference)
qt_generate_deploy_app_script(
    TARGET GhostReference
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# CPack
set(CPACK_PACKAGE_NAME "Ghost Reference")
set(CPACK_PACKAGE_VENDOR "Avelgest")
set(CPACK_MONOLITHIC_INSTALL "TRUE")
set(CPACK_PACKAGE_DESCRIPTION "Reference image overlay application with click-through mode.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reference image overlay")
set(CPACK_PACKAGE_EXECUTABLES "GhostReference" "Ghost Reference")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")

set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon.ico")
set(CPACK_WIX_PROGRAM_MENU_FOLDER ".")
set(CPACK_WIX_UPGRADE_GUID "EC7D995A-4EBD-4D6D-B66F-B739566CD1D4")
set(CPACK_WIX_VERSION 4)

# Needed when CPACK_MONOLITHIC_INSTALL is false. See https://gitlab.kitware.com/cmake/cmake/-/issues/20177
# set(CPACK_COMPONENTS_ALL Unspecified)

include(CPACK)