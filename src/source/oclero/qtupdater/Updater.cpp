#include <oclero/qtupdater/Updater.h>

#include <oclero/qtupdater/Downloader.h>
#include <oclero/qtupdater/AppCast.h>
#include <oclero/qtupdater/Types.h>

#include "utils/FileUtils.h"
#include "utils/SettingsUtils.h"

#include <QLoggingCategory>
#include <QFile>
#include <QVersionNumber>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>

#include <optional>

Q_LOGGING_CATEGORY(CATEGORY_UPDATER, "oclero.qtupdater")

#if !defined UPDATER_ENABLE_DEBUG
#  define UPDATER_ENABLE_DEBUG 0
#endif

namespace oclero::qtupdater {
constexpr auto SETTINGS_KEY_LASTCHECKTIME = "Update/LastCheckTime";
constexpr auto SETTINGS_KEY_FREQUENCY = "Update/CheckFrequency";
constexpr auto SETTINGS_KEY_LASTUPDATEJSON = "Update/LastUpdateJSON";

struct UpdateInfo {
  AppCast appCast;
  QFileInfo installer;
  QFileInfo changelog;
  utils::LazyFileContent changelogContent;

  bool isValid() const {
    return appCast.isValid();
  }

  bool readyToDisplayChangelog() const {
    return isValid() && changelog.exists() && changelog.isFile();
  }

  bool readyToInstall() const {
#if defined(Q_OS_WIN)
    const auto isOSInstaller = installer.isExecutable();
#elif defined(Q_OS_MAC)
    const auto isOSInstaller = true;
#else
    const auto isOSInstaller = true;
#endif

    return isValid() && installer.exists() && installer.isFile() && isOSInstaller;
  }

  const QString& getChangelogContent() {
    if (changelog.exists()) {
      changelogContent.setPath(changelog.absoluteFilePath());
    }
    return changelogContent.getContent();
  }
};

struct Updater::Impl {
  Updater& owner;
  Downloader downloader;
  SettingsParameters settingsParameters;
  QString serverUrl;
  bool serverUrlInitialized{ false };
  UpdaterState state{ UpdaterState::Idle };
  UpdateInfo localUpdateInfo;
  UpdateInfo onlineUpdateInfo;
  Frequency frequency{ Frequency::EveryDay };
  QDateTime lastCheckTime;
  int checkTimeout{ Downloader::DefaultTimeout };
  QTimer timer;
  QString downloadsDir{ utils::getDefaultTemporaryDirectoryPath() };
  QString currentVersion{ QCoreApplication::applicationVersion() };
  QDateTime currentVersionDate;
  InstallMode installMode{ InstallMode::ExecuteFile };
  QString installerDestinationDir;
  std::function<QJsonObject(const QJsonDocument&)> customParser{ nullptr };

  Impl(Updater& o, const SettingsParameters& p = {})
    : owner(o)
    , settingsParameters(p) {
    // Load settings.
    QSettings settings(settingsParameters.format, settingsParameters.scope, settingsParameters.organization,
      settingsParameters.application);
    const auto lastCheckTimeInSettings = utils::loadSetting<QString>(settings, SETTINGS_KEY_LASTCHECKTIME);
    lastCheckTime = QDateTime::fromString(lastCheckTimeInSettings, Qt::DateFormat::ISODate);

    const auto freq = utils::tryLoadSetting<Frequency>(settings, SETTINGS_KEY_FREQUENCY);
    if (freq) {
      frequency = freq.value();
    } else {
      utils::saveSetting(settings, SETTINGS_KEY_FREQUENCY, frequency);
    }

    // Setup timer, for hourly checking for updates.
    timer.setInterval(std::chrono::seconds(3600));
    timer.setTimerType(Qt::TimerType::VeryCoarseTimer); // No need for precision.
    QObject::connect(&timer, &QTimer::timeout, &o, [this]() {
      owner.checkForUpdate();
    });
    if (frequency == Frequency::EveryHour) {
      timer.start();
    }
  }

  void setState(UpdaterState const value) {
    if (value != state) {
      state = value;
      emit owner.stateChanged();
    }
  }

  const UpdateInfo* mostRecentUpdate() const {
    if (onlineUpdateInfo.isValid()) {
      // Priority is the update from the server.
      return &onlineUpdateInfo;
    } else if (localUpdateInfo.isValid()) {
      // Then, local update, if one.
      return &localUpdateInfo;
    } else {
      return nullptr;
    }
  }

