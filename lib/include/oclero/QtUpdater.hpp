#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QSettings>

#include <memory>

namespace oclero {
/**
 * @brief Updater that checks for updates and download installer (Windows-only feature).
 * The Updater expects a JSON response like this from the server.
 * {
 *   "version": "x.y.z",
 *   "date": "dd/MM/YYYY",
 *   "checksum": "418397de9ef332cd0e477ff5e8ca38d4",
 *   "checksumType": "md5",
 *   "installerUrl": "http://server/endpoint/package-name.exe",
 *   "changelogUrl": "http://server/endpoint/changelog-name.md"
 * }
 */
class QtUpdater : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString temporaryDirectoryPath READ temporaryDirectoryPath WRITE setTemporaryDirectoryPath NOTIFY
      temporaryDirectoryPathChanged)
  Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
  Q_PROPERTY(bool installerAvailable READ installerAvailable NOTIFY installerAvailableChanged)
  Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
  Q_PROPERTY(QString latestChangelog READ latestChangelog NOTIFY latestChangelogChanged)
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
  Q_PROPERTY(Frequency frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
  Q_PROPERTY(QDateTime lastCheckTime READ lastCheckTime NOTIFY lastCheckTimeChanged)

public:
  enum class State {
    Idle,
    CheckingForUpdate,
    DownloadingChangelog,
    DownloadingInstaller,
    InstallingUpdate,
  };
  Q_ENUM(State)

  enum class Frequency {
    Never,
    EveryStart,
    EveryHour,
    EveryDay,
    EveryWeek,
    EveryTwoWeeks,
    EveryMonth,
  };
  Q_ENUM(Frequency)

  enum class InstallMode {
    ExecuteFile,
    MoveFileToDir,
  };
  Q_ENUM(InstallMode)

  struct SettingsParameters {
    QSettings::Format format;
    QSettings::Scope scope;
    QString organization;
    QString application;
  };

public:
  explicit QtUpdater(QObject* parent = nullptr);
  QtUpdater(const SettingsParameters& settingsParameters = {}, QObject* parent = nullptr);
  QtUpdater(const QString& serverUrl, const SettingsParameters& settingsParameters = {}, QObject* parent = nullptr);
  ~QtUpdater();

public:
  const QString& temporaryDirectoryPath() const;
  bool updateAvailable() const;
  bool changelogAvailable() const;
  bool installerAvailable() const;
  const QString& currentVersion() const;
  QString latestVersion() const;
  const QString& latestChangelog() const;
  State state() const;
  const QString& serverUrl() const;
  Frequency frequency() const;
  QDateTime lastCheckTime() const;
  int checkTimeout() const;

public slots:
  void setTemporaryDirectoryPath(const QString& path);
  void setServerUrl(const QString& serverUrl);
  void setFrequency(oclero::QtUpdater::Frequency frequency);
  void checkForUpdate();
  void forceCheckForUpdate();
  void downloadChangelog();
  void downloadInstaller();
  // Set dry to true if you don't want to quit the application.
  void installUpdate(InstallMode const mode, const QString& moveDestinationDir = {}, const bool quitAfter = true,
    const bool dry = false);
  void setCheckTimeout(int timeout);
  void cancel();

signals:
  void temporaryDirectoryPathChanged();
  void latestVersionChanged();
  void latestChangelogChanged();
  void stateChanged();
  void serverUrlChanged();
  void frequencyChanged();
  void lastCheckTimeChanged();
  void checkTimeoutChanged();

  void checkForUpdateStarted();
  void checkForUpdateProgressChanged(int percentage);
  void checkForUpdateFinished();
  void checkForUpdateOnlineFailed();
  void checkForUpdateFailed();
  void checkForUpdateCancelled();
  void updateAvailableChanged();

  void changelogDownloadStarted();
  void changelogDownloadProgressChanged(int percentage);
  void changelogDownloadFinished();
  void changelogDownloadFailed();
  void changelogDownloadCancelled();
  void changelogAvailableChanged();

  void installerDownloadStarted();
  void installerDownloadProgressChanged(int percentage);
  void installerDownloadFinished();
  void installerDownloadFailed();
  void installerDownloadCancelled();
  void installerAvailableChanged();

  void installationStarted();
  void installationFailed();
  // Emitted only when run in dry mode.
  void installationFinished();

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
} // namespace oclero
