#include <oclero/QtDownloader.hpp>

#include "DeleteLaterScopedPointer.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFileInfo>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>

#include <optional>

namespace oclero {
static const QString PARTIAL_DOWNLOAD_SUFFIX = ".part";
static const int PARTIAL_DOWNLOAD_SUFFIX_LENGTH = PARTIAL_DOWNLOAD_SUFFIX.length();

struct QtDownloader::Impl {
  QtDownloader& owner;
  QNetworkAccessManager manager;
  QUrl url;
  QFileInfo fileInfo;
  QScopedPointer<QFile> fileStream{ nullptr };
  bool isDownloading{ false };
  QMetaObject::Connection progressConnection;
  QMetaObject::Connection readyReadConnection;
  QMetaObject::Connection finishedConnection;
  FileFinishedCallback onFileFinished;
  DataFinishedCallback onDataFinished;
  ProgressCallback onProgress;
  QString localDir;
  QString downloadedFilepath;
  QByteArray downloadedData;

  Impl(QtDownloader& o)
    : owner(o) {
    manager.setAutoDeleteReplies(false);
  }

  ~Impl() {
    QObject::disconnect(progressConnection);
    QObject::disconnect(readyReadConnection);
    QObject::disconnect(finishedConnection);
  }

  void startFileDownload() {
    isDownloading = true;

    // Check url validity.
    if (url.isEmpty() || !url.isValid()) {
      onFileDownloadFinished(ErrorCode::UrlIsInvalid);
      return;
    }

    // Check directory.
    if (localDir.isEmpty()) {
      onFileDownloadFinished(ErrorCode::LocalDirIsInvalid);
      return;
    }

    // Create directory if it does not exist.
    QDir dir(localDir);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        onFileDownloadFinished(ErrorCode::CannotCreateLocalDir);
        return;
      }
    }

    // Get output file name.
    const auto urlFileName = url.fileName();
    const auto finalFilePath = dir.absolutePath() + '/' + urlFileName;
    const auto partialFilePath = finalFilePath + PARTIAL_DOWNLOAD_SUFFIX;

    // Remove file if it was previously downloaded.
    QFile previousFile{ finalFilePath };
    if (previousFile.exists()) {
      if (!previousFile.remove()) {
        onFileDownloadFinished(ErrorCode::CannotRemoveFile);
        return;
      }
    }

    // Create file to write to.
    fileInfo = QFileInfo(partialFilePath);
    fileStream.reset(new QFile(partialFilePath));
    if (fileStream->exists()) {
      if (!fileStream->remove()) {
        onFileDownloadFinished(ErrorCode::CannotRemoveFile);
        return;
      }
    }

    if (!fileStream->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      onFileDownloadFinished(ErrorCode::NotAllowedToWriteFile);
      return;
    }

    auto reply = manager.get(QNetworkRequest(url));
    if (onProgress) {
      onProgress(0);
      progressConnection = QObject::connect(
        reply, &QNetworkReply::downloadProgress, &owner, [this](qint64 bytesReceived, qint64 bytesTotal) {
          onDownloadProgress(bytesReceived, bytesTotal);
        });
    }

    readyReadConnection = QObject::connect(reply, &QNetworkReply::readyRead, &owner, [this, reply]() {
      if (reply->bytesAvailable()) {
        fileStream->write(reply->readAll());
      }
    });

