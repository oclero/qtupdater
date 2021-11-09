#include <oclero/QtUpdater.hpp>

#include <oclero/QtDownloader.hpp>

#include "EnumUtils.hpp"
#include "SettingsUtils.hpp"

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
#include <QStandardPaths>
#include <QProcess>

#include <optional>

Q_LOGGING_CATEGORY(CATEGORY_UPDATER, "oclero.qtupdater")

#if !defined UPDATER_ENABLE_DEBUG
#  define UPDATER_ENABLE_DEBUG 0
#endif

namespace utils {
void clearDirectoryContent(const QString& dirPath, const QStringList& extensionFilter = {}) {
  QDir dir(dirPath);
  if (!dir.exists()) {
    return;
  }

  const auto& entryInfoList = extensionFilter.isEmpty()
                                ? dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)
                                : dir.entryInfoList(extensionFilter, QDir::Files | QDir::Hidden);
  for (const auto& entryInfo : entryInfoList) {
    const auto entryAbsPath = entryInfo.absoluteFilePath();
    if (entryInfo.isDir()) {
      QDir subDir(entryAbsPath);
      subDir.removeRecursively();
    } else {
      dir.remove(entryAbsPath);
    }
  }
}

QString getTemporaryDirectoryPath() {
  QString result;

  const auto dirs = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::TempLocation);
  if (!dirs.isEmpty()) {
    result = dirs.first();

    const auto subDirectories = {
      QCoreApplication::organizationName(),
      QCoreApplication::applicationName(),
    };

    QStringList subDirectoriesList;
    for (const auto& subDirectory : subDirectories) {
      if (!subDirectory.isEmpty()) {
        subDirectoriesList << subDirectory;
      }
    }

    result += '/' + subDirectoriesList.join('/');
  }

  return result;
}
} // namespace utils

namespace oclero {
constexpr auto JSON_DATETIME_FORMAT = "dd/MM/yyyy";
constexpr auto JSON_TAG_CHECKSUM = "checksum";
constexpr auto JSON_TAG_CHECKSUM_TYPE = "checksumType";
constexpr auto JSON_TAG_DATE = "date";
constexpr auto JSON_TAG_INSTALLER_URL = "installerUrl";
constexpr auto JSON_TAG_CHANGELOG_URL = "changelogUrl";
constexpr auto JSON_TAG_VERSION = "version";

constexpr auto SETTINGS_KEY_LASTCHECKTIME = "Update/LastCheckTime";
constexpr auto SETTINGS_KEY_FREQUENCY = "Update/CheckFrequency";
constexpr auto SETTINGS_KEY_LASTUPDATEJSON = "Update/LastUpdateJSON";

constexpr auto CURRENT_CHANGELOG_FILEPATH = "CHANGELOG.md";

constexpr auto TEMP_DIRECTORY = "/Update";

class LazyFileContent {
public:
  LazyFileContent(const QString& p = {})
    : path(p) {}

  void setPath(const QString& p) {
    if (p != path) {
      path = p;
      content.reset();
    }
  }

  const QString& getContent() {
    if (!content.has_value()) {
      QFile file(path);
      if (path.isEmpty()) {
        content = QString(); // Mark as read.
      } else if (file.open(QIODevice::ReadOnly)) {
        content = QString::fromUtf8(file.readAll());
      } else {
        content = QString(); // Mark as read.
      }
      file.close();
    }
    return content.value();
  }

private:
  QString path;
  std::optional<QString> content;
};

struct UpdateJSON {
  QVersionNumber version;
  QUrl installerUrl;
  QUrl changelogUrl;
  QByteArray checksum;
  QtDownloader::ChecksumType checksumType{ QtDownloader::ChecksumType::NoChecksum };
  QDateTime date;

  UpdateJSON() = default;

