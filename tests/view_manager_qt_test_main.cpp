#include <QApplication>
#include <gmock/gmock.h>

int main(int argc, char** argv) {
  testing::InitGoogleMock(&argc, argv);

  QApplication app{argc, argv};
  return RUN_ALL_TESTS();
}
