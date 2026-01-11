#include <oclero/qtupdater/Downloader.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFileInfo>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QByteArray>
#include <QEventLoop>

#include <optional>
#include <cmath>

namespace oclero::qtupdater {
static const QString PARTIAL_DOWNLOAD_SUFFIX = ".part";
static const int PARTIAL_DOWNLOAD_SUFFIX_LENGTH = static_cast<int>(PARTIAL_DOWNLOAD_SUFFIX.length());

struct Downloader::Impl {
  Downloader& owner;
  QNetworkAccessManager manager;
  QUrl url;
  QFileInfo fileInfo;
  QScopedPointer<QFile> fileStream{ nullptr };
  bool isDownloading{ false };
  bool cancelled{ false };
  QPointer<QNetworkReply> reply{ nullptr };
  QMetaObject::Connection progressConnection;
  QMetaObject::Connection readyReadConnection;
  QMetaObject::Connection finishedConnection;
  FileFinishedCallback onFileFinished;
  DataFinishedCallback onDataFinished;
  ProgressCallback onProgress;
  QString localDir;
  QString downloadedFilepath;
  QByteArray downloadedData;
  int timeout{ DefaultTimeout };

  Impl(Downloader& o)
    : owner(o) {
    manager.setAutoDeleteReplies(false);
  }

  ~Impl() {
    // Clear callbacks FIRST before any cleanup
    onFileFinished = nullptr;
    onDataFinished = nullptr;
    onProgress = nullptr;

    // Disconnect our specific connections
    QObject::disconnect(progressConnection);
    QObject::disconnect(readyReadConnection);
    QObject::disconnect(finishedConnection);
  }

  void startFileDownload() {
    // Disconnect any previous connections to avoid multiple lambdas executing
    QObject::disconnect(progressConnection);
    QObject::disconnect(readyReadConnection);
    QObject::disconnect(finishedConnection);

    isDownloading = true;

    // Check url validity.
    if (url.isEmpty() || !url.isValid()) {
      onFileDownloadFinished(DownloaderError::UrlIsInvalid);
      return;
    }

    // Check directory.
    if (localDir.isEmpty()) {
      onFileDownloadFinished(DownloaderError::LocalDirIsInvalid);
      return;
    }

    // Create directory if it does not exist.
    QDir dir(localDir);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        onFileDownloadFinished(DownloaderError::CannotCreateLocalDir);
        return;
      }
    }

    // Get output file name.
    const auto urlFileName = url.fileName();
    const auto finalFilePath = dir.absolutePath() + '/' + urlFileName;
    const auto partialFilePath = finalFilePath + PARTIAL_DOWNLOAD_SUFFIX;

    // Remove file if it was previously downloaded.
    for (const auto& previousPath : { partialFilePath, finalFilePath }) {
      QFile previousFile{ previousPath };
      if (previousFile.exists()) {
        if (!previousFile.remove()) {
          onFileDownloadFinished(DownloaderError::CannotRemoveFile);
          return;
        }
      }
    }

    // Create file to write to.
    fileInfo = QFileInfo(partialFilePath);
    fileStream.reset(new QFile(partialFilePath));
    if (fileStream->exists()) {
      if (!fileStream->remove()) {
        onFileDownloadFinished(DownloaderError::CannotRemoveFile);
        return;
      }
    }

    if (!fileStream->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      onFileDownloadFinished(DownloaderError::NotAllowedToWriteFile);
      return;
    }

    auto request = QNetworkRequest(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(timeout);
    reply = manager.get(request);
    if (onProgress) {
      onProgress(0);
      progressConnection = QObject::connect(
        reply, &QNetworkReply::downloadProgress, &owner, [this](qint64 bytesReceived, qint64 bytesTotal) {
          if (bytesTotal >= bytesReceived) {
            onDownloadProgress(bytesReceived, bytesTotal);
          }
        });
    }

    readyReadConnection = QObject::connect(reply, &QNetworkReply::readyRead, &owner, [this]() {
      if (reply->bytesAvailable()) {
        fileStream->write(reply->readAll());
      }
    });

    finishedConnection = QObject::connect(reply, &QNetworkReply::finished, &owner, [this]() {
      if (onProgress) {
        onProgress(100);
      }

      QObject::disconnect(progressConnection);
      QObject::disconnect(readyReadConnection);
      QObject::disconnect(finishedConnection);
      const auto errorCode = handleFileReply(reply, cancelled);
      onFileDownloadFinished(errorCode);
    });
  }

  void startDataDownload() {
    // Disconnect any previous connections to avoid multiple lambdas executing
    QObject::disconnect(progressConnection);
    QObject::disconnect(readyReadConnection);
    QObject::disconnect(finishedConnection);

    isDownloading = true;
    downloadedData.clear();

    const auto scheme = url.scheme();
    if (url.isEmpty() || !url.isValid() || scheme.isEmpty()) {
      onDataDownloadFinished(DownloaderError::UrlIsInvalid);
      return;
    }

    // Create request.
    auto request = QNetworkRequest(url);
    request.setTransferTimeout(timeout);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);

    // Do GET request.
    reply = manager.get(request);

    const auto error = reply->error();
    if (error != QNetworkReply::NoError) {
      onDataDownloadFinished(DownloaderError::NetworkError);
      return;
    }

    if (onProgress) {
      onProgress(0);

      progressConnection = QObject::connect(
        reply, &QNetworkReply::downloadProgress, &owner,
        [this](qint64 bytesReceived, qint64 bytesTotal) {
          onDownloadProgress(bytesReceived, bytesTotal);
        },
        Qt::QueuedConnection);
    }

    readyReadConnection = QObject::connect(
      reply, &QNetworkReply::readyRead, &owner,
      [this]() {
        if (reply->bytesAvailable()) {
          downloadedData.append(reply->readAll());
        }
      },
      Qt::QueuedConnection);