  UpdateJSON(const QByteArray& data) {
    const auto jsonDocument = QJsonDocument::fromJson(data);
    if (!jsonDocument.isNull() && jsonDocument.isObject()) {
      const auto jsonObject = jsonDocument.object();
      if (!jsonObject.isEmpty()) {
        if (jsonObject.contains(JSON_TAG_VERSION)) {
          version = QVersionNumber::fromString(jsonObject[JSON_TAG_VERSION].toString());
        }

        if (jsonObject.contains(JSON_TAG_CHANGELOG_URL)) {
          changelogUrl = QUrl(jsonObject[JSON_TAG_CHANGELOG_URL].toString());
        }

        if (jsonObject.contains(JSON_TAG_INSTALLER_URL)) {
          installerUrl = QUrl(jsonObject[JSON_TAG_INSTALLER_URL].toString());
        }

        if (jsonObject.contains(JSON_TAG_CHECKSUM)) {
          checksum = jsonObject[JSON_TAG_CHECKSUM].toString().toUtf8();
        }

        if (jsonObject.contains(JSON_TAG_CHECKSUM_TYPE)) {
          checksumType =
            enumFromString<QtDownloader::ChecksumType>(jsonObject[JSON_TAG_CHECKSUM_TYPE].toString().toUpper());
        }

        if (jsonObject.contains(JSON_TAG_DATE)) {
          date = QDateTime::fromString(jsonObject[JSON_TAG_DATE].toString(), JSON_DATETIME_FORMAT);
        }
      }
    }
  }

  bool isValid() const {
    const auto validVersionNumber = !version.isNull();
    const auto validInstallerUrl = installerUrl.isEmpty() || installerUrl.isValid();
    const auto validChangelogUrl = changelogUrl.isEmpty() || changelogUrl.isValid();
    const auto validDate = date.isValid();

    auto validChecksum = true;
    if (checksumType != QtDownloader::ChecksumType::NoChecksum) {
      auto qtAlgorithm = QCryptographicHash::Md5;
      switch (checksumType) {
        case QtDownloader::ChecksumType::MD5:
          qtAlgorithm = QCryptographicHash::Algorithm::Md5;
          break;
        case QtDownloader::ChecksumType::SHA1:
          qtAlgorithm = QCryptographicHash::Algorithm::Sha1;
          break;
        default:
          break;
      }

      validChecksum = !checksum.isEmpty() && checksum.size() == 2 * QCryptographicHash::hashLength(qtAlgorithm);
    }
    return validVersionNumber && validInstallerUrl && validChangelogUrl && validDate && validChecksum;
  }

  QByteArray toJSON() const {
    if (!isValid()) {
      return QByteArray();
    }

    // Create JSON object.
    QJsonObject jsonObject({
      { JSON_TAG_VERSION, version.toString() },
      { JSON_TAG_INSTALLER_URL, installerUrl.toString() },
      { JSON_TAG_CHANGELOG_URL, changelogUrl.toString() },
      { JSON_TAG_CHECKSUM, checksum.constData() },
      { JSON_TAG_CHECKSUM_TYPE, enumToString(checksumType).toLower() },
      { JSON_TAG_DATE, date.toString(JSON_DATETIME_FORMAT) },
    });

    return QJsonDocument(jsonObject).toJson(QJsonDocument::JsonFormat::Compact);
  }

  bool saveToFile(const QString& dirPath, QString& savePath) const {
    if (!isValid()) {
      return false;
    }

    const auto filename = QFileInfo(installerUrl.fileName()).completeBaseName();
    const auto filePath = dirPath + '/' + filename + ".json";
    savePath = filePath;
    QFile file(filePath);

    // Remove existing JSON file, if one.
    if (file.exists()) {
      if (!file.remove()) {
        return false;
      }
    }

    // Create directory if not existing yet.
    QDir const dir(dirPath);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        return false;
      }
    }

    // Write file.
    if (!file.open(QIODevice::WriteOnly)) {
      return false;
    }

    const auto data = toJSON();
    if (data.isEmpty()) {
      return false;
    }

