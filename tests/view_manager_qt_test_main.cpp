// From build-support/, put on the include path by the product root; see the
// header for why it lives there and not beside this file.
#include "scada_qt_offscreen_platform.h"

#include <QApplication>
#include <gmock/gmock.h>

int main(int argc, char** argv) {
  testing::InitGoogleMock(&argc, argv);

  // Listing the tests needs no Qt. ctest's test discovery runs this binary
  // with --gtest_list_tests under a 5-second limit, and building the
  // QApplication first took longer than that on a Windows CI runner, so
  // discovery failed before a single test ran (scada-client run 36342865771).
  if (GTEST_FLAG_GET(list_tests))
    return RUN_ALL_TESTS();

  // Before QApplication, which reads QT_QPA_PLATFORM in its constructor.
  scada::qt_test::DefaultToOffscreenPlatform();

  QApplication app{argc, argv};
  return RUN_ALL_TESTS();
}
