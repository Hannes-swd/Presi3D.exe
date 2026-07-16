cmake_minimum_required(VERSION 3.16)
project(Presi3D VERSION 1.12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets Network Svg)

set(SOURCES
    resources/icons.qrc
    src/main.cpp
    src/MainWindow.cpp
    src/LocalHttpServer.cpp
    src/UpdateChecker.cpp
    src/SlideListPanel.cpp
    src/EditorArea.cpp
    src/SlideEditor2D.cpp
    src/SlideEditor3D.cpp
    src/PropertiesPanel.cpp
    src/models/Presentation.cpp
    src/models/ChartData.cpp
    src/models/MeshGradientData.cpp
    src/MeshGradientRenderer.cpp
    src/models/VariableModel.cpp
    src/models/VariableEngine.cpp
    src/models/TimelineTrack.cpp
    src/models/TimelineEngine.cpp
    src/TimelinePanel.cpp
    src/rendering/ChartRenderer.cpp
    src/rendering/LatexRenderer.cpp
    src/rendering/CodeHighlighter.cpp
    src/export/HtmlExporter.cpp
    src/import/HtmlImporter.cpp
    src/dialogs/ExportDialog.cpp
    src/dialogs/StartDialog.cpp
    src/dialogs/InsertTableDialog.cpp
    src/dialogs/InsertChartDialog.cpp
    src/dialogs/InsertFormulaDialog.cpp
    src/dialogs/InsertIFrameDialog.cpp
    src/dialogs/InsertWorldObjectDialog.cpp
    src/dialogs/InsertButtonDialog.cpp
    src/dialogs/InsertCheckboxDialog.cpp
    src/dialogs/InsertSliderDialog.cpp
    src/dialogs/ChartEditorDialog.cpp
    src/dialogs/MeshGradientDialog.cpp
    src/dialogs/ShapePickerDialog.cpp
    src/dialogs/IconPickerDialog.cpp
    src/IconUtils.cpp
    src/IconPackManager.cpp
    src/dialogs/VariableManagerDialog.cpp
    src/FormatBar.cpp
    src/models/WorldObjectMesh.cpp
    src/third_party/tinygltf_impl.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src src/third_party/tinygltf)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::Svg
)

# To deploy Qt DLLs, run manually:
#   C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe <exe_path>
