#include "QtUpdateWidget.hpp"

#include <oclero/QtUpdateController.hpp>

#include <QAction>
#include <QBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QProgressBar>
#include <QStyle>
#include <QApplication>
#include <QIcon>
#include <QDateTime>
#include <QTextEdit>

namespace oclero {
namespace {
QString toBold(const QString& str) {
  return QString("<b>%1</b>").arg(str);
}

class StartPage : public QWidget {
public:
  StartPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    _title = new QLabel(this);
    _title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(_title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    _title->setFont(titleFont);
    layout->addWidget(_title, 0, Qt::AlignTop);
    updateTitle(window()->windowTitle());
    QObject::connect(this->window(), &QWidget::windowTitleChanged, this, [this](const QString& text) {
      updateTitle(text);
    });

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("This wizard will guide you during the update."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Apply);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::Cancel);
    cancelBtn->setAutoDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    auto* checkForUpdateBtn = btnBox->button(QDialogButtonBox::StandardButton::Apply);
    checkForUpdateBtn->setText(tr("Check For Updates"));
    checkForUpdateBtn->setDefault(true);
    QObject::connect(checkForUpdateBtn, &QPushButton::clicked, this, [this, &controller]() {
      emit controller.checkForUpdates();
    });
  }

private:
  void updateTitle(const QString& text) {
    if (text.isEmpty()) {
      _title->setText(toBold(tr("Update")));
    } else {
      _title->setText(toBold(text));
    }
  }

  QLabel* _title;
};

class CheckingPage : public QWidget {
public:
  CheckingPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Checking For Updates")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Please wait while the application is looking for update…"));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::Fixed));

    // ProgressBar.
    auto* progressBar = new QProgressBar(this);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setRange(0, 0);
    progressBar->setTextVisible(false);
    layout->addWidget(progressBar, 0);

    // Space.
    layout->addItem(new QSpacerItem(0, space * 3, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
    layout->setStretch(2, 1);

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::Cancel);
    cancelBtn->setAutoDefault(false);
    cancelBtn->setDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });
  }
};

class CheckingFailPage : public QWidget {
public:
  CheckingFailPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Checking for Updates Failed")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Sorry, an error happened. We can't find updates."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Error code.
    auto* errorLabel = new QLabel(this);
    errorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(errorLabel, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* okBtn = btnBox->button(QDialogButtonBox::StandardButton::Ok);
    okBtn->setDefault(true);
    QObject::connect(okBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(
      &controller, &QtUpdateController::checkForUpdateErrorChanged, this, [errorLabel](QtUpdater::ErrorCode error) {
        const auto errorStr = QString::number(static_cast<int>(error));
        const auto text = tr("Error code: %1").arg(errorStr);
        errorLabel->setText(text);
      });
  }
};

class CheckingUpToDatePage : public QWidget {
public:
  CheckingUpToDatePage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("You are up-to-date!")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("You are running the latest version of the application."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* okBtn = btnBox->button(QDialogButtonBox::StandardButton::Ok);
    okBtn->setDefault(true);
    QObject::connect(okBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });
  }
};

class CheckingSuccessPage : public QWidget {
public:
  CheckingSuccessPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent)
    , _controller(controller) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("A New Version is Available!")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    _label = new QLabel(this);
    _label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _label->setWordWrap(true);
    layout->addWidget(_label, 0, Qt::AlignTop);
    updateLabelText();

    // Changelog.
    auto* textEdit = new QTextEdit(this);
    textEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    textEdit->setTextInteractionFlags(Qt::TextInteractionFlag::TextBrowserInteraction);
    textEdit->setTabStopDistance(0);
    layout->addWidget(textEdit, 1);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::Fixed));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Apply | QDialogButtonBox::StandardButton::Cancel);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* downloadButton = btnBox->button(QDialogButtonBox::StandardButton::Apply);
    downloadButton->setDefault(true);
    downloadButton->setText(tr("Download"));
    QObject::connect(downloadButton, &QPushButton::clicked, this, [this, &controller]() {
      controller.downloadUpdate();
    });
    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::Cancel);
    cancelBtn->setAutoDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(&controller, &QtUpdateController::latestVersionChanged, this, [this]() {
      updateLabelText();
    });
    QObject::connect(&controller, &QtUpdateController::latestVersionDateChanged, this, [this]() {
      updateLabelText();
    });
    QObject::connect(&controller, &QtUpdateController::latestVersionChangelogChanged, this, [this, textEdit]() {
      textEdit->setMarkdown(_controller.latestVersionChangelog());
    });

    // Connections to controller.
    QObject::connect(
      &controller, &QtUpdateController::changelogDownloadErrorChanged, this, [textEdit](QtUpdater::ErrorCode error) {
        const auto firstLine = tr("Can't download changelog.");
        const auto errorStr = QString::number(static_cast<int>(error));
        const auto text = tr("Error code: %1").arg(errorStr);
        textEdit->setText(firstLine + '\n' + text);
      });
  }

