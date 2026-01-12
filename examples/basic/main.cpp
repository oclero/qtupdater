#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

#include <oclero/qtupdater/Updater.h>

int main(int argc, char* argv[]) {
  QCoreApplication::setApplicationName("BasicUpdaterExample");
  QCoreApplication::setApplicationVersion("1.0.0");
  QCoreApplication::setOrganizationName("example");
  QCoreApplication app(argc, argv);

  oclero::qtupdater::Updater updater;
  updater.setServerUrl("http://localhost:8000/");
  updater.setInstallerDestinationDir(QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).constFirst());
  updater.setInstallMode(oclero::qtupdater::InstallMode::MoveFileToDir);

  QObject::connect(&updater, &oclero::qtupdater::Updater::updateAvailabilityChanged, &updater, [&updater]() {
    if (updater.updateAvailability() == oclero::qtupdater::UpdateAvailability::Available) {
      qDebug() << "Update available! You have " << qPrintable(updater.currentVersion()) << " - Latest is "
               << qPrintable(updater.latestVersion());

      qDebug() << "Downloading changelog...";
      updater.downloadChangelog();
    }
  });

  QObject::connect(&updater, &oclero::qtupdater::Updater::changelogAvailableChanged, &updater, [&updater]() {
    if (updater.changelogAvailable()) {
      qDebug() << "Changelog downloaded!\nHere's what's new:";
      qDebug() << updater.latestChangelog();

      qDebug() << "Downloading installer...";
      updater.downloadInstaller();
    }
  });

  QObject::connect(&updater, &oclero::qtupdater::Updater::installerAvailableChanged, &updater, [&updater]() {
    if (updater.installerAvailable()) {
      qDebug() << "Installer downloaded!";

      qDebug() << "Starting installation...";

      updater.installUpdate(/* dry */ false);
    }
  });

  QObject::connect(&updater, &oclero::qtupdater::Updater::installationFinished, &updater, []() {
    qDebug() << "Installation done!";
  });

  updater.checkForUpdate();

  return app.exec();
}
