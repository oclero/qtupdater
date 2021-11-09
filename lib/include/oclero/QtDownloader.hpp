#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QByteArray>

#include <functional>
#include <memory>

namespace oclero {
/**
 * @brief Utility class to download a file or a data buffer.
 */
class QtDownloader : QObject {
  Q_OBJECT

public:
  enum class ErrorCode {
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
  };
  Q_ENUM(ErrorCode)

  enum class ChecksumType {
    NoChecksum,
    MD5,
    SHA1,
  };
  Q_ENUM(ChecksumType)

  enum class InvalidChecksumBehavior {
    RemoveFile,
    KeepFile,
  };
  Q_ENUM(InvalidChecksumBehavior)

  using FileFinishedCallback = std::function<void(ErrorCode const, const QString&)>;
  using DataFinishedCallback = std::function<void(ErrorCode const, const QByteArray &)>;
  using ProgressCallback = std::function<void(int const)>;

public:
  QtDownloader(QObject* parent = nullptr);
  ~QtDownloader();

  void downloadFile(const QUrl& url, const QString& localDir, const FileFinishedCallback&& onFinished,
    const ProgressCallback&& onProgress = nullptr);

  void downloadData(
    const QUrl& url, const DataFinishedCallback&& onFinished, const ProgressCallback&& onProgress = nullptr);

  static bool verifyFileChecksum(const QString& filePath, const QString& checksum, ChecksumType const checksumType,
    InvalidChecksumBehavior const behavior = InvalidChecksumBehavior::RemoveFile);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
} // namespace oclero