  UpdateAvailability updateAvailability() const {
    const auto update = mostRecentUpdate();
    if (update && update->isValid()) {
      const auto currentVersionNumber = QVersionNumber::fromString(currentVersion);
      const auto& newVersionNumber = update->appCast.version;
      const auto newUpdateAvailable = QVersionNumber::compare(currentVersionNumber, newVersionNumber) < 0;
      return newUpdateAvailable ? UpdateAvailability::Available : UpdateAvailability::UpToDate;
    }
    return UpdateAvailability::Unknown;
  }

  bool changelogAvailable() const {
    return updateAvailability() == UpdateAvailability::Available ? mostRecentUpdate()->readyToDisplayChangelog()
                                                                 : false;
  }

  bool installerAvailable() const {
    if (updateAvailability() == UpdateAvailability::Available) {
      return installMode == InstallMode::ExecuteFile ? mostRecentUpdate()->readyToInstall() : true;
    }
    return false;
  }

  bool shouldCheckForUpdate() const {
    auto shouldCheckForUpdate = true;

    if (lastCheckTime.isValid()) {
      const auto currentTime = QDateTime::currentDateTime();
      QDateTime comparisonTime;
      switch (frequency) {
        case Frequency::EveryStart:
          comparisonTime = lastCheckTime;
          break;
        case Frequency::EveryHour:
          comparisonTime = lastCheckTime.addSecs(3600);
          break;
        case Frequency::EveryDay:
          comparisonTime = lastCheckTime.addDays(1);
          break;
        case Frequency::EveryWeek:
          comparisonTime = lastCheckTime.addDays(7);
          break;
        case Frequency::EveryTwoWeeks:
          comparisonTime = lastCheckTime.addDays(14);
          break;
        case Frequency::EveryMonth:
          comparisonTime = lastCheckTime.addMonths(1);
          break;
        default:
          break;
      }
      shouldCheckForUpdate = comparisonTime < currentTime;
    }

    return shouldCheckForUpdate;
  }

  UpdateInfo checkForLocalUpdate() const {
    // Check presence of a JSON file.
    QSettings settings(settingsParameters.format, settingsParameters.scope, settingsParameters.organization,
      settingsParameters.application);
    const auto optFilePath = utils::tryLoadSetting<QString>(settings, SETTINGS_KEY_LASTUPDATEJSON);

    if (!optFilePath.has_value()) {
      return UpdateInfo{};
    }

    const auto& filePath = optFilePath.value();
    if (filePath.isEmpty()) {
      return UpdateInfo{};
    }

    QFile infoFile(filePath);
    if (!infoFile.exists()) {
      infoFile.close();
      return UpdateInfo{};
    }

#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Found previously downloaded update data";
#endif

    // Try to open it.
    if (!infoFile.open(QIODevice::ReadOnly)) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Cannot open local JSON file";
#endif
      infoFile.close();
      return UpdateInfo{};
    }

    // Read it.
    auto localAppCast = AppCast{};
    localAppCast.fromJson(infoFile.readAll());
    infoFile.close();

    if (!localAppCast.isValid()) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Previously downloaded data is invalid";
#endif
      return UpdateInfo{};
    }

    // Check presence of changelog and installer files along with the JSON file.
    const auto changelogFileName = localAppCast.changelogUrl.fileName();
    QFileInfo localChangelog(downloadsDir + '/' + changelogFileName);
    const auto installerFileName = localAppCast.installerUrl.fileName();
    QFileInfo localInstaller(downloadsDir + '/' + installerFileName);

    // Remove exisiting files if the whole bundle is not present.
    const auto allFilesExist =
      localChangelog.exists() && localChangelog.isFile() && localInstaller.exists() && localInstaller.isFile();
    if (!allFilesExist) {
      utils::clearDirectoryContent(downloadsDir);
      return UpdateInfo{};
    }

