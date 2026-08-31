cmake_minimum_required(VERSION 3.25)

# The shared build-support kit (ADR 0011): at this product's root in an export,
# at the tree root in the monorepo. Before project(), so the machine config it
# reads can still set the vcpkg variables the toolchain consumes there.
set(_scada_dir "${CMAKE_CURRENT_SOURCE_DIR}")
while(NOT EXISTS "${_scada_dir}/build-support/ScadaProducts.cmake")
  get_filename_component(_scada_parent "${_scada_dir}" DIRECTORY)
  if(_scada_parent STREQUAL _scada_dir)
    message(FATAL_ERROR
      "build-support/ not found above ${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  set(_scada_dir "${_scada_parent}")
endwhile()
include("${_scada_dir}/build-support/ScadaProducts.cmake")
include("${_scada_dir}/build-support/ScadaProductBase.cmake")
unset(_scada_dir)
unset(_scada_parent)

scada_product_prologue()

project(view_manager_qt LANGUAGES CXX)

scada_product_base()


include(CTest)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

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
  Qt6::Widgets
)

set_target_properties(view_manager_qt PROPERTIES
  AUTOMOC ON
)

option(VIEW_MANAGER_QT_BUILD_TESTS "Build view_manager_qt unit tests" ${BUILD_TESTING})

if(VIEW_MANAGER_QT_BUILD_TESTS)
  find_package(GTest REQUIRED)
  include(GoogleTest)

  add_executable(view_manager_qt_unittests
    tests/dock_tab_widget_unittest.cpp
    tests/golden_image.h
    tests/golden_image_unittest.cpp
    tests/view_manager_qt_component_integration_unittest.cpp
    tests/view_manager_qt_component_unittest.cpp
    tests/view_manager_qt_test_main.cpp
  )

  target_link_libraries(view_manager_qt_unittests PRIVATE
    view_manager_qt
    GTest::gmock
    Qt6::Widgets
  )

  target_compile_definitions(view_manager_qt_unittests PRIVATE
    VIEW_MANAGER_QT_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata"
  )

  set_target_properties(view_manager_qt_unittests PROPERTIES
    AUTOMOC ON
  )

  # PublicApiWorkflow shows a real QMainWindow and asserts on window
  # activation, active-view focus, and a tab context menu. Those depend on the
  # process owning the GUI focus, so running it alongside other Qt test
  # processes makes it flaky. Register it on its own with RUN_SERIAL so ctest
  # never schedules it next to another test; the remaining tests keep running
  # in parallel.
  #
  # RUN_SERIAL narrows the window but cannot close it, and this comment used to
  # imply otherwise. Window activation is machine-wide: ctest can stop a
  # *sibling test* contending for it, and nothing here can stop a foreground
  # application that is not a test at all -- a client left running on the
  # developer's desktop is enough, and reproduced this failure 5/5. The test
  # itself therefore gates its focus-derived assertions on
  # `isActiveWindow()`; see the comment at that gate for why the coverage is
  # not lost.
  set(view_manager_qt_isolated_test
      "ViewManagerQtComponentIntegrationTest.PublicApiWorkflow")
  gtest_discover_tests(view_manager_qt_unittests DISCOVERY_MODE PRE_TEST
    TEST_FILTER "-${view_manager_qt_isolated_test}")
  gtest_discover_tests(view_manager_qt_unittests DISCOVERY_MODE PRE_TEST
    TEST_FILTER "${view_manager_qt_isolated_test}"
    PROPERTIES RUN_SERIAL TRUE)
endif()
