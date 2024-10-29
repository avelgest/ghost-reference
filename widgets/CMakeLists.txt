target_sources(GhostReferenceLib
PRIVATE
    back_window.cpp
    back_window_actions.cpp
    help_window.cpp
    main_toolbar.cpp
    picture_widget.cpp
    preferences_window.cpp
    reference_window.cpp
    resize_frame.cpp
    settings_panel.cpp
    tab_bar.cpp
)

target_include_directories(GhostReferenceLib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
