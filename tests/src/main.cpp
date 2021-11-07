#include <QTest>
#include <QCoreApplication>

#include "QtUpdaterTests.hpp"

int main(int argc, char* argv[]) {
  QTEST_SET_MAIN_SOURCE_PATH;

  // Necessary to get a socket name and to have an event loop running.
  QCoreApplication::setApplicationName("QtUpdaterTests");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("oclero");
  QCoreApplication app(argc, argv);

  Tests tests;
  const auto success = QTest::qExec(&tests) == 0;
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