    return UpdateInfo{ localAppCast, localInstaller, localChangelog, {} };
  }

  void notifyUpdateAvailable(const bool newUpdateAvailable) {
    // Signals for GUI.
    setState(UpdaterState::Idle);
    emit owner.checkForUpdateFinished();
    if (newUpdateAvailable) {
      emit owner.latestVersionChanged();
      emit owner.latestVersionDateChanged();
    }
    emit owner.updateAvailabilityChanged();
  };

  void onCheckForUpdateFinished(const QByteArray& data, bool cancelled, UpdaterError errorCode) {
    if (cancelled) {
      onlineUpdateInfo = {};
      localUpdateInfo = {};
      emit owner.checkForUpdateCancelled();
      notifyUpdateAvailable(false);
      return;
    }

    // Save online info.
    auto downloadedAppCast = AppCast{};
    downloadedAppCast.fromJson(data, customParser);
    onlineUpdateInfo = UpdateInfo{ downloadedAppCast, {}, {}, {} };

    // Check for previously downloaded update, locally.
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Checking if an update is locally available...";
#endif
    localUpdateInfo = checkForLocalUpdate();

    // Order of priority:
    // 1. Online information (if available and valid).
    // 2. Local information, previously downloaded (if available and valid).
    const auto* update = mostRecentUpdate();
    if (!update) {
#if UPDATER_ENABLE_DEBUG
      qDebug(CATEGORY_UPDATER) << "No update available";
#endif
      emit owner.checkForUpdateFailed(errorCode);
      notifyUpdateAvailable(false);
      return;
    }

    // If the most recent is the one from the server,
    // wipe existing files because there are obsolete.
    if (update == &onlineUpdateInfo) {
      utils::clearDirectoryContent(downloadsDir);

      // Write downloaded JSON to disk.
      const auto saveJSONFilePath = update->appCast.saveToFile(downloadsDir);
      if (!saveJSONFilePath.has_value()) {
        emit owner.checkForUpdateFailed(UpdaterError::DiskError);
        notifyUpdateAvailable(false);
        return;
      }

      QSettings settings(settingsParameters.format, settingsParameters.scope, settingsParameters.organization,
        settingsParameters.application);
      utils::saveSetting(settings, SETTINGS_KEY_LASTUPDATEJSON, saveJSONFilePath.value());
    }

    // Compare version numbers.
    const auto currentVersionNumber = QVersionNumber::fromString(currentVersion);
    const auto& newVersion = update->appCast.version;
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Current:" << currentVersion << "- Latest:" << newVersion;
#endif

    // An update is available if the version is superior to the previous one.
    const auto newUpdateAvailable = QVersionNumber::compare(currentVersionNumber, newVersion) < 0;

    // Signals for GUI.
    notifyUpdateAvailable(newUpdateAvailable);
  }

  void onDownloadChangelogFinished(const QString& filePath) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Changelog downloaded @" << filePath;
#endif
    onlineUpdateInfo.changelog = QFileInfo(filePath);

    setState(UpdaterState::Idle);
    emit owner.changelogDownloadFinished();
    emit owner.changelogAvailableChanged();
    emit owner.latestChangelogChanged();
  }

  void onDownloadInstallerFinished(const QString& filePath) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Installer downloaded @" << filePath;
#endif
    onlineUpdateInfo.installer = QFileInfo(filePath);

    const auto checksumIsValid = Downloader::verifyFileChecksum(
      filePath, onlineUpdateInfo.appCast.checksum, onlineUpdateInfo.appCast.checksumType);
    setState(UpdaterState::Idle);

    if (!checksumIsValid) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Checksum is invalid";
#endif
      emit owner.installerDownloadFailed(UpdaterError::ChecksumError);
      return;
    }
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Checksum is valid";
#endif
    emit owner.installerDownloadFinished();
    emit owner.installerAvailableChanged();
  }
};

UpdaterError mapError(DownloaderError error) {
  switch (error) {
    case DownloaderError::NoError:
      return UpdaterError::NoError;
    case DownloaderError::UrlIsInvalid:
      return UpdaterError::UrlError;
    case DownloaderError::LocalDirIsInvalid:
    case DownloaderError::CannotCreateLocalDir:
    case DownloaderError::CannotRemoveFile:
    case DownloaderError::NotAllowedToWriteFile:
    case DownloaderError::FileDoesNotExistOrIsCorrupted:
    case DownloaderError::FileDoesNotEndWithSuffix:
    case DownloaderError::CannotRenameFile:
      return UpdaterError::DiskError;
    case DownloaderError::NetworkError:
      return UpdaterError::NetworkError;
    default:
      return UpdaterError::UnknownError;
  }
}