    file.write(data);
    file.close();

    return true;
  }
};

struct UpdateInfo {
  UpdateJSON json;
  QFileInfo installer;
  QFileInfo changelog;
  LazyFileContent changelogContent;

  bool isValid() const {
    return json.isValid();
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
    const auto isOSInstaller = false;
#endif

    return isValid() && installer.exists() && installer.isFile() && isOSInstaller;
  }

  const QString& getChangelogContent() {
    changelogContent.setPath(changelog.absoluteFilePath());
    return changelogContent.getContent();
  }
};

struct QtUpdater::Impl {
  QtUpdater& owner;
  QString serverUrl;
  bool serverUrlInitialized{ false };
  State state{ State::Idle };
  QtDownloader downloader;
  UpdateInfo localUpdateInfo;
  UpdateInfo onlineUpdateInfo;
  Frequency frequency{ Frequency::EveryDay };
  QDateTime lastCheckTime;
  QTimer timer;
  QString downloadsDir;
  LazyFileContent currentChangelog{ QCoreApplication::applicationDirPath() + '/' + CURRENT_CHANGELOG_FILEPATH };
  QString currentVersion{ QCoreApplication::applicationVersion() };

  Impl(QtUpdater& o)
    : owner(o) {
    // Load settings.
    const auto lastCheckTimeInSettings = loadSetting<QString>(SETTINGS_KEY_LASTCHECKTIME);
    lastCheckTime = QDateTime::fromString(lastCheckTimeInSettings, Qt::DateFormat::ISODate);

    const auto freq = tryLoadSetting<Frequency>(SETTINGS_KEY_FREQUENCY);
    if (freq) {
      frequency = freq.value();
    } else {
      saveSetting(SETTINGS_KEY_FREQUENCY, frequency);
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

    downloadsDir = utils::getTemporaryDirectoryPath() + TEMP_DIRECTORY;
  }

  void setState(State const value) {
    if (value != state) {
      state = value;
      emit owner.stateChanged();
    }
  }

  QUrl getCheckForUpdatesUrl() const {
    // Ask server for JSON that contains update information.
    const auto query = "?version=latest";
#if defined(Q_OS_WIN)
    return serverUrl + "/win" + query;
#elif defined(Q_OS_MAC)
    return serverUrl + "/mac" + query;
#else
    return QUrl{};
#endif
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

  bool updateAvailable() const {
    const auto update = mostRecentUpdate();
    if (update && update->isValid()) {
      const auto currentVersionNumber = QVersionNumber::fromString(currentVersion);
      const auto newVersionNumber = update->json.version;
      const auto newUpdateAvailable = QVersionNumber::compare(currentVersionNumber, newVersionNumber) < 0;
      return newUpdateAvailable;
    }
    return false;
  }

  bool changelogAvailable() const {
    return updateAvailable() ? mostRecentUpdate()->readyToDisplayChangelog() : false;
  }

  bool installerAvailable() const {
    return updateAvailable() ? mostRecentUpdate()->readyToInstall() : false;
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
    const auto optFilePath = tryLoadSetting<QString>(SETTINGS_KEY_LASTUPDATEJSON);

    if (!optFilePath.has_value()) {
      return UpdateInfo();
    }

    const auto filePath = optFilePath.value();
    if (filePath.isEmpty()) {
      return UpdateInfo();
    }

    QFile infoFile(filePath);
    if (!infoFile.exists()) {
      infoFile.close();
      return UpdateInfo();
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
      return UpdateInfo();
    }

    // Read it.
    const auto localJSON = UpdateJSON(infoFile.readAll());
    infoFile.close();

    if (!localJSON.isValid()) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Previously downloaded data is invalid";
#endif
      return UpdateInfo();
    }

    // Check presence of changelog and installer files along with the JSON file.
    const auto changelogFileName = localJSON.changelogUrl.fileName();
    QFileInfo localChangelog(downloadsDir + '/' + changelogFileName);
    const auto installerFileName = localJSON.installerUrl.fileName();
    QFileInfo localInstaller(downloadsDir + '/' + installerFileName);

    // Remove exisiting files if the whole bundle is not present.
    const auto allFilesExist =
      localChangelog.exists() && localChangelog.isFile() && localInstaller.exists() && localInstaller.isFile();
    if (!allFilesExist) {
      utils::clearDirectoryContent(downloadsDir);
      return UpdateInfo();
    }

    return UpdateInfo{ localJSON, localInstaller, localChangelog };
  }

  void onCheckForUpdateFinished(const QByteArray & data) {
    const auto notifyUpdateAvailable = [this](bool const newUpdateAvailable) {
      // Signals for GUI.
      setState(Idle);
      emit owner.checkForUpdateFinished();
      if (newUpdateAvailable) {
        emit owner.latestVersionChanged();
      }
      emit owner.updateAvailableChanged();
    };

    // Save online info.
    const auto downloadedJSON = UpdateJSON(data);
    onlineUpdateInfo = UpdateInfo{ downloadedJSON };

    // Check for previously downloaded update, locally.
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Checking if an update is locally available...";
#endif
    localUpdateInfo = checkForLocalUpdate();

    // Order of priority:
    // 1. Online information (if available and valid).
    // 2. Local information, previously downloaded (if available and valid).
    const auto update = mostRecentUpdate();
    if (!update) {
#if UPDATER_ENABLE_DEBUG
      qDebug(CATEGORY_UPDATER) << "No update available";
#endif
      emit owner.checkForUpdateFailed();
      notifyUpdateAvailable(false);
      return;
    }

    // If the most recent is the one from the server,
    // wipe existing files because there are obsolete.
    if (update == &onlineUpdateInfo) {
      utils::clearDirectoryContent(downloadsDir);

      // Write downloaded JSON to disk.
      QString saveJSONFilePath;
      if (!update->json.saveToFile(downloadsDir, saveJSONFilePath)) {
        emit owner.checkForUpdateFailed();
        notifyUpdateAvailable(false);
        return;
      }
      saveSetting(SETTINGS_KEY_LASTUPDATEJSON, saveJSONFilePath);
    }

    // Compare version numbers.
    const auto currentVersionNumber = QVersionNumber::fromString(currentVersion);
    const auto& newVersion = update->json.version;
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

    setState(Idle);
    emit owner.changelogDownloadFinished();
    emit owner.changelogAvailableChanged();
  }

  void onDownloadInstallerFinished(const QString& filePath) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Installer downloaded @" << filePath;
#endif
    onlineUpdateInfo.installer = QFileInfo(filePath);

    const auto checksumIsValid =
      QtDownloader::verifyFileChecksum(filePath, onlineUpdateInfo.json.checksum, onlineUpdateInfo.json.checksumType);
    if (!checksumIsValid) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Checksum is invalid";
#endif
      emit owner.installerDownloadFailed();
      return;
    }
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Checksum is valid";
#endif
    setState(Idle);
    emit owner.installerDownloadFinished();
    emit owner.installerAvailableChanged();
  }
};

