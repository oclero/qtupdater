#pragma once

#include "EnumUtils.hpp"

#include <QSettings>
#include <QVariant>

namespace oclero {
template<typename TValue>
std::optional<TValue> tryGetVariantValue(const QVariant& variant) {
  if (!variant.isValid())
    return {};

  if constexpr (std::is_same<TValue, int>::value) {
    const auto variantType = variant.type();
    const auto validType = variantType == QVariant::Type::Int || variantType == QVariant::Type::LongLong
                           || variantType == QVariant::Type::UInt || variantType == QVariant::Type::ULongLong;
    return validType ? variant.toInt() : std::optional<TValue>();
  } else if constexpr (std::is_same<TValue, double>::value) {
    const auto variantType = variant.type();
    const auto validType = variantType == QVariant::Type::Int || variantType == QVariant::Type::LongLong
                           || variantType == QVariant::Type::UInt || variantType == QVariant::Type::ULongLong
                           || variantType == QVariant::Type::Double;
    return validType ? variant.toDouble() : std::optional<TValue>();
  } else if constexpr (std::is_same<TValue, QString>::value) {
    return variant.type() == QVariant::Type::String ? variant.toString() : std::optional<TValue>();
  } else if (variant.canConvert<TValue>()) {
    return variant.value<TValue>();
  } else {
    return {};
  }
}

template<typename TValue>
std::optional<TValue> tryLoadSetting(const char* key) {
  QSettings settings;
  if constexpr (std::is_enum<TValue>()) {
    const auto variant_str = settings.value(key).toString();
    return tryGetEnumFromString<TValue>(variant_str);
  } else {
    const auto variant = settings.value(key);
    return tryGetVariantValue<TValue>(variant);
  }
}

template<typename TValue>
std::optional<TValue> tryLoadSetting(const QString& key) {
  const auto byteArray = key.toUtf8();
  const char* rawKey = byteArray.constData();
  return tryLoadSetting<TValue>(rawKey);
}

template<typename TValue>
TValue loadSetting(const char* key, const TValue& defaultValue = TValue()) {
  return tryLoadSetting<TValue>(key).value_or(defaultValue);
}

template<typename TValue>
TValue loadSetting(const QString& key, const TValue& defaultValue = TValue()) {
  const auto byteArray = key.toUtf8();
  const char* rawKey = byteArray.constData();
  return loadSetting<TValue>(rawKey, defaultValue);
}

template<typename TValue>
void saveSetting(const char* key, const TValue& value) {
  QSettings settings;
  if constexpr (std::is_enum<TValue>()) {
    settings.setValue(key, QVariant::fromValue<QString>(enumToString(value)));
  } else {
    settings.setValue(key, value);
  }
  settings.sync();
}

template<typename TValue>
void saveSetting(const QString& key, const TValue& value) {
  const auto byteArray = key.toUtf8();
  const char* rawKey = byteArray.constData();
  saveSetting<TValue>(rawKey, value);
}

void clearSetting(const char* key);

void clearSetting(const QString& key);
} // namespace oclero