Updater::Updater(QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this)) {}

Updater::Updater(const QString& serverUrl, QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this, SettingsParameters{})) {
  setServerUrl(serverUrl);
}

Updater::Updater(const QString& serverUrl, const SettingsParameters& settingsParameters, QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this, settingsParameters)) {
  setServerUrl(serverUrl);
}

Updater::~Updater() {}

void Updater::setCustomJsonParser(const std::function<QJsonObject(const QJsonDocument&)>& customParser) {
  _impl->customParser = customParser;
}

const QString& Updater::temporaryDirectoryPath() const {
  return _impl->downloadsDir;
}

void Updater::setTemporaryDirectoryPath(const QString& path) {
  if (path != _impl->downloadsDir) {
    _impl->downloadsDir = path;
    emit temporaryDirectoryPathChanged();
  }
}

UpdateAvailability Updater::updateAvailability() const {
  return _impl->updateAvailability();
}

bool Updater::changelogAvailable() const {
  return _impl->changelogAvailable();
}

bool Updater::installerAvailable() const {
  return _impl->installerAvailable();
}

const QString& Updater::serverUrl() const {
  return _impl->serverUrl;
}

void Updater::setServerUrl(const QString& serverUrl) {
  if (serverUrl != _impl->serverUrl) {
    _impl->serverUrl = serverUrl;
    emit serverUrlChanged();

    // Reset data.
    _impl->localUpdateInfo = {};
    _impl->onlineUpdateInfo = {};
    _impl->timer.stop();
    _impl->timer.start();

    // If previous was empty, it means it was not yet set.
    if (_impl->serverUrlInitialized) {
      _impl->lastCheckTime = {};
      emit updateAvailabilityChanged();
      emit installerAvailableChanged();
    } else {
      _impl->serverUrlInitialized = true;
    }
  }
}

const QString& Updater::currentVersion() const {
  return _impl->currentVersion;
}

const QDateTime& Updater::currentVersionDate() const {
  return _impl->currentVersionDate;
}

QString Updater::latestVersion() const {
  if (_impl->onlineUpdateInfo.isValid()) {
    return _impl->onlineUpdateInfo.appCast.version.toString();
  } else if (_impl->localUpdateInfo.isValid()) {
    return _impl->localUpdateInfo.appCast.version.toString();
  } else {
    return _impl->currentVersion;
  }
}

QDateTime Updater::latestVersionDate() const {
  if (_impl->onlineUpdateInfo.isValid()) {
    return _impl->onlineUpdateInfo.appCast.date;
  } else if (_impl->localUpdateInfo.isValid()) {
    return _impl->localUpdateInfo.appCast.date;
  } else {
    return _impl->currentVersionDate;
  }
}

const QString& Updater::latestChangelog() const {
  static const QString fallback;
  if (const auto update = const_cast<UpdateInfo*>(_impl->mostRecentUpdate())) {
    return update->getChangelogContent();
  }
  return fallback;
}

UpdaterState Updater::state() const {
  return _impl->state;
}

Frequency Updater::frequency() const {
  return _impl->frequency;
}

void Updater::setFrequency(Frequency frequency) {
  if (frequency != _impl->frequency) {
    _impl->frequency = frequency;
    emit frequencyChanged();

    // Start timer if hourly check.
    if (frequency == Frequency::EveryHour) {
      _impl->timer.start();
    }
  }
}

QDateTime Updater::lastCheckTime() const {
  return _impl->lastCheckTime;
}

int Updater::checkTimeout() const {
  return _impl->checkTimeout;
}

void Updater::setCheckTimeout(int timeout) {
  if (timeout != _impl->checkTimeout) {
    _impl->checkTimeout = timeout;
    emit checkTimeoutChanged();
  }
}

InstallMode Updater::installMode() const {
  return _impl->installMode;
}

void Updater::setInstallMode(InstallMode installMode) {
  if (installMode != _impl->installMode) {
    _impl->installMode = installMode;
    emit installModeChanged();
  }
}

const QString& Updater::installerDestinationDir() const {
  return _impl->installerDestinationDir;
}

void Updater::setInstallerDestinationDir(const QString& path) {
  if (path != _impl->installerDestinationDir) {
    _impl->installerDestinationDir = path;
    emit installerDestinationDirChanged();
  }
}