#pragma region Ctor / Dtor

QtUpdater::QtUpdater(QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this)) {}

QtUpdater::QtUpdater(const QString& serverUrl, QObject* parent)
  : QtUpdater(parent) {
  setServerUrl(serverUrl);
}

QtUpdater::~QtUpdater() {}

#pragma endregion

#pragma region Properties

bool QtUpdater::updateAvailable() const {
  return _impl->updateAvailable();
}

bool QtUpdater::changelogAvailable() const {
  return _impl->changelogAvailable();
}

bool QtUpdater::installerAvailable() const {
  return _impl->installerAvailable();
}

const QString& QtUpdater::serverUrl() const {
  return _impl->serverUrl;
}

void QtUpdater::setServerUrl(const QString& serverUrl) {
  if (serverUrl != _impl->serverUrl) {
    _impl->serverUrl = serverUrl;

    // Reset data.
    _impl->localUpdateInfo = {};
    _impl->onlineUpdateInfo = {};
    _impl->lastCheckTime = {};
    _impl->timer.stop();
    _impl->timer.start();

    emit serverUrlChanged();

    // If previous was empty, it means it was not yet set.
    if (_impl->serverUrlInitialized) {
      emit updateAvailableChanged();
      emit installerAvailableChanged();
    } else {
      _impl->serverUrlInitialized = true;
    }
  }
}

