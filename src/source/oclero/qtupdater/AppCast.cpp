#include "AppCast.h"

#include "utils/EnumUtils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QDir>

namespace oclero::qtupdater {
constexpr auto JSON_DATETIME_FORMAT = "dd/MM/yyyy";
constexpr auto JSON_TAG_CHECKSUM = "checksum";
constexpr auto JSON_TAG_CHECKSUM_TYPE = "checksumType";
constexpr auto JSON_TAG_DATE = "date";
constexpr auto JSON_TAG_INSTALLER_URL = "installerUrl";
constexpr auto JSON_TAG_CHANGELOG_URL = "changelogUrl";
constexpr auto JSON_TAG_VERSION = "version";

void AppCast::fromJson(const QByteArray& data, const std::function<QJsonObject(const QJsonDocument&)>& customParser) {
  const auto jsonDocument = QJsonDocument::fromJson(data);
  if (!jsonDocument.isNull()) {
    if (customParser) {
      const auto jsonObject = customParser(jsonDocument);
      fromJsonObject(jsonObject);
    } else if (jsonDocument.isObject()) {
      const auto jsonObject = jsonDocument.object();
      fromJsonObject(jsonObject);
    }
  }
}

void AppCast::fromJsonObject(const QJsonObject& jsonObject) {
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
      checksumType = utils::enumFromString<ChecksumType>(jsonObject[JSON_TAG_CHECKSUM_TYPE].toString().toUpper());
    }

    if (jsonObject.contains(JSON_TAG_DATE)) {
      date = QDateTime::fromString(jsonObject[JSON_TAG_DATE].toString(), JSON_DATETIME_FORMAT);
    }
  }
}

bool AppCast::isValid() const {
  const auto validVersionNumber = !version.isNull();
  if (!validVersionNumber)
    return false;

  const auto validInstallerUrl = installerUrl.isEmpty() || installerUrl.isValid();
  if (!validInstallerUrl)
    return false;

  const auto validChangelogUrl = changelogUrl.isEmpty() || changelogUrl.isValid();
  if (!validChangelogUrl)
    return false;

  const auto validDate = date.isValid();
  if (!validDate)
    return false;

  auto validChecksum = true;
  if (checksumType != ChecksumType::NoChecksum) {
    auto qtAlgorithm = QCryptographicHash::Md5;
    switch (checksumType) {
      case ChecksumType::MD5:
        qtAlgorithm = QCryptographicHash::Algorithm::Md5;
        break;
      case ChecksumType::SHA1:
        qtAlgorithm = QCryptographicHash::Algorithm::Sha1;
        break;
      default:
        break;
    }

    validChecksum = !checksum.isEmpty() && checksum.size() == 2 * QCryptographicHash::hashLength(qtAlgorithm);
  }
  if (!validChecksum)
    return false;

  return true;
}

QByteArray AppCast::toJSON() const {
  if (!isValid()) {
    return QByteArray();
  }

  // Create JSON object.
  QJsonObject jsonObject({
    { JSON_TAG_VERSION, version.toString() },
    { JSON_TAG_INSTALLER_URL, installerUrl.toString() },
    { JSON_TAG_CHANGELOG_URL, changelogUrl.toString() },
    { JSON_TAG_CHECKSUM, checksum.constData() },
    { JSON_TAG_CHECKSUM_TYPE, utils::enumToString(checksumType).toLower() },
    { JSON_TAG_DATE, date.toString(JSON_DATETIME_FORMAT) },
  });

  return QJsonDocument(jsonObject).toJson(QJsonDocument::JsonFormat::Compact);
}

std::optional<QString> AppCast::saveToFile(const QString& dirPath) const {
  if (!isValid()) {
    return std::nullopt;
  }

  const auto filename = QFileInfo(installerUrl.fileName()).completeBaseName();
  const auto filePath = dirPath + '/' + filename + ".json";
  QFile file(filePath);

  // Remove existing JSON file, if one.
  if (file.exists()) {
    if (!file.remove()) {
      return std::nullopt;
    }
  }

  // Create directory if not existing yet.
  QDir const dir(dirPath);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      return std::nullopt;
    }
  }

  // Write file.
  if (!file.open(QIODevice::WriteOnly)) {
    return std::nullopt;
  }

  const auto data = toJSON();
  if (data.isEmpty()) {
    return std::nullopt;
  }

  file.write(data);
  file.close();

  return filePath;
}
} // namespace oclero::qtupdater
