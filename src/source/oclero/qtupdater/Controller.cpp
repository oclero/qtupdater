#include <oclero/qtupdater/Controller.h>

#include <oclero/qtupdater/Updater.h>
#include <oclero/qtupdater/Types.h>

namespace oclero::qtupdater {
Controller::Controller(Updater& updater, QObject* parent)
  : QObject(parent)
  , _updater(updater) {
  const auto updaterState = _updater.state();
  switch (updaterState) {
    case UpdaterState::CheckingForUpdate:
      setState(State::Checking);
      break;
    case UpdaterState::DownloadingInstaller:
      setState(State::Downloading);
      break;
    case UpdaterState::InstallingUpdate:
      setState(State::Installing);
      break;
    default:
      break;
  }

  // Checking.
  QObject::connect(&_updater, &Updater::checkForUpdateForced, this, &Controller::manualCheckingRequested);
  QObject::connect(&_updater, &Updater::checkForUpdateStarted, this, [this]() {
    setState(State::Checking);
  });
  QObject::connect(&_updater, &Updater::checkForUpdateProgressChanged, this, [this](int percentage) {
    setDownloadProgress(percentage);
  });
  QObject::connect(&_updater, &Updater::checkForUpdateFailed, this, [this](UpdaterError code) {
    setState(State::CheckingFail);
    emit checkForUpdateErrorChanged(code);
  });
  QObject::connect(&_updater, &Updater::checkForUpdateFinished, this, [this]() {
    const auto availability = _updater.updateAvailability();
    switch (availability) {
      case UpdateAvailability::Available:
        _updater.downloadChangelog();
        break;
      case UpdateAvailability::UpToDate:
        setState(State::CheckingUpToDate);
        break;
      default:
        setState(State::CheckingFail);
        break;
    }
  });
  QObject::connect(&_updater, &Updater::changelogDownloadFinished, this, [this]() {
    const auto available = _updater.changelogAvailable();
    if (available) {
      setState(State::CheckingSuccess);
    }
  });
  QObject::connect(&_updater, &Updater::changelogDownloadFailed, this, [this](UpdaterError code) {
    emit changelogDownloadErrorChanged(code);
  });

  // Downloading.
  QObject::connect(&_updater, &Updater::installerDownloadStarted, this, [this]() {
    setState(State::Downloading);
  });
  QObject::connect(&_updater, &Updater::installerDownloadProgressChanged, this, [this](int percentage) {
    setDownloadProgress(percentage);
  });
  QObject::connect(&_updater, &Updater::installerDownloadFailed, this, [this](UpdaterError code) {
    setState(State::DownloadingFail);
    emit updateDownloadErrorChanged(code);
  });
  QObject::connect(&_updater, &Updater::installerDownloadFinished, this, [this]() {
    const auto available = _updater.installerAvailable();
    setState(available ? State::DownloadingSuccess : State::DownloadingFail);
  });

  // Installing.
  QObject::connect(&_updater, &Updater::installationStarted, this, [this]() {
    setState(State::Installing);
  });
  QObject::connect(&_updater, &Updater::installationFailed, this, [this](UpdaterError code) {
    setState(State::InstallingFail);
    emit updateInstallationErrorChanged(code);
  });
  QObject::connect(&_updater, &Updater::installationFinished, this, [this]() {
    setState(State::InstallingSuccess);
  });

  // Metadata.
  QObject::connect(&_updater, &Updater::latestVersionChanged, this, &Controller::latestVersionChanged);
  QObject::connect(&_updater, &Updater::latestVersionDateChanged, this, &Controller::latestVersionDateChanged);
  QObject::connect(&_updater, &Updater::latestChangelogChanged, this, &Controller::latestVersionChangelogChanged);
}

Controller::State Controller::state() const {
  return _state;
}

void Controller::setState(State state) {
  if (state != _state) {
    _state = state;
    emit stateChanged();
  }
}

void Controller::setDownloadProgress(int value) {
  if (value != _downloadProgress) {
    _downloadProgress = value;
    emit downloadProgressChanged(value);
  }
}

QString Controller::currentVersion() const {
  return _updater.currentVersion();
}

QDateTime Controller::currentVersionDate() const {
  return _updater.currentVersionDate();
}

QString Controller::latestVersion() const {
  return _updater.latestVersion();
}

QDateTime Controller::latestVersionDate() const {
  return _updater.latestVersionDate();
}

QString Controller::latestVersionChangelog() const {
  return _updater.latestChangelog();
}

int Controller::downloadProgress() const {
  return _downloadProgress;
}

void Controller::cancel() {
  _updater.cancel();
  setState(State::None);
  emit closeDialogRequested();
}

void Controller::checkForUpdate() {
  _updater.checkForUpdate();
}

void Controller::forceCheckForUpdate() {
  _updater.forceCheckForUpdate();
}

void Controller::downloadUpdate() {
  if (_updater.updateAvailability() == UpdateAvailability::Available) {
#ifdef Q_OS_LINUX
    emit linuxDownloadUpdateRequested();
    emit closeDialogRequested();
#else
    _updater.downloadInstaller();
#endif
  }
}

void Controller::installUpdate() {
  if (_updater.installerAvailable()) {
    _updater.installUpdate();
  }
}
} // namespace oclero::qtupdater
