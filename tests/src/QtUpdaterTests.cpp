#include "QtUpdaterTests.h"

#include <QHttpServer>
#include <QHttpServerResponse>
#include <QTcpServer>

#include <oclero/qtupdater/Updater.h>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QTest>
#include <QTimer>
#include <QThread>

using namespace oclero::qtupdater;

namespace {
constexpr auto CURRENT_VERSION = "1.0.0";
constexpr auto LATEST_VERSION = "2.0.0";
constexpr auto SERVER_PORT = 8080;
constexpr auto SERVER_HOST = "0.0.0.0";
constexpr auto SERVER_URL = "localhost";

constexpr auto CONTENT_TYPE_JSON = "application/json";
constexpr auto CONTENT_TYPE_EXE = "application/vnd.microsoft.portable-executable";
constexpr auto CONTENT_TYPE_MD = "text/markdown";

constexpr auto DUMMY_INSTALLER_DATA = "This is just dummy data to simulate an installer file";

// Dummy markdown changelog.
constexpr auto DUMMY_CHANGELOG = R"(# Changelog
## MyApp 2.0.0
### Bugfixes
- Fix bug 1
- Fix bug 1
### New Features
- New Feature 1
- New Feature 2
)";

constexpr auto APPCAST_TEMPLATE = R"({
  "version": "%1",
  "date": "%2",
  "checksum": "%3",
  "checksumType": "md5",
  "installerUrl": "%4/installer-%1.0.exe",
  "changelogUrl": "%4/changelog-%1.0.md"
})";

static const QString SERVER_URL_FOR_CLIENT = "http://" + QString(SERVER_URL) + ':' + QString::number(SERVER_PORT);

QString getInstallerChecksum(const char* data) {
  QByteArray installerData(data);
  QCryptographicHash hash(QCryptographicHash::Algorithm::Md5);
  hash.addData(installerData);
  const auto installerHash = hash.result().toHex();
  return installerHash;
}

QString getAppCastStr(const QString& version) {
  static const auto checksum = getInstallerChecksum(DUMMY_INSTALLER_DATA);
  const auto todayDate = QDate::currentDate().toString("dd/MM/yyyy");
  return QString(APPCAST_TEMPLATE).arg(version, todayDate, checksum, SERVER_URL_FOR_CLIENT);
}

// Wrapper class to simplify HTTP server setup in tests.
class TestHttpServer {
  QHttpServer _httpServer;
  bool _started = false;

public:
  TestHttpServer() = default;

  template<typename Functor>
  bool route(const QString& path, Functor&& handler) {
    _httpServer.route(path, std::forward<Functor>(handler));
    return start();
  }

  template<typename Functor>
  bool addRoute(const QString& path, Functor&& handler) {
    _httpServer.route(path, std::forward<Functor>(handler));
    return true;
  }

  bool start() {
    if (_started) {
      return true;
    }

    auto tcpServer = new QTcpServer();
    if (!tcpServer->listen(QHostAddress::LocalHost, SERVER_PORT) || !_httpServer.bind(tcpServer)) {
      delete tcpServer;
      return false;
    }

    _started = true;
    return true;
  }
};

} // namespace

void Tests::test_emptyServerUrl() {
  Updater updater("");
  updater.setCheckTimeout(1000);

  auto hasStartedChecking = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&hasStartedChecking]() {
    hasStartedChecking = true;
  });

  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&hasStartedChecking]() {
    hasStartedChecking = true;
  });

  // Start checking. The updater should immediately fail.
  updater.forceCheckForUpdate();

  QVERIFY(hasStartedChecking == false);
}

void Tests::test_invalidServerUrl() {
  Updater updater("dummyInvalidUrl");
  updater.setCheckTimeout(1000);

  auto done = false;
  auto failed = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });
  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &failed]() {
    failed = true;
    done = true;
  });

  // Start checking. The updater should immediately fail.
  updater.forceCheckForUpdate();

  // Wait for timeout.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(failed);
}

void Tests::test_validServerUrlButNoServer() {
  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  auto done = false;
  auto failed = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &failed]() {
    failed = true;
    done = true;
  });

  // Start checking. It should fail after a timeout.
  updater.forceCheckForUpdate();

  // Wait for the client to receive the error.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(failed);
}

