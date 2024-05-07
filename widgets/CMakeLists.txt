target_sources(GhostReference
PRIVATE
    back_window.cpp
    main_toolbar_actions.cpp
    main_toolbar.cpp
    picture_widget.cpp
    preferences_window.cpp
    reference_titlebar.cpp
    reference_window.cpp
    resize_frame.cpp
    settings_panel.cpp
)

target_include_directories(GhostReference PUBLIC ${CMAKE_CURRENT_LIST_DIR})
