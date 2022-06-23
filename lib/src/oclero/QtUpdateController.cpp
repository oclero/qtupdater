#include <oclero/QtUpdateController.hpp>

#include <oclero/QtUpdater.hpp>

namespace oclero {
QtUpdateController::QtUpdateController(oclero::QtUpdater& updater, QObject* parent)
  : QObject(parent)
  , _updater(updater) {
  const auto updaterState = _updater.state();
  switch (updaterState) {
    case QtUpdater::State::CheckingForUpdate:
      setState(State::Checking);
      break;
    case QtUpdater::State::DownloadingInstaller:
      setState(State::Downloading);
      break;
    case QtUpdater::State::InstallingUpdate:
      setState(State::Installing);
      break;
    default:
      break;
  }

  // Checking.
  QObject::connect(&_updater, &QtUpdater::checkForUpdateForced, this, &QtUpdateController::manualCheckingRequested);
  QObject::connect(&_updater, &QtUpdater::checkForUpdateStarted, this, [this]() {
    setState(State::Checking);
  });
  QObject::connect(&_updater, &QtUpdater::checkForUpdateProgressChanged, this, [this](int percentage) {
    setDownloadProgress(percentage);
  });
  QObject::connect(&_updater, &QtUpdater::checkForUpdateFailed, this, [this](QtUpdater::ErrorCode code) {
    setState(State::CheckingFail);
    emit checkForUpdateErrorChanged(code);
  });
  QObject::connect(&_updater, &QtUpdater::checkForUpdateFinished, this, [this]() {
    const auto availability = _updater.updateAvailability();
    switch (availability) {
      case oclero::QtUpdater::UpdateAvailability::Available:
        _updater.downloadChangelog();
        break;
      case oclero::QtUpdater::UpdateAvailability::UpToDate:
        setState(State::CheckingUpToDate);
        break;
      default:
        setState(State::CheckingFail);
        break;
    }
  });
  QObject::connect(&_updater, &QtUpdater::changelogDownloadFinished, this, [this]() {
    const auto available = _updater.changelogAvailable();
    if (available) {
      setState(State::CheckingSuccess);
    }
  });
  QObject::connect(&_updater, &QtUpdater::changelogDownloadFailed, this, [this](QtUpdater::ErrorCode code) {
    emit changelogDownloadErrorChanged(code);
  });

  // Downloading.
  QObject::connect(&_updater, &QtUpdater::installerDownloadStarted, this, [this]() {
    setState(State::Downloading);
  });
  QObject::connect(&_updater, &QtUpdater::installerDownloadProgressChanged, this, [this](int percentage) {
    setDownloadProgress(percentage);
  });
  QObject::connect(&_updater, &QtUpdater::installerDownloadFailed, this, [this](QtUpdater::ErrorCode code) {
    setState(State::DownloadingFail);
    emit updateDownloadErrorChanged(code);
  });
  QObject::connect(&_updater, &QtUpdater::installerDownloadFinished, this, [this]() {
    const auto available = _updater.installerAvailable();
    setState(available ? State::DownloadingSuccess : State::DownloadingFail);
  });

  // Installing.
  QObject::connect(&_updater, &QtUpdater::installationStarted, this, [this]() {
    setState(State::Installing);
  });
  QObject::connect(&_updater, &QtUpdater::installationFailed, this, [this](QtUpdater::ErrorCode code) {
    setState(State::InstallingFail);
    emit updateInstallationErrorChanged(code);
  });
  QObject::connect(&_updater, &QtUpdater::installationFinished, this, [this]() {
    setState(State::InstallingSuccess);
  });

  // Metadata.
  QObject::connect(&_updater, &QtUpdater::latestVersionChanged, this, &QtUpdateController::latestVersionChanged);
  QObject::connect(
    &_updater, &QtUpdater::latestVersionDateChanged, this, &QtUpdateController::latestVersionDateChanged);
  QObject::connect(
    &_updater, &QtUpdater::latestChangelogChanged, this, &QtUpdateController::latestVersionChangelogChanged);
}

QtUpdateController::State QtUpdateController::state() const {
  return _state;
}

void QtUpdateController::setState(State state) {
  if (state != _state) {
    _state = state;
    emit stateChanged();
  }
}

void QtUpdateController::setDownloadProgress(int value) {
  if (value != _downloadProgress) {
    _downloadProgress = value;
    emit downloadProgressChanged(value);
  }
}

QString QtUpdateController::currentVersion() const {
  return _updater.currentVersion();
}

QDateTime QtUpdateController::currentVersionDate() const {
  return _updater.currentVersionDate();
}

QString QtUpdateController::latestVersion() const {
  return _updater.latestVersion();
}

QDateTime QtUpdateController::latestVersionDate() const {
  return _updater.latestVersionDate();
}

QString QtUpdateController::latestVersionChangelog() const {
  return _updater.latestChangelog();
}

int QtUpdateController::downloadProgress() const {
  return _downloadProgress;
}

void QtUpdateController::cancel() {
  _updater.cancel();
  setState(State::None);
  emit closeDialogRequested();
}

void QtUpdateController::checkForUpdate() {
  _updater.checkForUpdate();
}

void QtUpdateController::forceCheckForUpdate() {
  _updater.forceCheckForUpdate();
}

void QtUpdateController::downloadUpdate() {
  if (_updater.updateAvailability() == oclero::QtUpdater::UpdateAvailability::Available) {
#ifdef Q_OS_LINUX
    emit linuxDownloadUpdateRequested();
    emit closeDialogRequested();
#else
    _updater.downloadInstaller();
#endif
  }
}

void QtUpdateController::installUpdate() {
  if (_updater.installerAvailable()) {
    _updater.installUpdate(oclero::QtUpdater::InstallMode::ExecuteFile);
  }
}
} // namespace oclero
