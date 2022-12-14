#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

#include <oclero/QtUpdater.hpp>

int main(int argc, char* argv[]) {
  QCoreApplication::setApplicationName("BasicUpdaterExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QCoreApplication app(argc, argv);

  oclero::QtUpdater updater;
  updater.setServerUrl("http://localhost:8000/");
  updater.setInstallerDestinationDir(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).first());
  updater.setInstallMode(oclero::QtUpdater::InstallMode::MoveFileToDir);

  QObject::connect(&updater, &oclero::QtUpdater::updateAvailabilityChanged, &updater, [&updater]() {
    if (updater.updateAvailability() == oclero::QtUpdater::UpdateAvailability::Available) {
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

  QObject::connect(&updater, &oclero::QtUpdater::installerAvailableChanged, &updater, [&updater]() {
    if (updater.installerAvailable()) {
      qDebug() << "Installer downloaded!";

      qDebug() << "Starting installation...";

      updater.installUpdate(/* dry */ false);
    }
  });

  QObject::connect(&updater, &oclero::QtUpdater::installationFinished, &updater, []() {
    qDebug() << "Installation done!";
  });

  updater.checkForUpdate();

  return app.exec();
}