const QString& QtUpdater::currentVersion() const {
  return _impl->currentVersion;
}

QString QtUpdater::latestVersion() const {
  if (_impl->onlineUpdateInfo.isValid()) {
    return _impl->onlineUpdateInfo.json.version.toString();
  } else if (_impl->localUpdateInfo.isValid()) {
    return _impl->localUpdateInfo.json.version.toString();
  } else {
    return _impl->currentVersion;
  }
}

const QString& QtUpdater::currentChangelog() const {
  return _impl->currentChangelog.getContent();
}

const QString& QtUpdater::latestChangelog() const {
  static const QString fallback;
  if (const auto update = const_cast<UpdateInfo*>(_impl->mostRecentUpdate())) {
    return update->getChangelogContent();
  }
  return fallback;
}

QtUpdater::State QtUpdater::state() const {
  return _impl->state;
}

QtUpdater::Frequency QtUpdater::frequency() const {
  return _impl->frequency;
}

void QtUpdater::setFrequency(Frequency frequency) {
  if (frequency != _impl->frequency) {
    _impl->frequency = frequency;
    emit frequencyChanged();

    // Start timer if hourly check.
    if (frequency == Frequency::EveryHour) {
      _impl->timer.start();
    }
  }
}

QDateTime QtUpdater::lastCheckTime() const {
  return _impl->lastCheckTime;
}

#pragma endregion

#pragma region Public slots

void QtUpdater::checkForUpdate() {
  if (state() != Idle || _impl->serverUrl.isEmpty()) {
    return;
  }

  if (_impl->shouldCheckForUpdate()) {
    forceCheckForUpdate();
  }
}

void QtUpdater::forceCheckForUpdate() {
  if (state() != Idle) {
    return;
  }

  // Reset data.
  _impl->localUpdateInfo = {};
  _impl->onlineUpdateInfo = {};

  // Change last checked time.
  _impl->lastCheckTime = QDateTime::currentDateTime();
  saveSetting(SETTINGS_KEY_LASTCHECKTIME, _impl->lastCheckTime.toString(Qt::DateFormat::ISODate));
  emit lastCheckTimeChanged();

  // Start checking.
  _impl->setState(CheckingForUpdate);
  emit checkForUpdateStarted();

  const auto url = _impl->getCheckForUpdatesUrl();
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Checking for updates at" << url.toString() << "...";
#endif

  _impl->downloader.downloadData(
    url,
    [this](QtDownloader::ErrorCode const errorCode, const QByteArray & data) {
      if (errorCode != QtDownloader::ErrorCode::NoError) {
        emit checkForUpdateOnlineFailed();
      }
      _impl->onCheckForUpdateFinished(data);
    },
    [this](int const percentage) {
      emit checkForUpdateProgressChanged(percentage);
    });
}

void QtUpdater::downloadChangelog() {
  if (state() != Idle) {
    return;
  }

  // Check if local changelog.
  if (!_impl->onlineUpdateInfo.isValid()) {
    if (_impl->localUpdateInfo.readyToDisplayChangelog()) {
      emit changelogAvailableChanged();
    }
    return;
  }

  _impl->setState(DownloadingChangelog);
  emit changelogDownloadStarted();
  const auto& url = _impl->onlineUpdateInfo.json.changelogUrl;

#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Downloading changelog @" << url.toString() << "...";
#endif

  if (!url.isValid()) {
    emit changelogDownloadFailed();
    return;
  }
  const auto& dir = _impl->downloadsDir;
  _impl->downloader.downloadFile(
    url, dir,
    [this](QtDownloader::ErrorCode const errorCode, const QString& filePath) {
      if (errorCode == QtDownloader::ErrorCode::NoError) {
        _impl->onDownloadChangelogFinished(filePath);
      } else {
        emit changelogDownloadFailed();
      }
    },
    [this](int const percentage) {
      emit changelogDownloadProgressChanged(percentage);
    });
}

