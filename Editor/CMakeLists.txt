add_library(Editor STATIC)

target_sources(Editor PRIVATE
    Camera/EditorCamera.cpp
    Camera/EditorCamera.hpp
    Core/EditorApp.cpp
    Core/EditorApp.hpp
    Core/EditorAppHost.cpp
    Core/EditorAppHost.hpp
    Core/EditorContext.hpp
    Core/EditorTheme.cpp
    Core/EditorTheme.hpp
    Core/IconManager.cpp
    Core/IconManager.hpp
    Core/ShortcutRegistry.cpp
    Core/ShortcutRegistry.hpp
    Panels/ComponentDrawer.hpp
    Panels/ContentBrowserPanel.cpp
    Panels/ContentBrowserPanel.hpp
    Panels/HelpShortcutsPanel.cpp
    Panels/HelpShortcutsPanel.hpp
    Panels/HierarchyPanel.cpp
    Panels/HierarchyPanel.hpp
    Panels/InspectorPanel.cpp
    Panels/InspectorPanel.hpp
    Panels/LogPanel.cpp
    Panels/LogPanel.hpp
    Panels/RendererSettingsPanel.cpp
    Panels/RendererSettingsPanel.hpp
    Panels/SceneViewPanel.cpp
    Panels/SceneViewPanel.hpp
)

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(Editor PUBLIC
    Engine
    Platform
    imgui
)