private:
  void updateLabelText() {
    const auto currentVersion = _controller.currentVersion();
    const auto currentVersionDate = _controller.currentVersionDate().toString(Qt::DateFormat::DefaultLocaleShortDate);
    const auto latestVersion = _controller.latestVersion();
    const auto latestVersionDate = _controller.latestVersionDate().toString(Qt::DateFormat::DefaultLocaleShortDate);
    const auto text =
      QString("You have version <b>%1</b> (released on %2).<br/>Version <b>%3</b> is available (released on %4).")
        .arg(currentVersion)
        .arg(currentVersionDate)
        .arg(latestVersion)
        .arg(latestVersionDate);
    _label->setText(text);
  }

  QLabel* _label;
  QtUpdateController& _controller;
};

class DownloadingPage : public QWidget {
public:
  DownloadingPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Downloading the Update")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Please wait while the application is downloading the update..."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::Fixed));

    // ProgressBar.
    auto* progressBar = new QProgressBar(this);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setTextVisible(false);
    progressBar->setRange(0, 100);
    layout->addWidget(progressBar, 0);

    // Progress label.
    auto* progressLabel = new QLabel(this);
    progressLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(progressLabel, 0, Qt::AlignHCenter);

    // Space.
    layout->addItem(new QSpacerItem(0, space * 3, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
    layout->setStretch(2, 1);

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::Cancel);
    cancelBtn->setAutoDefault(false);
    cancelBtn->setDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(
      &controller, &QtUpdateController::downloadProgressChanged, this, [progressBar, progressLabel](int value) {
        progressBar->setValue(value);
        progressLabel->setText(QString("%1%").arg(value));
      });
  }
};

class DownloadingFailPage : public QWidget {
public:
  DownloadingFailPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Update Download Failed")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Sorry, an error happened. We can't download the update."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Error code.
    auto* errorLabel = new QLabel(this);
    errorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(errorLabel, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* okBtn = btnBox->button(QDialogButtonBox::StandardButton::Ok);
    okBtn->setDefault(true);
    QObject::connect(okBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(
      &controller, &QtUpdateController::updateDownloadErrorChanged, this, [errorLabel](QtUpdater::ErrorCode error) {
        const auto errorStr = QString::number(static_cast<int>(error));
        const auto text = tr("Error code: %1").arg(errorStr);
        errorLabel->setText(text);
      });
  }
};

class DownloadingSuccessPage : public QWidget {
public:
  DownloadingSuccessPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Download successful!")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Do you want to install the update?"));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Apply | QDialogButtonBox::StandardButton::No);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* installButton = btnBox->button(QDialogButtonBox::StandardButton::Apply);
    installButton->setAutoDefault(false);
    installButton->setDefault(true);
    installButton->setText(tr("Install"));
    QObject::connect(installButton, &QPushButton::clicked, this, [this, &controller]() {
      controller.installUpdate();
    });
    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::No);
    cancelBtn->setAutoDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });
  }
};

class InstallingPage : public QWidget {
public:
  InstallingPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Installation")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Please wait while the application is installing the update..."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::Fixed));

    // ProgressBar.
    auto* progressBar = new QProgressBar(this);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setRange(0, 0);
    progressBar->setTextVisible(false);
    layout->addWidget(progressBar, 0);

    // Space.
    layout->addItem(new QSpacerItem(0, space * 3, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
    layout->setStretch(2, 1);

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* cancelBtn = btnBox->button(QDialogButtonBox::StandardButton::Cancel);
    cancelBtn->setAutoDefault(false);
    cancelBtn->setDefault(false);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });
  }
};

class InstallingFailPage : public QWidget {
public:
  InstallingFailPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Installation Error")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    auto* label = new QLabel(this);
    label->setText(tr("Sorry, an error happened. We can't install the update."));
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setWordWrap(true);
    layout->addWidget(label, 0, Qt::AlignTop);

