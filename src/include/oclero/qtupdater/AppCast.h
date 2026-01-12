#pragma once

#include <oclero/qtupdater/Types.h>

#include <QVersionNumber>
#include <QUrl>
#include <QByteArray>
#include <QDateTime>

#include <optional>

namespace oclero::qtupdater {
struct AppCast {
  QVersionNumber version;
  QUrl installerUrl;
  QUrl changelogUrl;
  QByteArray checksum;
  ChecksumType checksumType{ ChecksumType::NoChecksum };
  QDateTime date;

  bool isValid() const;
  QByteArray toJSON() const;
  std::optional<QString> saveToFile(const QString& dirPath) const;

  static AppCast fromJson(const QByteArray& data);
  static AppCast fromJsonObject(const QJsonObject& jsonObject);
};
} // namespace oclero::qtupdater