void Updater::cancel() {
  const auto currentState = state();
  if (currentState == UpdaterState::Idle || currentState == UpdaterState::InstallingUpdate)
    return;

  _impl->downloader.cancel();
  _impl->state = UpdaterState::Idle;
  emit stateChanged();
}

void Updater::checkForUpdate() {
  if (state() != UpdaterState::Idle || _impl->serverUrl.isEmpty()) {
    return;
  }

  if (_impl->shouldCheckForUpdate()) {
    forceCheckForUpdate();
  }
}

void Updater::forceCheckForUpdate() {
  emit checkForUpdateForced();

  if (state() != UpdaterState::Idle || _impl->serverUrl.isEmpty()) {
    return;
  }

  // Validate URL before attempting download.
  QUrl url(_impl->serverUrl);
  if (!url.isValid() || url.scheme().isEmpty()) {
    _impl->setState(UpdaterState::Idle);
    emit checkForUpdateFailed(UpdaterError::UrlError);
    return;
  }

  // Reset data.
  _impl->localUpdateInfo = {};
  _impl->onlineUpdateInfo = {};

  // Change last checked time.
  _impl->lastCheckTime = QDateTime::currentDateTime();
  QSettings settings(_impl->settingsParameters.format, _impl->settingsParameters.scope,
    _impl->settingsParameters.organization, _impl->settingsParameters.application);
  const auto dateAsString = _impl->lastCheckTime.toString(Qt::DateFormat::ISODate);
  utils::saveSetting(settings, SETTINGS_KEY_LASTCHECKTIME, dateAsString);
  emit lastCheckTimeChanged();

  // Start checking.
  _impl->setState(UpdaterState::CheckingForUpdate);
  emit checkForUpdateStarted();

#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Checking for updates @" << url.toString() << "...";
#endif

  _impl->downloader.downloadData(
    _impl->serverUrl,
    [this](DownloaderError const errorCode, const QByteArray& data) {
      if (errorCode != DownloaderError::NoError) {
        emit checkForUpdateOnlineFailed();
      }
      const auto cancelled = errorCode == DownloaderError::Cancelled;
      const auto mappedErrorCode = mapError(errorCode);
      _impl->onCheckForUpdateFinished(data, cancelled, mappedErrorCode);
    },
    [this](int const percentage) {
      emit checkForUpdateProgressChanged(percentage);
    },
    _impl->checkTimeout);
}

void Updater::downloadChangelog() {
  if (state() != UpdaterState::Idle) {
    return;
  }

  // Check if local changelog.
  if (!_impl->onlineUpdateInfo.isValid()) {
    if (_impl->localUpdateInfo.readyToDisplayChangelog()) {
      emit changelogAvailableChanged();
    }
    return;
  }

  _impl->setState(UpdaterState::DownloadingChangelog);
  emit changelogDownloadStarted();
  const auto& url = _impl->onlineUpdateInfo.appCast.changelogUrl;

#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Downloading changelog @" << url.toString() << "...";
#endif

  if (!url.isValid()) {
    _impl->setState(UpdaterState::Idle);
    emit changelogDownloadFailed(UpdaterError::UrlError);
    return;
  }
  const auto& dir = _impl->downloadsDir;
  _impl->downloader.downloadFile(
    url, dir,
    [this](DownloaderError const errorCode, const QString& filePath) {
      if (errorCode == DownloaderError::NoError) {
        _impl->onDownloadChangelogFinished(filePath);
      } else if (errorCode == DownloaderError::Cancelled) {
        _impl->setState(UpdaterState::Idle);
        emit changelogDownloadCancelled();
      } else {
        _impl->setState(UpdaterState::Idle);
        emit changelogDownloadFailed(mapError(errorCode));
      }
    },
    [this](int const percentage) {
      emit changelogDownloadProgressChanged(percentage);
    },
    _impl->checkTimeout);
}

