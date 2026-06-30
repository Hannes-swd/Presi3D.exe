cmake_minimum_required(VERSION 3.16)
project(Presi3D VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/SlideListPanel.cpp
    src/EditorArea.cpp
    src/SlideEditor2D.cpp
    src/SlideEditor3D.cpp
    src/PropertiesPanel.cpp
    src/models/Presentation.cpp
    src/export/HtmlExporter.cpp
    src/import/HtmlImporter.cpp
    src/dialogs/ExportDialog.cpp
    src/dialogs/StartDialog.cpp
    src/dialogs/InsertTableDialog.cpp
    src/FormatBar.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

# To deploy Qt DLLs, run manually:
#   C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe <exe_path>
