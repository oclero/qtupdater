#include "FileUtils.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QCoreApplication>

namespace oclero::qtupdater::utils {
bool clearDirectoryContent(const QString& dirPath) {
  QDir dir(dirPath);
  if (!dir.exists()) {
    return true; // Nothing to clear
  }

  const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
  for (const auto& entry : entries) {
    if (entry.isDir()) {
      // Recursively remove subdirectories
      QDir subDir(entry.absoluteFilePath());
      if (!subDir.removeRecursively()) {
        return false;
      }
    } else {
      // Remove files
      if (!QFile::remove(entry.absoluteFilePath())) {
        return false;
      }
    }
  }

  return true;
}

QString getDefaultTemporaryDirectoryPath() {
  QString result;

  const auto dirs = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::TempLocation);
  if (!dirs.isEmpty()) {
    result = dirs.first();

    const auto subDirectories = {
      QCoreApplication::organizationName(),
      QCoreApplication::applicationName(),
    };

    QStringList subDirectoriesList;
    for (const auto& subDirectory : subDirectories) {
      if (!subDirectory.isEmpty()) {
        subDirectoriesList << subDirectory;
      }
    }

    result += '/' + subDirectoriesList.join('/') + "/update";
  }

  return result;
}

LazyFileContent::LazyFileContent(const QString& path)
  : _path(path) {}

void LazyFileContent::setPath(const QString& path) {
  if (path != _path) {
    _path = path;
    _content.reset();
  }
}

const QString& LazyFileContent::getContent() {
  if (!_content.has_value()) {
    if (!_path.isEmpty()) {
      QFile file(_path);
      if (file.open(QIODevice::ReadOnly)) {
        _content = QString::fromUtf8(file.readAll());
      } else {
        _content = QString{}; // Mark as read.
      }
      file.close();
    } else {
      _content = QString{}; // Mark as read.
    }
  }
  return _content.value();
}

} // namespace oclero::qtupdater::utils
