cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
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
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets)
find_package(ITK CONFIG QUIET COMPONENTS
    ITKCommon
    ITKIOImageBase
    ITKIONIFTI
    ITKIONRRD
    ITKIOMeta
    ITKIOGDCM
)

find_path(STB_INCLUDE_DIR
    NAMES stb_image.h
    REQUIRED
)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(QT_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
file(GLOB_RECURSE QT_HOST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/widgets/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/windows/*.cpp"
)
list(REMOVE_ITEM SOURCES "${QT_MAIN}" ${QT_HOST_SOURCES})
list(REMOVE_DUPLICATES SOURCES)

add_executable(app_qt
    ${SOURCES}
    ${QT_MAIN}
    ${QT_HOST_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/controllers/MainWindowShortcuts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/OpenGLViewportWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/DTIViewportWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/InspectorWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/state/RenderStatistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/RenderStatisticsWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/SceneObjectListWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/widgets/inspect_fields/InspectNumberFieldWidget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/windows/WidgetsMainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/qt-adapters/QTSceneInspector.h
)

target_include_directories(app_qt PRIVATE
    include
    include/app
    include/core
    include/engine
    include/ui
    ${STB_INCLUDE_DIR}
)

target_link_libraries(app_qt PRIVATE
    OpenGL::GL
    glm::glm
    glad::glad
    assimp::assimp
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Widgets
)

if (ITK_FOUND)
    target_link_libraries(app_qt PRIVATE ${ITK_LIBRARIES})
    target_compile_definitions(app_qt PRIVATE CONNECTOMICS_ENABLE_ITK_IO=1)
endif()

