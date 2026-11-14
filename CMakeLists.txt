cmake_minimum_required(VERSION 3.21)

project(Worldbilder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(worldbilder
    src/main.cpp
    src/MapFeature.h
    src/MapFeature.cpp
    src/MapView.h
    src/MapView.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(worldbilder PRIVATE Qt6::Widgets)

set_target_properties(worldbilder PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