    finishedConnection = QObject::connect(reply, &QNetworkReply::finished, &owner, [this, reply]() {
      if (onProgress) {
        onProgress(100);
      }

      QObject::disconnect(progressConnection);
      QObject::disconnect(readyReadConnection);
      QObject::disconnect(finishedConnection);
      const auto errorCode = handleFileReply(reply);
      onFileDownloadFinished(errorCode);
    });
  }

  void startDataDownload() {
    isDownloading = true;
    downloadedData.clear();

    if (url.isEmpty() || !url.isValid()) {
      onDataDownloadFinished(ErrorCode::UrlIsInvalid);
      return;
    }

    auto reply = manager.get(QNetworkRequest(url));

    const auto error = reply->error();
    if (error != QNetworkReply::NoError) {
      onDataDownloadFinished(ErrorCode::NetworkError);
    }

    if (onProgress) {
      onProgress(0);

      progressConnection = QObject::connect(
        reply, &QNetworkReply::downloadProgress, &owner, [this](qint64 bytesReceived, qint64 bytesTotal) {
          onDownloadProgress(bytesReceived, bytesTotal);
        });
    }

    readyReadConnection = QObject::connect(reply, &QNetworkReply::readyRead, &owner, [this, reply]() {
      if (reply->bytesAvailable()) {
        downloadedData.append(reply->readAll());
      }
    });

    finishedConnection = QObject::connect(reply, &QNetworkReply::finished, &owner, [this, reply]() {
      if (onProgress) {
        onProgress(100);
      }

      QObject::disconnect(progressConnection);
      QObject::disconnect(readyReadConnection);
      QObject::disconnect(finishedConnection);
      const auto errorCode = handleDataReply(reply);
      onDataDownloadFinished(errorCode);
    });
  }

  void onFileDownloadFinished(ErrorCode const errorCode) {
    isDownloading = false;
    if (onFileFinished) {
      downloadedFilepath = errorCode != ErrorCode::NoError ? QString{} : fileInfo.absoluteFilePath();
      onFileFinished(errorCode, downloadedFilepath);
    }
  }

  void onDataDownloadFinished(ErrorCode const errorCode) {
    isDownloading = false;
    if (onDataFinished) {
      onDataFinished(errorCode, downloadedData);
    }
  }

  void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    // Arbitrary minimum size above which we consider we are actually downloading a real file (>= 1KB),
    // and not just a reply from the server.
    if (bytesTotal >= 1000) {
      const auto percentage = bytesTotal == 0 ? 0. : (bytesReceived * 100.) / bytesTotal;
      const auto percentageInt = static_cast<int>(std::round(percentage));
      if (onProgress) {
        onProgress(percentageInt);
      }
    }
  }

  ErrorCode handleFileReply(QNetworkReply* reply) {
    assert(reply);
    assert(fileStream.get());

    DeleteLaterScopedPointer<QNetworkReply> replyRAII(reply);

    const auto closeFilestream = [this, reply](bool const removeFile) {
      isDownloading = false;
      if (removeFile) {
        fileStream->remove();
      }
      fileStream.reset(nullptr);
    };

    // Network error.
    if (reply->error() != QNetworkReply::NoError) {
      closeFilestream(true);
      return ErrorCode::NetworkError;
    }

    // IO error.
    if (!fileInfo.exists()) {
      closeFilestream(true);
      return ErrorCode::FileDoesNotExistOrIsCorrupted;
    }

    // Filename should end with a certain suffix as we are still writing to disk.
    if (!fileInfo.fileName().endsWith(PARTIAL_DOWNLOAD_SUFFIX)) {
      closeFilestream(true);
      return ErrorCode::FileDoesNotEndWithSuffix;
    }

    // Rename file as it is fully downloaded
    auto actualFileName = fileInfo.absoluteFilePath().chopped(PARTIAL_DOWNLOAD_SUFFIX_LENGTH);
    fileInfo.setFile(actualFileName);
    if (!fileStream->rename(actualFileName)) {
      closeFilestream(true);
      return ErrorCode::CannotRenameFile;
    }

    // File is ready.
    closeFilestream(false);
    return ErrorCode::NoError;
  }

  ErrorCode handleDataReply(QNetworkReply* reply) {
    assert(reply);

    DeleteLaterScopedPointer<QNetworkReply> replyRAII(reply);

    isDownloading = false;
    return reply->error() != QNetworkReply::NoError ? ErrorCode::NetworkError : ErrorCode::NoError;
  }

  static std::optional<QCryptographicHash::Algorithm> getQtAlgorithm(ChecksumType const checksumType) {
    auto result = std::optional<QCryptographicHash::Algorithm>();
    switch (checksumType) {
      case ChecksumType::MD5:
        result = QCryptographicHash::Algorithm::Md5;
        break;
      case ChecksumType::SHA1:
        result = QCryptographicHash::Algorithm::Sha1;
        break;
      default:
        break;
    }
    return result;
  }
};

QtDownloader::QtDownloader(QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this)) {}

QtDownloader::~QtDownloader() = default;

void QtDownloader::downloadFile(QUrl const& url, const QString& localDir, FileFinishedCallback const&& onFinished,
  const ProgressCallback&& onProgress) {
  if (_impl->isDownloading) {
    if (onFinished) {
      onFinished(ErrorCode::AlreadyDownloading, {});
    }
    return;
  }

  _impl->url = url;
  _impl->localDir = localDir;
  _impl->onFileFinished = onFinished;
  _impl->onDataFinished = nullptr;
  _impl->onProgress = onProgress;

  _impl->startFileDownload();
}

void QtDownloader::downloadData(
  QUrl const& url, DataFinishedCallback const&& onFinished, ProgressCallback const&& onProgress) {
  if (_impl->isDownloading) {
    if (onFinished) {
      onFinished(ErrorCode::AlreadyDownloading, {});
    }
    return;
  }

  _impl->url = url;
  _impl->localDir.clear();
  _impl->onFileFinished = nullptr;
  _impl->onDataFinished = onFinished;
  _impl->onProgress = onProgress;

  _impl->startDataDownload();
}

bool QtDownloader::verifyFileChecksum(const QString& filePath, const QString& checksumStr,
  ChecksumType const checksumType, InvalidChecksumBehavior const behavior) {
  if (checksumType == ChecksumType::NoChecksum) {
    return true;
  }

  if (checksumStr.isEmpty() || filePath.isEmpty()) {
    return false;
  }

  const auto qtAlgorithm = Impl::getQtAlgorithm(checksumType);
  if (!qtAlgorithm) {
    return false;
  }

  auto result = false;
  QFile file(filePath);
  if (file.open(QFile::ReadOnly)) {
    QCryptographicHash hash(qtAlgorithm.value());
    if (hash.addData(&file)) {
      const auto fileHash = hash.result().toHex();
      const auto checksum = checksumStr.toLower().toUtf8();
      result = checksum == fileHash;
    }
  }
  file.close();

  if (!result && behavior == InvalidChecksumBehavior::RemoveFile) {
    file.remove();
  }

  return result;
}
} // namespace oclero