void Updater::downloadInstaller() {
  if (state() != UpdaterState::Idle) {
    return;
  }

  // Priority is given to server updates.
  if (!_impl->onlineUpdateInfo.isValid()) {
    // However, there might be a previously downloaded update, locally.
    if (_impl->localUpdateInfo.readyToInstall()) {
      emit installerAvailableChanged();
    }
    return;
  }

  _impl->setState(UpdaterState::DownloadingInstaller);
  emit installerDownloadStarted();

  const auto& url = _impl->onlineUpdateInfo.appCast.installerUrl;

#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Downloading installer @" << url.toString() << "...";
#endif

  if (!url.isValid()) {
    _impl->setState(UpdaterState::Idle);
    emit installerDownloadFailed(UpdaterError::UrlError);
    return;
  }
  const auto& dir = _impl->downloadsDir;
  _impl->downloader.downloadFile(
    url, dir,
    [this](DownloaderError const errorCode, const QString& filePath) {
      _impl->setState(UpdaterState::Idle);
      if (errorCode == DownloaderError::NoError) {
        _impl->onDownloadInstallerFinished(filePath);
      } else if (errorCode == DownloaderError::Cancelled) {
        emit installerDownloadCancelled();
      } else {
        emit installerDownloadFailed(mapError(errorCode));
      }
    },
    [this](int const percentage) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Downloading installer..." << percentage << "%";
#endif
      emit installerDownloadProgressChanged(percentage);
    },
    _impl->checkTimeout);
}

void Updater::installUpdate(const bool dry) {
  const auto raiseError = [this](UpdaterError error, const char* msg = nullptr) {
    Q_UNUSED(msg);
#if UPDATER_ENABLE_DEBUG
    if (msg) {
      qCDebug(CATEGORY_UPDATER) << msg;
    }
#endif
    emit installationFailed(error);
  };

  if (state() != UpdaterState::Idle || !_impl->installerAvailable()) {
    raiseError(UpdaterError::UnknownError, "Installer not available");
    return;
  }

  emit installationStarted();
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Installing update...";
#endif
  _impl->setState(UpdaterState::InstallingUpdate);

  // Should not be null because 'installerAvailable()' returned 'true'.
  const auto update = _impl->mostRecentUpdate();
  assert(update);
  if (!update) {
    return;
  }

  // Verify checksum before installing.
  if (update->appCast.checksumType != ChecksumType::NoChecksum) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Verifying checksum...";
#endif
    if (!Downloader::verifyFileChecksum(
          update->installer.absoluteFilePath(), update->appCast.checksum, update->appCast.checksumType)) {
      raiseError(UpdaterError::ChecksumError, "Checksum is invalid");
      return;
    } else {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Checksum is valid";
#endif
    }
  }

  // For the tests, we don't stop the application.
  if (dry) {
    _impl->setState(UpdaterState::Idle);
    emit installationFinished();
    return;
  }

  // Start installer in a separate process.
  if (_impl->installMode == InstallMode::ExecuteFile) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Starting installer...";
#endif
    auto installerProcessSuccess = false;
#if defined(Q_OS_WIN)
    installerProcessSuccess = QProcess::startDetached(update->installer.absoluteFilePath(), {});
#elif defined(Q_OS_MAC)
    installerProcessSuccess = QProcess::startDetached("open", { update->installer.absoluteFilePath() });
#else
    raiseError(UpdaterError::InstallerExecutionError, "OS not supported");
#endif
    if (!installerProcessSuccess) {
      raiseError(UpdaterError::InstallerExecutionError, "Failed to start uninstaller");
      _impl->setState(UpdaterState::Idle);
      return;
    }
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Installer started";
#endif

    // Quit the app.
    if (_impl->installMode == InstallMode::ExecuteFile) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "App will quit to let the installer do the update";
#endif
      QCoreApplication::quit();
    }
  } else if (_impl->installMode == InstallMode::MoveFileToDir && !_impl->installerDestinationDir.isEmpty()) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Moving file...";
#endif
    const auto installerPath = update->installer.absoluteFilePath();
    const auto fileName = update->installer.fileName();
    const auto movedInstallerPath = _impl->installerDestinationDir + '/' + fileName;
    if (!QFile::copy(installerPath, movedInstallerPath)) {
      raiseError(UpdaterError::DiskError, "Can't copy file to new destination");
    }
    if (!QFile::remove(installerPath)) {
      raiseError(UpdaterError::DiskError, "Can't remove temporary file");
    }
  }

  _impl->setState(UpdaterState::Idle);
  emit installationFinished();
}
} // namespace oclero::qtupdater

#if defined UPDATER_ENABLE_DEBUG
#  undef UPDATER_ENABLE_DEBUG
#endif
