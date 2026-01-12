#pragma once

#include <QString>
#include <QMetaEnum>

namespace oclero::qtupdater::utils {
/**
 * @brief Converts a string to an enum value using Qt's meta-object system.
 * @tparam T The enum type (must be registered with Q_ENUM).
 * @param str The string representation of the enum value.
 * @return The enum value, or a default-constructed value if conversion fails.
 */
template<typename T>
T enumFromString(const QString& str) {
  const auto metaEnum = QMetaEnum::fromType<T>();
  auto ok = false;
  const auto value = metaEnum.keyToValue(str.toUtf8().constData(), &ok);
  return ok ? static_cast<T>(value) : T{};
}

/**
 * @brief Converts an enum value to a string using Qt's meta-object system.
 * @tparam T The enum type (must be registered with Q_ENUM).
 * @param value The enum value to convert.
 * @return The string representation of the enum value.
 */
template<typename T>
QString enumToString(T value) {
  const auto metaEnum = QMetaEnum::fromType<T>();
  return QString::fromUtf8(metaEnum.valueToKey(static_cast<int>(value)));
}
} // namespace oclero::qtupdater::utils
