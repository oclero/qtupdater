#pragma once

#include <QSettings>
#include <QVariant>

#include <optional>

namespace oclero::qtupdater::utils {
/**
 * @brief Loads a setting value from QSettings.
 * @tparam T The type to convert the setting value to.
 * @param settings The QSettings instance to read from.
 * @param key The setting key to read.
 * @return The setting value converted to type T, or a default-constructed T if not found.
 */
template<typename T>
T loadSetting(QSettings& settings, const QString& key) {
  const auto variant = settings.value(key);
  if (variant.isValid()) {
    return variant.value<T>();
  }
  return T{};
}

/**
 * @brief Tries to load a setting value from QSettings.
 * @tparam T The type to convert the setting value to.
 * @param settings The QSettings instance to read from.
 * @param key The setting key to read.
 * @return An optional containing the value if found and valid, otherwise std::nullopt.
 */
template<typename T>
std::optional<T> tryLoadSetting(QSettings& settings, const QString& key) {
  if (!settings.contains(key)) {
    return std::nullopt;
  }

  const auto variant = settings.value(key);
  if (!variant.isValid() || !variant.canConvert<T>()) {
    return std::nullopt;
  }

  return variant.value<T>();
}

/**
 * @brief Saves a setting value to QSettings.
 * @tparam T The type of the value to save.
 * @param settings The QSettings instance to write to.
 * @param key The setting key to write.
 * @param value The value to save.
 */
template<typename T>
void saveSetting(QSettings& settings, const QString& key, const T& value) {
  settings.setValue(key, QVariant::fromValue(value));
}

} // namespace oclero::qtupdater::utils
