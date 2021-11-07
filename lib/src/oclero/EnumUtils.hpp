#pragma once

#include <QMetaEnum>
#include <QString>
#include <optional>

namespace oclero {
template<typename TEnum>
QString enumToString(const TEnum value) {
  if constexpr (!std::is_enum<TEnum>()) {
    return QString();
  }
  const auto metaEnum = QMetaEnum::fromType<TEnum>();
  return metaEnum.valueToKey(static_cast<int>(value));
}

template<typename TEnum>
std::optional<TEnum> tryGetEnumFromString(const QString& str) {
  if constexpr (!std::is_enum<TEnum>()) {
    return {};
  }

  const auto metaEnum = QMetaEnum::fromType<TEnum>();
  auto isSuccess = false;
  const auto intValue = metaEnum.keyToValue(str.toUtf8().constData(), &isSuccess);
  if (isSuccess) {
    return static_cast<TEnum>(intValue);
  } else {
    return {};
  }
}

template<typename TEnum>
TEnum enumFromString(const QString& str, const TEnum defaultValue = TEnum()) {
  if constexpr (!std::is_enum<TEnum>()) {
    return defaultValue;
  }
  return tryGetEnumFromString<TEnum>(str).value_or(defaultValue);
}

template<typename TEnum>
constexpr auto toUnderlyingType(const TEnum value) {
  return static_cast<typename std::underlying_type<TEnum>::type>(value);
}

template<typename TEnum, typename TValue>
bool isEnumValue(const TValue value) {
  if constexpr (!std::is_enum<TEnum>()) {
    return false;
  }
  const auto metaEnum = QMetaEnum::fromType<TEnum>();
  const char* const keyAsString = metaEnum.valueToKey(static_cast<int>(value));
  return keyAsString != nullptr;
}

template<typename TEnum>
std::optional<TEnum> tryGetEnumFromInt(const int value) {
  if constexpr (!std::is_enum<TEnum>()) {
    return {};
  }
  if (isEnumValue<TEnum>(value)) {
    return static_cast<TEnum>(value);
  }
  return {};
}

template<typename TEnum>
TEnum enumFromInt(const int value, const TEnum defaultValue = TEnum()) {
  if constexpr (!std::is_enum<TEnum>()) {
    return defaultValue;
  }
  return tryGetEnumFromInt<TEnum>(value).value_or(defaultValue);
}
} // namespace oclero