    finishedConnection = QObject::connect(
      reply, &QNetworkReply::finished, &owner,
      [this]() {
        if (onProgress) {
          onProgress(100);
        }

        QObject::disconnect(progressConnection);
        QObject::disconnect(readyReadConnection);
        QObject::disconnect(finishedConnection);
        const auto errorCode = handleDataReply(reply, cancelled);
        onDataDownloadFinished(errorCode);
      },
      Qt::QueuedConnection);
  }

  void onFileDownloadFinished(DownloaderError const errorCode) {
    isDownloading = false;
    cancelled = false;
    if (onFileFinished) {
      downloadedFilepath = errorCode != DownloaderError::NoError ? QString{} : fileInfo.absoluteFilePath();
      onFileFinished(errorCode, downloadedFilepath);
    }
  }

  void onDataDownloadFinished(DownloaderError const errorCode) {
    isDownloading = false;
    cancelled = false;
    if (onDataFinished) {
      onDataFinished(errorCode, downloadedData);
    }
  }

  void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    // Arbitrary minimum size above which we consider we are actually downloading a real file (>= 1KB),
    // and not just a reply from the server.
    if (bytesTotal >= 1024) {
      const auto percentage = bytesTotal == 0 ? 0. : (bytesReceived * 100.) / bytesTotal;
      const auto percentageInt = static_cast<int>(std::round(percentage));
      if (onProgress) {
        onProgress(percentageInt);
      }
    }
  }

  // RAII guard to handle file stream cleanup.
  struct FileStreamGuard {
    Impl* impl{ nullptr };
    bool removeFile{ true };

    FileStreamGuard(Impl* impl)
      : impl(impl) {}

    ~FileStreamGuard() {
      impl->isDownloading = false;
      if (removeFile && impl->fileStream) {
        impl->fileStream->remove();
      }
      impl->fileStream.reset(nullptr);
    }
  };

  DownloaderError handleFileReply(QNetworkReply* reply, bool cancelled) {
    assert(reply);
    assert(fileStream.get());

    if (!reply)
      return DownloaderError::NetworkError;

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> replyRAII(reply);

    FileStreamGuard fileStreamGuard(this);

    // Cancelled by user.
    if (cancelled) {
      return DownloaderError::Cancelled;
    }

    // Network error.
    if (reply->error() != QNetworkReply::NoError) {
      return DownloaderError::NetworkError;
    }

    // IO error.
    if (!fileInfo.exists()) {
      return DownloaderError::FileDoesNotExistOrIsCorrupted;
    }

    // Filename should end with a certain suffix as we are still writing to disk.
    if (!fileInfo.fileName().endsWith(PARTIAL_DOWNLOAD_SUFFIX)) {
      return DownloaderError::FileDoesNotEndWithSuffix;
    }

    // Rename file as it is fully downloaded
    auto actualFileName = fileInfo.absoluteFilePath().chopped(PARTIAL_DOWNLOAD_SUFFIX_LENGTH);
    fileInfo.setFile(actualFileName);
    if (!fileStream->rename(actualFileName)) {
      return DownloaderError::CannotRenameFile;
    }

    // File is ready.
    fileStreamGuard.removeFile = false;
    return DownloaderError::NoError;
  }

  DownloaderError handleDataReply(QNetworkReply* reply, bool cancelled) {
    assert(reply);

    if (!reply)
      return DownloaderError::NetworkError;

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> replyRAII(reply);

    // Cancelled by user.
    if (cancelled) {
      return DownloaderError::Cancelled;
    }

    const auto reply_error = reply->error();
    return reply_error != QNetworkReply::NoError ? DownloaderError::NetworkError : DownloaderError::NoError;
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

Downloader::Downloader(QObject* parent)
  : QObject(parent)
  , _impl(new Impl(*this)) {}

Downloader::~Downloader() {}

void Downloader::downloadFile(const QUrl& url, const QString& localDir, const FileFinishedCallback&& onFinished,
  const ProgressCallback&& onProgress, const int timeout) {
  if (_impl->isDownloading) {
    if (onFinished) {
      onFinished(DownloaderError::AlreadyDownloading, {});
    }
    return;
  }

  _impl->url = url;
  _impl->localDir = localDir;
  _impl->onFileFinished = onFinished;
  _impl->onDataFinished = nullptr;
  _impl->onProgress = onProgress;
  _impl->timeout = timeout;
  _impl->reply.clear();
  _impl->cancelled = false;

  _impl->startFileDownload();
}

void Downloader::downloadData(
  const QUrl& url, const DataFinishedCallback&& onFinished, const ProgressCallback&& onProgress, const int timeout) {
  if (_impl->isDownloading) {
    if (onFinished) {
      onFinished(DownloaderError::AlreadyDownloading, {});
    }
    return;
  }

  _impl->url = url;
  _impl->localDir.clear();
  _impl->onFileFinished = nullptr;
  _impl->onDataFinished = onFinished;
  _impl->onProgress = onProgress;
  _impl->timeout = timeout;
  if (_impl->reply) {
    _impl->reply->deleteLater();
  }
  _impl->reply.clear();
  _impl->cancelled = false;

  _impl->startDataDownload();
}

void Downloader::cancel() {
  if (isDownloading()) {
    _impl->cancelled = true;

    if (_impl->reply) {
      // Finished signal will be emitted, and the reply will be deleted at this moment.
      _impl->reply->abort();
    }
  }
}

bool Downloader::isDownloading() const {
  return _impl->isDownloading;
}

bool Downloader::verifyFileChecksum(const QString& filePath, const QString& checksumStr,
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
} // namespace oclero::qtupdater
