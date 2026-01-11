#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QIcon>

#include <oclero/qtupdater/Updater.h>
#include <oclero/qtupdater/Controller.h>

#include "QtUpdateWidget.h"

int main(int argc, char* argv[]) {
  Q_INIT_RESOURCE(resources);

  QCoreApplication::setApplicationName("QtWidgetsUpdaterExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QApplication app(argc, argv);
  QApplication::setWindowIcon(QIcon(":/example/icon.ico"));

  // 1. Create updater backend.
  oclero::qtupdater::Updater updater;
  updater.setServerUrl("http://localhost:8000/");
  updater.setFrequency(oclero::qtupdater::Frequency::Never);

  // 2. Create update dialog controller.
  oclero::qtupdater::Controller updateCtrl(updater);

  // 3. Create and show dialog.
  auto* widget = new oclero::qtupdater::QtUpdateWidget(updateCtrl);
  widget->show();

  return app.exec();
}
