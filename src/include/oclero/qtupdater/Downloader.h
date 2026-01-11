#pragma once

#include <oclero/qtupdater/Types.h>

#include <QObject>

#include <functional>
#include <memory>

class QByteArray;
class QString;
class QUrl;

namespace oclero::qtupdater {
/**
 * @brief Utility class to download a file or a data buffer.
 */
class Downloader : public QObject {
  Q_OBJECT

public:
  using FileFinishedCallback = std::function<void(DownloaderError const, const QString&)>;
  using DataFinishedCallback = std::function<void(DownloaderError const, const QByteArray&)>;
  using ProgressCallback = std::function<void(int const)>;

  static inline const int DefaultTimeout = 10000;

public:
  explicit Downloader(QObject* parent = nullptr);
  ~Downloader() override;

  void downloadFile(const QUrl& url, const QString& localDir, const FileFinishedCallback&& onFinished,
    const ProgressCallback&& onProgress = nullptr, const int timeout = DefaultTimeout);

  void downloadData(const QUrl& url, const DataFinishedCallback&& onFinished,
    const ProgressCallback&& onProgress = nullptr, const int timeout = DefaultTimeout);

  void cancel();

  bool isDownloading() const;

  static bool verifyFileChecksum(const QString& filePath, const QString& checksum, ChecksumType const checksumType,
    InvalidChecksumBehavior const behavior = InvalidChecksumBehavior::RemoveFile);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
} // namespace oclero::qtupdater