void Tests::test_validAppcastUrl() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);


  auto done = false;
  auto error = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking.
  updater.forceCheckForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (error) {
    QFAIL("Can't download latest version JSON");
    return;
  }

  // Latest version should be the newest one.
  const auto updateAvailable = updater.updateAvailability() == UpdateAvailability::Available;
  const auto latestVersion = updater.latestVersion();
  QVERIFY(updateAvailable);
  QVERIFY(latestVersion == LATEST_VERSION);

  // A second check should not be made because a check has already been made the same day.
  done = false;
  updater.setFrequency(Frequency::EveryDay);
  updater.checkForUpdate();
  QVERIFY(!done);
}

void Tests::test_validAppcastUrlButNoServer() {
  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking.
  updater.forceCheckForUpdate();

  // Wait for the timeout.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(error);

  // Latest version should stay the current one.
  const auto updateAvailable = updater.updateAvailability() == UpdateAvailability::Available;
  const auto latestVersion = updater.latestVersion();
  QVERIFY(!updateAvailable);
  QVERIFY(latestVersion == CURRENT_VERSION);
}

void Tests::test_validAppcastUrlButNoUpdate() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(CURRENT_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Verify that these signals are called (or not) correctly.
  auto updateAvailableChanged = false;
  auto latestVersionChanged = false;
  QObject::connect(&updater, &Updater::updateAvailabilityChanged, this, [&updateAvailableChanged]() {
    // Should be always called.
    updateAvailableChanged = true;
  });
  QObject::connect(&updater, &Updater::latestVersionChanged, this, [&latestVersionChanged]() {
    // Should be called only if a (greater) new version exists.
    latestVersionChanged = true;
  });

  // Start checking.
  updater.forceCheckForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (error) {
    QFAIL("Can't download latest version JSON");
    return;
  }

  // Latest version should be the current one.
  const auto updateAvailable = updater.updateAvailability() == UpdateAvailability::Available;
  const auto latestVersion = updater.latestVersion();

  QVERIFY(!updateAvailable);
  QVERIFY(latestVersion == CURRENT_VERSION);
  QVERIFY(updateAvailableChanged);
  QVERIFY(!latestVersionChanged);
}

