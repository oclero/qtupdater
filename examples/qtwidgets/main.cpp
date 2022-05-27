#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QIcon>

#include <oclero/QtUpdater.hpp>
#include <oclero/QtUpdateController.hpp>

#include "QtUpdateWidget.hpp"

int main(int argc, char* argv[]) {
  Q_INIT_RESOURCE(resources);

  QCoreApplication::setApplicationName("QtWidgetsUpdaterExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QApplication app(argc, argv);
  QApplication::setWindowIcon(QIcon(":/example/icon.ico"));

  // 1. Create updater backend.
  oclero::QtUpdater updater;
  updater.setServerUrl("http://localhost:8000/");

  // 2. Create update dialog controller.
  oclero::QtUpdateController updateCtrl(updater);

  // 3. Create and show dialog.
  auto* widget = new oclero::QtUpdateWidget(updateCtrl);
  widget->show();

  return app.exec();
}
