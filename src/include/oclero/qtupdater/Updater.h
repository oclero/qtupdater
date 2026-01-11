#pragma once

#include <oclero/qtupdater/Types.h>

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>

#include <memory>

namespace oclero::qtupdater {
/**
 * @brief Updater that checks for updates and downloads installer.
 */
class Updater : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString temporaryDirectoryPath READ temporaryDirectoryPath WRITE setTemporaryDirectoryPath NOTIFY
      temporaryDirectoryPathChanged)
  Q_PROPERTY(
    oclero::qtupdater::UpdateAvailability updateAvailability READ updateAvailability NOTIFY updateAvailabilityChanged)
  Q_PROPERTY(bool installerAvailable READ installerAvailable NOTIFY installerAvailableChanged)
  Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
  Q_PROPERTY(QDateTime currentVersionDate READ currentVersionDate CONSTANT)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
  Q_PROPERTY(QDateTime latestVersionDate READ latestVersionDate NOTIFY latestVersionDateChanged)
  Q_PROPERTY(QString latestChangelog READ latestChangelog NOTIFY latestChangelogChanged)
  Q_PROPERTY(oclero::qtupdater::UpdaterState state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
  Q_PROPERTY(Frequency frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
  Q_PROPERTY(QDateTime lastCheckTime READ lastCheckTime NOTIFY lastCheckTimeChanged)
  Q_PROPERTY(oclero::qtupdater::InstallMode installMode READ installMode WRITE setInstallMode NOTIFY installModeChanged)
  Q_PROPERTY(QString installerDestinationDir READ installerDestinationDir WRITE setInstallerDestinationDir NOTIFY
      installerDestinationDirChanged)

public:
  explicit Updater(QObject* parent = nullptr);
  explicit Updater(const QString& serverUrl, QObject* parent = nullptr);
  explicit Updater(const QString& serverUrl, const SettingsParameters& settingsParameters, QObject* parent = nullptr);
  ~Updater() override;

public:
  /**
   * @brief Set the Custom Json Parser object. By default, the parser expects a JSON object with the
   * following structure:
   * {
   * "latestVersion": "1.2.3",
   * "latestVersionDate": "2023-01-01T12:00:00Z",
   * "changelog": "Changelog text here",
   * "installerUrl": "https://example.com/installer.exe",
   * }
   *
   * If your server returns a different structure, you can provide a custom parser function to extract
   * the necessary information.
   *
   * @param customParser A function that takes a QJsonDocument and returns a QJsonObject with the expected structure.
   */
  void setCustomJsonParser(const std::function<QJsonObject(const QJsonDocument&)>& customParser);

  const QString& temporaryDirectoryPath() const;
  void setTemporaryDirectoryPath(const QString& path);

  const QString& serverUrl() const;
  void setServerUrl(const QString& serverUrl);

  void setFrequency(Frequency frequency);
  Frequency frequency() const;

  UpdateAvailability updateAvailability() const;
  bool changelogAvailable() const;
  bool installerAvailable() const;
  const QString& currentVersion() const;
  const QDateTime& currentVersionDate() const;
  QString latestVersion() const;
  QDateTime latestVersionDate() const;
  const QString& latestChangelog() const;
  UpdaterState state() const;
  QDateTime lastCheckTime() const;
  int checkTimeout() const;
  InstallMode installMode() const;
  const QString& installerDestinationDir() const;

public slots:
  void checkForUpdate();
  void forceCheckForUpdate();
  void downloadChangelog();
  void downloadInstaller();
  // Set dry to true if you don't want to quit the application.
  void installUpdate(const bool dry = false);
  void setCheckTimeout(int timeout);
  void setInstallMode(oclero::qtupdater::InstallMode mode);
  void setInstallerDestinationDir(const QString& path);
  void cancel();

signals:
  void temporaryDirectoryPathChanged();
  void latestVersionChanged();
  void latestVersionDateChanged();
  void latestChangelogChanged();
  void stateChanged();
  void serverUrlChanged();
  void frequencyChanged();
  void lastCheckTimeChanged();
  void installModeChanged();
  void installerDestinationDirChanged();
  void checkTimeoutChanged();

  void checkForUpdateForced();
  void checkForUpdateStarted();
  void checkForUpdateProgressChanged(int percentage);
  void checkForUpdateFinished();
  void checkForUpdateOnlineFailed();
  void checkForUpdateFailed(oclero::qtupdater::UpdaterError error);
  void checkForUpdateCancelled();
  void updateAvailabilityChanged();

  void changelogDownloadStarted();
  void changelogDownloadProgressChanged(int percentage);
  void changelogDownloadFinished();
  void changelogDownloadFailed(oclero::qtupdater::UpdaterError error);
  void changelogDownloadCancelled();
  void changelogAvailableChanged();

  void installerDownloadStarted();
  void installerDownloadProgressChanged(int percentage);
  void installerDownloadFinished();
  void installerDownloadFailed(oclero::qtupdater::UpdaterError error);
  void installerDownloadCancelled();
  void installerAvailableChanged();

  void installationStarted();
  void installationFailed(oclero::qtupdater::UpdaterError error);
  // Emitted only when run in dry mode.
  void installationFinished();

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
} // namespace oclero::qtupdater