void Tests::test_validChangelogUrl() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }
  server.addRoute("/changelog-<arg>", [](const QString&) {
    return QHttpServerResponse("text/markdown", DUMMY_CHANGELOG);
  });

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.forceCheckForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (updater.updateAvailability() != UpdateAvailability::Available) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download changelog.
  auto downloadedChangelog = false;
  auto error = false;
  QObject::connect(&updater, &Updater::changelogDownloadFinished, this, [&downloadedChangelog]() {
    downloadedChangelog = true;
  });
  QObject::connect(&updater, &Updater::changelogDownloadFailed, this, [&downloadedChangelog, &error]() {
    error = true;
    downloadedChangelog = true;
  });
  updater.downloadChangelog();
  if (!QTest::qWaitFor(
        [&downloadedChangelog]() {
          return downloadedChangelog;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (error) {
    QFAIL("Can't download changelog");
    return;
  }

  const auto changelogAvailable = updater.changelogAvailable();
  const auto& latestChangelog = updater.latestChangelog();
  QVERIFY(changelogAvailable);
  QVERIFY(latestChangelog == DUMMY_CHANGELOG);
}

void Tests::test_invalidChangelogUrl() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.forceCheckForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (updater.updateAvailability() != UpdateAvailability::Available) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download changelog.
  auto downloadedChangelog = false;
  auto error = false;
  QObject::connect(&updater, &Updater::changelogDownloadFinished, this, [&downloadedChangelog]() {
    downloadedChangelog = true;
  });
  QObject::connect(&updater, &Updater::changelogDownloadFailed, this, [&downloadedChangelog, &error]() {
    error = true;
    downloadedChangelog = true;
  });
  updater.downloadChangelog();
  if (!QTest::qWaitFor(
        [&downloadedChangelog]() {
          return downloadedChangelog;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(error);

  const auto changelogAvailable = updater.changelogAvailable();
  QVERIFY(!changelogAvailable);

  const auto& latestChangelog = updater.latestChangelog();
  QVERIFY(latestChangelog.isEmpty());
}

void Tests::test_validInstallerUrl() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }
  server.addRoute("/installer-<arg>", [](const QString&) {
    return QHttpServerResponse("application/vnd.microsoft.portable-executable", DUMMY_INSTALLER_DATA);
  });

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.forceCheckForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (updater.updateAvailability() != UpdateAvailability::Available) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download installer.
  auto downloadFinished = false;
  auto error = false;
  QObject::connect(&updater, &Updater::installerDownloadFinished, this, [&downloadFinished]() {
    downloadFinished = true;
  });
  QObject::connect(&updater, &Updater::installerDownloadFailed, this, [&downloadFinished, &error]() {
    error = true;
    downloadFinished = true;
  });
  updater.downloadInstaller();
  if (!QTest::qWaitFor(
        [&downloadFinished]() {
          return downloadFinished;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (error) {
    QFAIL("Can't download installer");
    return;
  }

  const auto installerAvailable = updater.installerAvailable();
  QVERIFY(installerAvailable);

  // Install update (synchronous).
  auto installationFailed = false;
  auto installationFinished = false;
  QObject::connect(&updater, &Updater::installationFinished, this, [&installationFinished]() {
    installationFinished = true;
  });
  QObject::connect(&updater, &Updater::installationFailed, this, [&installationFinished, &installationFailed]() {
    installationFailed = true;
    installationFinished = true;
  });
  updater.installUpdate(/*dry*/ true);

  QVERIFY(installationFinished);
  QVERIFY(!installationFailed);
}

void Tests::test_invalidInstallerUrl() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.forceCheckForUpdate();
  while (!checked) {
    QCoreApplication::processEvents();
  }

  if (updater.updateAvailability() != UpdateAvailability::Available) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download installer.
  auto downloadFinished = false;
  auto error = false;
  QObject::connect(&updater, &Updater::installerDownloadFinished, this, [&downloadFinished]() {
    downloadFinished = true;
  });
  QObject::connect(&updater, &Updater::installerDownloadFailed, this, [&downloadFinished, &error]() {
    error = true;
    downloadFinished = true;
  });
  updater.downloadInstaller();
  while (!downloadFinished) {
    QCoreApplication::processEvents();
  }

  QVERIFY(error);

  const auto installerAvailable = updater.installerAvailable();
  QVERIFY(!installerAvailable);

  // Install update (synchronous).
  auto installationFailed = false;
  auto installationFinished = false;
  QObject::connect(&updater, &Updater::installationFailed, this, [&installationFailed, &installationFinished]() {
    installationFailed = true;
    installationFinished = true;
  });
  QObject::connect(&updater, &Updater::installationFinished, this, [&installationFinished]() {
    installationFinished = true;
  });
  updater.installUpdate(/*dry*/ true);

  QVERIFY(installationFinished);
  QVERIFY(installationFailed);
}

void Tests::test_cancel() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);

  // Check for updates.
  auto checked = false;
  auto cancelled = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  QObject::connect(&updater, &Updater::checkForUpdateCancelled, this, [&cancelled]() {
    cancelled = true;
  });
  updater.forceCheckForUpdate();
  updater.cancel();

  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(cancelled);
}

void Tests::test_customAppCastParser() {
  // Server.
  TestHttpServer server;
  if (!server.route("/", []() {
        const auto appCast = getAppCastStr(LATEST_VERSION);
        return QHttpServerResponse("application/json", appCast.toUtf8());
      })) {
    QFAIL("Can't start server");
  }

  // Configure updater.
  Updater updater(SERVER_URL_FOR_CLIENT);
  updater.setAppCastParser([](const QByteArray& data) -> AppCast {
    // Custom parser that just calls the default one.
    return AppCast::fromJson(data);
  });

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &Updater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });
  QObject::connect(&updater, &Updater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking.
  updater.forceCheckForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (error) {
    QFAIL("Can't download latest version JSON");
    return;
  }
}
