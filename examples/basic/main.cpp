#include <QCoreApplication>
#include <QDebug>

#include <oclero/QtUpdater.hpp>

int main(int argc, char* argv[]) {
  QCoreApplication::setApplicationName("SingleInstanceExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QCoreApplication app(argc, argv);

  oclero::QtUpdater updater;

  QObject::connect(&updater, &oclero::QtUpdater::updateAvailableChanged, &updater, [&updater]() {
    if (updater.updateAvailable()) {
      qDebug() << "Update available! You have " << qPrintable(updater.currentVersion()) << " - Latest is "
               << qPrintable(updater.latestVersion());

      qDebug() << "Downloading changelog...";
      updater.downloadChangelog();
    }
  });

  QObject::connect(&updater, &oclero::QtUpdater::changelogAvailableChanged, &updater, [&updater]() {
    if (updater.changelogAvailable()) {
      qDebug() << "Changelog downloaded!\nHere's what's new:";
      qDebug() << updater.latestChangelog();

      qDebug() << "Downloading installer...";
      updater.downloadInstaller();
    }
  });

  QObject::connect(&updater, &oclero::QtUpdater::changelogAvailableChanged, &updater, [&updater]() {
    if (updater.installerAvailable()) {
      qDebug() << "Installer downloaded!";

      qDebug() << "Starting installer...";
      updater.installUpdate(/* dry */ true);
    }
  });

  QObject::connect(&updater, &oclero::QtUpdater::installationFinished, &updater, [&updater]() {
    qDebug() << "Installation done!";
  });

  updater.checkForUpdate();

  return app.exec();
}
