#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QMetaEnum>

namespace oclero::qtupdater {
Q_NAMESPACE

// Updater state.
enum class UpdaterState {
  Idle,
  CheckingForUpdate,
  DownloadingChangelog,
  DownloadingInstaller,
  InstallingUpdate,
};
Q_ENUM_NS(UpdaterState)

// Update availability status.
enum class UpdateAvailability {
  Unknown,
  UpToDate,
  Available,
};
Q_ENUM_NS(UpdateAvailability)

// Update check frequency.
enum class Frequency {
  Never,
  EveryStart,
  EveryHour,
  EveryDay,
  EveryWeek,
  EveryTwoWeeks,
  EveryMonth,
};
Q_ENUM_NS(Frequency)

// Installation mode.
enum class InstallMode {
  ExecuteFile,
  MoveFileToDir,
};
Q_ENUM_NS(InstallMode)

// Updater error codes.
enum class UpdaterError {
  NoError,
  UrlError,
  NetworkError,
  DiskError,
  ChecksumError,
  InstallerExecutionError,
  UnknownError,
};
Q_ENUM_NS(UpdaterError)

// Downloader error codes.
enum class DownloaderError {
  NoError,
  AlreadyDownloading,
  UrlIsInvalid,
  LocalDirIsInvalid,
  CannotCreateLocalDir,
  CannotRemoveFile,
  NotAllowedToWriteFile,
  NetworkError,
  FileDoesNotExistOrIsCorrupted,
  FileDoesNotEndWithSuffix,
  CannotRenameFile,
  Cancelled,
};
Q_ENUM_NS(DownloaderError)

// Checksum algorithm type.
enum class ChecksumType {
  NoChecksum,
  MD5,
  SHA1,
};
Q_ENUM_NS(ChecksumType)

// Behavior when checksum is invalid.
enum class InvalidChecksumBehavior {
  RemoveFile,
  KeepFile,
};
Q_ENUM_NS(InvalidChecksumBehavior)

// Information needed to setup QSettings for Updater.
struct SettingsParameters {
  QSettings::Format format;
  QSettings::Scope scope;
  QString organization;
  QString application;
};
} // namespace oclero::qtupdater
