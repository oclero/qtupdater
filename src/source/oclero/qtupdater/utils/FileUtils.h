#pragma once

#include <QString>
#include <optional>

namespace oclero::qtupdater::utils {
/**
 * @brief Clears all content from a directory without removing the directory itself.
 * @param dirPath The path to the directory to clear.
 * @return true if successful, false otherwise.
 */
bool clearDirectoryContent(const QString& dirPath);

/**
 * @brief Gets the default temporary directory path for the application.
 * @return The default temporary directory path.
 */
QString getDefaultTemporaryDirectoryPath();

/**
 * @brief Lazy loader for file content.
 * Reads file content only when requested and caches it.
 */
class LazyFileContent {
public:
  LazyFileContent(const QString& path = {});

  void setPath(const QString& path);
  const QString& getContent();

private:
  QString _path;
  std::optional<QString> _content;
};
} // namespace oclero::qtupdater::utils
