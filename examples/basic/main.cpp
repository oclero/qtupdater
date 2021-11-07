#include <QCoreApplication>
#include <QDebug>

#include <oclero/QtUpdater.hpp>

int main(int argc, char* argv[]) {
  QCoreApplication::setApplicationName("SingleInstanceExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QCoreApplication app(argc, argv);

  oclero::QtUpdater updater;

  return app.exec();
}
