cmake_minimum_required(VERSION 3.25)

project(view_manager_qt LANGUAGES CXX)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_library(view_manager_qt
  dock_tab_widget.cpp
  dock_tab_widget.h
  view_manager_qt_component.cpp
  view_manager_qt_component.h
)

add_library(ViewManagerQt::view_manager_qt ALIAS view_manager_qt)

target_compile_features(view_manager_qt PUBLIC cxx_std_20)

target_include_directories(view_manager_qt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(view_manager_qt PUBLIC
  Qt5::Widgets
)

set_target_properties(view_manager_qt PROPERTIES
  AUTOMOC ON
)
