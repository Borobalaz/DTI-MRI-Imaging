cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(Connectomics-Imaging)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ITK CONFIG QUIET COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIONIFTI
    ITKIONRRD
    ITKIOMeta
    ITKIOGDCM
)
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets Svg)

find_path(STB_INCLUDE_DIR
    NAMES stb_image.h
    REQUIRED
)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/src/*.cpp"
)

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/widgets/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/windows/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/controllers/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/qt-adapters/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/styles/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/state/*.cpp"
)
add_library(engine SHARED ${ENGINE_SOURCES})
set_target_properties(engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

target_include_directories(engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${STB_INCLUDE_DIR}
)

target_compile_definitions(engine PRIVATE
    "CONNECTOMICS_SHADER_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/engine\""
)

target_link_libraries(engine PUBLIC
    OpenGL::GL
    glm::glm
    glad::glad
    assimp::assimp
)

target_link_libraries(engine PRIVATE
    nlohmann_json::nlohmann_json
)

if (ITK_FOUND)
    target_link_libraries(engine PRIVATE ${ITK_LIBRARIES})
    target_compile_definitions(engine PRIVATE CONNECTOMICS_ENABLE_ITK_IO=1)
endif()

add_executable(app_qt
    ${UI_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/src/app/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/controllers/MainWindowShortcuts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/OpenGLViewportWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/MainToolBar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/InspectProviderWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/InspectorWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/state/RenderStatistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/RenderStatisticsWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/SceneObjectListWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/widgets/inspect_fields/InspectNumberFieldWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/windows/WidgetsMainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include/qt-adapters/QTSceneInspector.h
)

target_include_directories(app_qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/include
)

target_compile_definitions(app_qt PRIVATE
    "CONNECTOMICS_STYLE_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/ui/styles\""
)

target_link_libraries(app_qt PRIVATE
    engine
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Widgets
    Qt6::Svg
)

if (WIN32)
    target_link_libraries(app_qt PRIVATE dwmapi)
endif()

# Copy shader files to build output directory (same location as executable)
add_custom_command(TARGET app_qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/engine/shaders"
            "$<TARGET_FILE_DIR:app_qt>/shaders"
    COMMENT "Copying shaders to build output..."
)

# Also copy assets (models, textures, volumes)
add_custom_command(TARGET app_qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/engine/assets"
            "$<TARGET_FILE_DIR:app_qt>/assets"
    COMMENT "Copying assets to build output..."
)

# Also copy the Qt stylesheets/tokens (ui/styles/widgets/**/*.qss, ui/styles/tokens/*.ini),
#  for deployed builds that don't have the source tree (StyleSheetComposer prefers
#  CONNECTOMICS_STYLE_SOURCE_DIR when it exists, so a dev build picks up edits directly,
#  without needing this copy)
add_custom_command(TARGET app_qt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/ui/styles"
            "$<TARGET_FILE_DIR:app_qt>/styles"
    COMMENT "Copying styles to build output..."
)