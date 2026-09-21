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

# The shared kit is on the include path so a test entry point can say
# `#include "scada_qt_offscreen_platform.h"`. It resolves in both layouts --
# SCADA_PRODUCT_SEARCH_ROOT is the tree root in the monorepo and this product
# root in an export, and `build-support/` sits beneath it in both -- and it is
# set here rather than by scada_qt_import_offscreen_platform_into_tests(),
# which returns early on a Qt with no offscreen plugin. Tying the include path
# to that guard would turn a missing plugin into a compile error in the file
# that exists to cope with one.
include_directories("${SCADA_BUILD_SUPPORT_DIR}")

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
    # The shared golden-image helper's own test, from the build kit. One copy,
    # compiled by every product that uses the header, so each still verifies
    # it in its own build. Named by absolute path because the kit is at the
    # tree root in the monorepo and at this product's root in an export.
    "${SCADA_BUILD_SUPPORT_DIR}/scada_qt_golden_image_unittest.cpp"
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

# Last, because it walks the targets that exist by now: the test executable
# gets the offscreen platform plugin linked in, so a static Qt can honour the
# headless default `view_manager_qt_test_main.cpp` applies on macOS and so
# `QT_QPA_PLATFORM=offscreen` works for a standalone ctest run.
#
# Neither was true until 2026-09-20 (backlog 808). The plugin is built by
# qtbase's `freetype` feature, which this product's manifest did not ask for
# with default features off, so a standalone Qt here had cocoa and minimal and
# nothing else; and the test main built a bare QApplication, so a `ctest` sweep
# bounced a Dock icon once per case. Spliced into the client both were masked --
# the client's qtbase keeps its default features, and since 799 the client's own
# root call reaches this target.
scada_qt_import_offscreen_platform_into_tests()
