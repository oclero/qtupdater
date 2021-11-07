#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>

#include <memory>

namespace oclero {
/**
 * @brief Updater that checks for updates and download installer (Windows-only feature).
 * The server should handle responses for serverurl/{win|mac}[?version=latest]
 * The Updater expects a JSON response like this from the server.
 * {
 *   "version": "x.y.z",
 *   "date": "dd/MM/YYYY",
 *   "checksum": "418397de9ef332cd0e477ff5e8ca38d4",
 *   "checksumType": "md5",
 *   "installerUrl": "http://your-server/win/YourApp-x.y.z.t-Windows-64bit.exe",
 *   "changelogUrl": "http://your-server/win/YourApp-x.y.z.t--Windows-64bit.md"
 * }
 */
class QtUpdater : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
  Q_PROPERTY(bool installerAvailable READ installerAvailable NOTIFY installerAvailableChanged)
  Q_PROPERTY(QString currentVersion READ currentVersion)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
  Q_PROPERTY(QString currentChangelog READ currentChangelog NOTIFY currentChangelogChanged)
  Q_PROPERTY(QString latestChangelog READ latestChangelog NOTIFY latestChangelogChanged)
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
  Q_PROPERTY(Frequency frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
  Q_PROPERTY(QDateTime lastCheckTime READ lastCheckTime NOTIFY lastCheckTimeChanged)

public:
  enum State {
    Idle,
    CheckingForUpdate,
    DownloadingChangelog,
    DownloadingInstaller,
    InstallingUpdate,
  };
  Q_ENUM(State)

  enum Frequency {
    Never,
    EveryStart,
    EveryHour,
    EveryDay,
    EveryWeek,
    EveryTwoWeeks,
    EveryMonth,
  };
  Q_ENUM(Frequency)

public:
  explicit QtUpdater(QObject* parent = nullptr);
  QtUpdater(const QString& serverUrl, QObject* parent = nullptr);
  ~QtUpdater();

public:
  bool updateAvailable() const;
  bool changelogAvailable() const;
  bool installerAvailable() const;
  const QString& currentVersion() const;
  QString latestVersion() const;
  const QString& currentChangelog() const;
  const QString& latestChangelog() const;
  State state() const;
  const QString& serverUrl() const;
  Frequency frequency() const;
  QDateTime lastCheckTime() const;

public slots:
  void setServerUrl(const QString& serverUrl);
  void setFrequency(Frequency frequency);
  void checkForUpdate();
  void forceCheckForUpdate();
  void downloadChangelog();
  void downloadInstaller();
  // Set dry to true if you don't want to quit the application.
  void installUpdate(bool const dry = false);

signals:
  void latestVersionChanged();
  void currentChangelogChanged();
  void latestChangelogChanged();
  void stateChanged();
  void serverUrlChanged();
  void frequencyChanged();
  void lastCheckTimeChanged();

  void checkForUpdateStarted();
  void checkForUpdateProgressChanged(int percentage);
  void checkForUpdateFinished();
  void checkForUpdateOnlineFailed();
  void checkForUpdateFailed();
  void updateAvailableChanged();

  void changelogDownloadStarted();
  void changelogDownloadProgressChanged(int percentage);
  void changelogDownloadFinished();
  void changelogDownloadFailed();
  void changelogAvailableChanged();

  void installerDownloadStarted();
  void installerDownloadProgressChanged(int percentage);
  void installerDownloadFinished();
  void installerDownloadFailed();
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
