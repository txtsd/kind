#include "logging.hpp"

#include <QApplication>
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
  // QGuiApplication satisfies both core tests (QCoreApplication) and GUI
  // renderer tests (QPainter/QTextLayout) since it inherits QCoreApplication.
  QApplication app(argc, argv);

  kind::log::init_console_only();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
