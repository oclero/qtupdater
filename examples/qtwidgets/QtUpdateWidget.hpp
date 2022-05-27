#pragma once

#include <QDialog>
#include <QMap>

#include <oclero/QtUpdateController.hpp>

class QStackedWidget;

namespace oclero {
class QtUpdateWidget : public QWidget {
public:
  QtUpdateWidget(QtUpdateController& controller, QWidget* parent = nullptr);
  ~QtUpdateWidget();

private:
  void setupUi();

private:
  QtUpdateController& _controller;
  QStackedWidget* _stackedWidget{ nullptr };
  QMap<QtUpdateController::State, QWidget*> _pages;
};
} // namespace oclero
