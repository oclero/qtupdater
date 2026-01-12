#pragma once

#include <oclero/qtupdater/Updater.h>

#include <QObject>
#include <QDateTime>

namespace oclero::qtupdater {
/**
 * @brief Controller for the Update dialog. Might be used with a QtWidgets-based or QtQuick-based dialog.
 */
class Controller : public QObject {
  Q_OBJECT

  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString currentVersion READ currentVersion NOTIFY currentVersionChanged)
  Q_PROPERTY(QDateTime currentVersionDate READ currentVersionDate NOTIFY currentVersionDateChanged)
  Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
  Q_PROPERTY(QDateTime latestVersionDate READ latestVersionDate NOTIFY latestVersionDateChanged)
  Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
  Q_PROPERTY(QString latestVersionChangelog READ latestVersionChangelog NOTIFY latestVersionChangelogChanged)

public:
  enum class State {
    None,
    Checking,
    CheckingFail,
    CheckingSuccess,
    CheckingUpToDate,
    Downloading,
    DownloadingFail,
    DownloadingSuccess,
    Installing,
    InstallingFail,
    InstallingSuccess,
  };
  Q_ENUM(State)

public:
  explicit Controller(Updater& updater, QObject* parent = nullptr);
  ~Controller() override = default;

  State state() const;
  QString currentVersion() const;
  QDateTime currentVersionDate() const;
  QString latestVersion() const;
  QDateTime latestVersionDate() const;
  QString latestVersionChangelog() const;
  int downloadProgress() const;

private:
  void setState(State state);
  void setDownloadProgress(int);

public slots:
  void cancel();
  void checkForUpdate();
  void forceCheckForUpdate();
  void downloadUpdate();
  void installUpdate();

signals:
  void stateChanged();
  void currentVersionChanged();
  void currentVersionDateChanged();
  void latestVersionChanged();
  void latestVersionDateChanged();
  void latestVersionChangelogChanged();
  void downloadProgressChanged(int);
  void manualCheckingRequested();
  void closeDialogRequested();
  void checkForUpdateErrorChanged(oclero::qtupdater::UpdaterError code);
  void changelogDownloadErrorChanged(oclero::qtupdater::UpdaterError code);
  void updateDownloadErrorChanged(oclero::qtupdater::UpdaterError code);
  void updateInstallationErrorChanged(oclero::qtupdater::UpdaterError code);

  void linuxDownloadUpdateRequested();

private:
  Updater& _updater;
  State _state{ State::None };
  int _downloadProgress{ 0 };
};
} // namespace oclero::qtupdater
