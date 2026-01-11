#pragma once

#include <oclero/qtupdater/Types.h>

#include <QVersionNumber>
#include <QUrl>
#include <QByteArray>
#include <QDateTime>

#include <optional>
#include <functional>

namespace oclero::qtupdater {
struct AppCast {
  QVersionNumber version;
  QUrl installerUrl;
  QUrl changelogUrl;
  QByteArray checksum;
  ChecksumType checksumType{ ChecksumType::NoChecksum };
  QDateTime date;

  void fromJson(const QByteArray& data, const std::function<QJsonObject(const QJsonDocument&)>& customParser = nullptr);
  void fromJsonObject(const QJsonObject& jsonObject);

  bool isValid() const;
  QByteArray toJSON() const;
  std::optional<QString> saveToFile(const QString& dirPath) const;
};
} // namespace oclero::qtupdater