void QtUpdater::downloadInstaller() {
  if (state() != Idle) {
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

  _impl->setState(DownloadingInstaller);
  emit installerDownloadStarted();

  const auto& url = _impl->onlineUpdateInfo.json.installerUrl;

#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Downloading installer @" << url.toString() << "...";
#endif

  if (!url.isValid()) {
    emit installerDownloadFailed();
    return;
  }
  const auto& dir = _impl->downloadsDir;
  _impl->downloader.downloadFile(
    url, dir,
    [this](QtDownloader::ErrorCode const errorCode, const QString& filePath) {
      if (errorCode == QtDownloader::ErrorCode::NoError) {
        _impl->onDownloadInstallerFinished(filePath);
      } else {
        emit installerDownloadFailed();
      }
    },
    [this](int const percentage) {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Downloading installer..." << percentage << "%";
#endif
      emit installerDownloadProgressChanged(percentage);
    });
}

void QtUpdater::installUpdate(bool const dry) {
  const auto raiseError = [this](const char* msg = nullptr) {
    Q_UNUSED(msg);
#if UPDATER_ENABLE_DEBUG
    if (msg) {
      qCDebug(CATEGORY_UPDATER) << msg;
    }
#endif
    emit installationFailed();
  };

  if (state() != Idle || !_impl->installerAvailable()) {
    raiseError("Installer not available");
    return;
  }

  emit installationStarted();
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Installing update...";
#endif
  _impl->setState(InstallingUpdate);

  // Should not be null because 'installerAvailable()' returned 'true'.
  const auto update = _impl->mostRecentUpdate();
  assert(update);
  if (!update) {
    return;
  }

  // Verify checksum before installing.
  if (update->json.checksumType != QtDownloader::ChecksumType::NoChecksum) {
#if UPDATER_ENABLE_DEBUG
    qCDebug(CATEGORY_UPDATER) << "Verifying checksum...";
#endif
    if (!QtDownloader::verifyFileChecksum(
          update->installer.absoluteFilePath(), update->json.checksum, update->json.checksumType)) {
      raiseError("Checksum is invalid");
      return;
    } else {
#if UPDATER_ENABLE_DEBUG
      qCDebug(CATEGORY_UPDATER) << "Checksum is valid";
#endif
    }
  }

  // For the tests, we don't stop the application.
  if (dry) {
    _impl->setState(Idle);
    emit installationFinished();
    return;
  }

  // Start installer in a separate process.
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Starting installer...";
#endif
  auto installerProcessSuccess = false;
#if defined(Q_OS_WIN)
  installerProcessSuccess = QProcess::startDetached(update->installer.absoluteFilePath(), {});
#elif defined(Q_OS_MAC)
  installerProcessSuccess = QProcess::startDetached("open", { update->installer.absoluteFilePath() });
#else
  raiseError("OS not supported");
#endif
  if (!installerProcessSuccess) {
    raiseError("Failed to start uninstaller");
    _impl->setState(Idle);
    return;
  }
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "Installer started";
#endif

  // Quit the app.
#if UPDATER_ENABLE_DEBUG
  qCDebug(CATEGORY_UPDATER) << "App will quit to let the installer do the update";
#endif
  QCoreApplication::quit();

  emit installationFinished();
}

#pragma endregion
} // namespace oclero

#if defined UPDATER_ENABLE_DEBUG
#  undef UPDATER_ENABLE_DEBUG
#endif
