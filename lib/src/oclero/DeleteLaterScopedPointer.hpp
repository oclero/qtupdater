#pragma once

#include <QObject>
#include <memory>

namespace oclero {
struct DeleteLater {
  void operator()(QObject* o) const {
    o->deleteLater();
  }
};

template<typename TQObject>
using DeleteLaterScopedPointer = std::unique_ptr<TQObject, DeleteLater>;
} // namespace oclero
