# Mod3D GUI Subdirectory CMakeLists.txt

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets OpenGLWidgets REQUIRED)
find_package(OpenGL REQUIRED)

set(GUI_HEADERS
    include/mod3d_gui/ModelDocument.h
    include/mod3d_gui/MainWindow.h
    include/mod3d_gui/ProfileView.h
    include/mod3d_gui/MapView.h
    include/mod3d_gui/GL3DView.h
    include/mod3d_gui/SpreadsheetView.h
    include/mod3d_gui/Toolbars.h
    include/mod3d_gui/StatusBarManager.h
    include/mod3d_gui/dialogs/BodyPropertiesDialog.h
    include/mod3d_gui/dialogs/ModelPropertiesDialog.h
    include/mod3d_gui/dialogs/DefineObservationDialog.h
    include/mod3d_gui/dialogs/VerticalRangeDialog.h
    include/mod3d_gui/dialogs/InducingFieldDialog.h
    include/mod3d_gui/dialogs/ComputePropertiesDialog.h
    include/mod3d_gui/dialogs/ActiveGridsDialog.h
    include/mod3d_gui/dialogs/Fit1DDialog.h
    include/mod3d_gui/SettingsPane.h
)

set(GUI_SOURCES
    src/ModelDocument.cpp
    src/MainWindow.cpp
    src/ProfileView.cpp
    src/MapView.cpp
    src/GL3DView.cpp
    src/SpreadsheetView.cpp
    src/Toolbars.cpp
    src/StatusBarManager.cpp
    src/SettingsPane.cpp
    src/dialogs/BodyPropertiesDialog.cpp
    src/dialogs/ModelPropertiesDialog.cpp
    src/dialogs/DefineObservationDialog.cpp
    src/dialogs/VerticalRangeDialog.cpp
    src/dialogs/InducingFieldDialog.cpp
    src/dialogs/ComputePropertiesDialog.cpp
    src/dialogs/ActiveGridsDialog.cpp
    src/dialogs/Fit1DDialog.cpp
)

add_library(mod3d_gui_lib STATIC
    ${GUI_HEADERS}
    ${GUI_SOURCES}
)

target_include_directories(mod3d_gui_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mod3d_gui_lib PUBLIC
    mod3d_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL::GL
)

target_compile_definitions(mod3d_gui_lib PUBLIC
    GL_SILENCE_DEPRECATION
)

add_executable(mod3d_gui
    src/main.cpp
)

target_link_libraries(mod3d_gui PRIVATE
    mod3d_gui_lib
)
