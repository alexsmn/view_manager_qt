cmake_minimum_required(VERSION 3.25)

project(view_manager_qt LANGUAGES CXX)

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
  # processes makes it flaky (a competing window steals activation and the
  # focus assertions fail before the test reaches its macOS golden-image skip).
  # Register it on its own with RUN_SERIAL so ctest never schedules it next to
  # another test; the remaining tests keep running in parallel.
  set(view_manager_qt_isolated_test
      "ViewManagerQtComponentIntegrationTest.PublicApiWorkflow")
  gtest_discover_tests(view_manager_qt_unittests DISCOVERY_MODE PRE_TEST
    TEST_FILTER "-${view_manager_qt_isolated_test}")
  gtest_discover_tests(view_manager_qt_unittests DISCOVERY_MODE PRE_TEST
    TEST_FILTER "${view_manager_qt_isolated_test}"
    PROPERTIES RUN_SERIAL TRUE)
endif()
