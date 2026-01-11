#pragma once

#include <QDialog>
#include <QHash>

#include <oclero/qtupdater/Controller.h>

class QStackedWidget;

namespace oclero::qtupdater {
class QtUpdateWidget : public QWidget {
public:
  explicit QtUpdateWidget(qtupdater::Controller& controller, QWidget* parent = nullptr);
  ~QtUpdateWidget() override;

private:
  void setupUi();

private:
  qtupdater::Controller& _controller;
  QStackedWidget* _stackedWidget{ nullptr };
  QHash<qtupdater::Controller::State, QWidget*> _pages;
};
} // namespace oclero::qtupdater