    // Error code.
    auto* errorLabel = new QLabel(this);
    errorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(errorLabel, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* okBtn = btnBox->button(QDialogButtonBox::StandardButton::Ok);
    okBtn->setDefault(true);
    QObject::connect(okBtn, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(
      &controller, &QtUpdateController::updateInstallationErrorChanged, this, [errorLabel](QtUpdater::ErrorCode error) {
        const auto errorStr = QString::number(static_cast<int>(error));
        const auto text = tr("Error code: %1").arg(errorStr);
        errorLabel->setText(text);
      });
  }
};

class InstallingSuccessPage : public QWidget {
public:
  InstallingSuccessPage(QtUpdateController& controller, QWidget* parent = nullptr)
    : QWidget(parent)
    , _controller(controller) {
    auto* layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title.
    auto* title = new QLabel(this);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto titleFont = QFont(title->font());
    titleFont.setPointSize(titleFont.pointSize() * 1.25);
    title->setFont(titleFont);
    title->setText(toBold(tr("Installation Successful!")));
    layout->addWidget(title, 0, Qt::AlignTop);

    // Text.
    _label = new QLabel(this);
    _label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _label->setWordWrap(true);
    layout->addWidget(_label, 0, Qt::AlignTop);

    // Space.
    const auto space = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);
    layout->addItem(new QSpacerItem(0, space * 2, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    btnBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    btnBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);
    layout->addWidget(btnBox, 0, Qt::AlignBottom);

    auto* okButton = btnBox->button(QDialogButtonBox::StandardButton::Ok);
    QObject::connect(okButton, &QPushButton::clicked, this, [this, &controller]() {
      controller.cancel();
    });

    // Connections to controller.
    QObject::connect(&controller, &QtUpdateController::latestVersionChanged, this, [this]() {
      const auto latestVersion = _controller.latestVersion();
      _label->setText(tr("The installation of version %1 succeeded.").arg(latestVersion));
    });
  }

private:
  void updateLabelText() {
    const auto currentVersion = _controller.currentVersion();
    const auto currentVersionDate = _controller.currentVersionDate().toString(Qt::DateFormat::DefaultLocaleShortDate);
    const auto latestVersion = _controller.latestVersion();
    const auto latestVersionDate = _controller.latestVersionDate().toString(Qt::DateFormat::DefaultLocaleShortDate);
    const auto text =
      tr("You have version <b>%1</b> (released on %2).<br/>Version <b>%3</b> is available (released on %4).")
        .arg(currentVersion)
        .arg(currentVersionDate)
        .arg(latestVersion)
        .arg(latestVersionDate);
    _label->setText(text);
  }

  QLabel* _label;
  QtUpdateController& _controller;
};
} // namespace

QtUpdateWidget::QtUpdateWidget(QtUpdateController& controller, QWidget* parent)
  : QWidget(parent)
  , _controller(controller) {
  setupUi();

  // Connections to controller.
  QObject::connect(&_controller, &QtUpdateController::stateChanged, this, [this]() {
    const auto state = _controller.state();
    const auto widget = _pages.contains(state) ? _pages.value(state) : nullptr;
    assert(widget != nullptr);
    if (widget) {
      _stackedWidget->setCurrentWidget(widget);
    }
  });

  QObject::connect(&_controller, &QtUpdateController::closeDialogRequested, this, [this]() {
    close();
  });
}

QtUpdateWidget::~QtUpdateWidget() {}

void QtUpdateWidget::setupUi() {
  auto* layout = new QHBoxLayout(this);
  setLayout(layout);

  // Icon on the left.
  const auto windowIcon = this->windowIcon();
  if (!windowIcon.isNull()) {
    const auto iconExtent = style()->pixelMetric(QStyle::PM_MessageBoxIconSize) * 2;
    const auto pixmap = windowIcon.pixmap(iconExtent);
    if (!pixmap.isNull()) {
      auto* labelContainer = new QWidget(this);
      labelContainer->setAttribute(Qt::WidgetAttribute::WA_TransparentForMouseEvents, true);
      auto* labelContainerLayout = new QVBoxLayout(labelContainer);
      labelContainer->setLayout(labelContainerLayout);
      auto* iconLabel = new QLabel(labelContainer);
      iconLabel->setFixedSize(iconExtent, iconExtent);
      iconLabel->setPixmap(pixmap);
      labelContainerLayout->addWidget(iconLabel, 0, Qt::Alignment{ Qt::AlignTop | Qt::AlignHCenter });
      layout->addWidget(labelContainer, 0, Qt::Alignment{ Qt::AlignTop | Qt::AlignHCenter });
    }
  }

  // Pages.
  _stackedWidget = new QStackedWidget(this);
  layout->addWidget(_stackedWidget);

  {
    auto* page = new StartPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::None, page);
  }
  {
    auto* page = new CheckingPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::Checking, page);
  }
  {
    auto* page = new CheckingFailPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::CheckingFail, page);
  }
  {
    auto* page = new CheckingSuccessPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::CheckingSuccess, page);
  }
  {
    auto* page = new CheckingUpToDatePage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::CheckingUpToDate, page);
  }
  {
    auto* page = new DownloadingPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::Downloading, page);
  }
  {
    auto* page = new DownloadingFailPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::DownloadingFail, page);
  }
  {
    auto* page = new DownloadingSuccessPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::DownloadingSuccess, page);
  }
  {
    auto* page = new InstallingPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::Installing, page);
  }
  {
    auto* page = new InstallingFailPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::InstallingFail, page);
  }
  {
    auto* page = new InstallingSuccessPage(_controller, _stackedWidget);
    _stackedWidget->addWidget(page);
    _pages.insert(QtUpdateController::State::InstallingSuccess, page);
  }

  // Fix the size.
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  const auto sizeHint = this->sizeHint();
  const auto w = std::max(sizeHint.width(), 450);
  const auto h = std::max(sizeHint.height(), 280);
  setFixedSize(w, h);

  // Close with Escape key.
  auto* closeAction = new QAction(this);
  closeAction->setShortcut(Qt::Key_Escape);
  closeAction->setShortcutContext(Qt::WindowShortcut);
  QObject::connect(closeAction, &QAction::triggered, this, [this]() {
    _controller.cancel();
  });
  addAction(closeAction);
}
} // namespace oclero
