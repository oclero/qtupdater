#include "QtUpdaterTests.hpp"

#include <httplib.h>
#include <oclero/QtUpdater.hpp>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QTest>

#include <thread>

using namespace oclero;

namespace {
constexpr auto CURRENT_VERSION = "1.0.0";
constexpr auto LATEST_VERSION = "2.0.0";
constexpr auto SERVER_PORT = 8080;
constexpr auto SERVER_HOST = "0.0.0.0";
constexpr auto SERVER_URL = "localhost";

constexpr auto APPCAST_QUERY_REGEX = R"(\/)";
constexpr auto INSTALLER_QUERY_REGEX = R"(\/installer-.+\.(exe|dmg)?)";
constexpr auto CHANGELOG_QUERY_REGEX = R"(\/changelog-.+\.md?)";

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

QString getAppCast(const QString& version) {
  static const auto checksum = getInstallerChecksum(DUMMY_INSTALLER_DATA);
  const auto todayDate = QDate::currentDate().toString("dd/MM/yyyy");
  return QString(APPCAST_TEMPLATE).arg(version).arg(todayDate).arg(checksum).arg(SERVER_URL_FOR_CLIENT);
}
} // namespace

void Tests::test_emptyServerUrl() {
  QtUpdater updater("");
  updater.setFrequency(QtUpdater::Frequency::Never);

  auto hasStartedChecking = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&hasStartedChecking]() {
    hasStartedChecking = true;
  });

  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&hasStartedChecking]() {
    hasStartedChecking = true;
  });

  // Start checking. The updater should immediately fail.
  updater.checkForUpdate();

  QVERIFY(hasStartedChecking == false);
}

void Tests::test_invalidServerUrl() {
  QtUpdater updater("dummyInvalidUrl");
  updater.setFrequency(QtUpdater::Frequency::Never);

  auto done = false;
  auto failed = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });
  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&done, &failed]() {
    failed = true;
    done = true;
  });

  // Start checking. The updater should immediately fail.
  updater.checkForUpdate();

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
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking. It should fail after a timeout.
  updater.checkForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  QVERIFY(error);
}

void Tests::test_validAppcastUrl() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::EveryDay);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking.
  updater.checkForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }
  server.stop();
  t.join();

  if (error) {
    QFAIL("Can't download latest version JSON");
    return;
  }

  // Latest version should be the newest one.
  const auto updateAvailable = updater.updateAvailable();
  const auto latestVersion = updater.latestVersion();
  QVERIFY(updateAvailable);
  QVERIFY(latestVersion == LATEST_VERSION);

  // A second check should not be made because a check has already been made the same day.
  done = false;
  updater.checkForUpdate();
  QVERIFY(!done);
}

void Tests::test_validAppcastUrlButNoServer() {
  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::EveryDay);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Start checking.
  updater.checkForUpdate();

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
  const auto updateAvailable = updater.updateAvailable();
  const auto latestVersion = updater.latestVersion();
  QVERIFY(!updateAvailable);
  QVERIFY(latestVersion == CURRENT_VERSION);
}

void Tests::test_validAppcastUrlButNoUpdate() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(CURRENT_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  auto done = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&done]() {
    done = true;
  });

  QObject::connect(&updater, &QtUpdater::checkForUpdateFailed, this, [&done, &error]() {
    error = true;
    done = true;
  });

  // Verify that these signals are called (or not) correctly.
  auto updateAvailableChanged = false;
  auto latestVersionChanged = false;
  QObject::connect(&updater, &QtUpdater::updateAvailableChanged, this, [&updateAvailableChanged]() {
    // Should be always called.
    updateAvailableChanged = true;
  });
  QObject::connect(&updater, &QtUpdater::latestVersionChanged, this, [&latestVersionChanged]() {
    // Should be called only if a (greater) new version exists.
    latestVersionChanged = true;
  });

  // Start checking.
  updater.checkForUpdate();

  // Wait for the client to receive the response from the server.
  if (!QTest::qWaitFor(
        [&done]() {
          return done;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }
  server.stop();
  t.join();

  if (error) {
    QFAIL("Can't download latest version JSON");
    return;
  }

  // Latest version should be the current one.
  const auto updateAvailable = updater.updateAvailable();
  const auto latestVersion = updater.latestVersion();

  QVERIFY(!updateAvailable);
  QVERIFY(latestVersion == CURRENT_VERSION);
  QVERIFY(updateAvailableChanged);
  QVERIFY(!latestVersionChanged);
}

void Tests::test_validChangelogUrl() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });
  server.Get(CHANGELOG_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    response.set_content(DUMMY_CHANGELOG, CONTENT_TYPE_MD);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.checkForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (!updater.updateAvailable()) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download changelog.
  auto downloadedChangelog = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::changelogDownloadFinished, this, [&downloadedChangelog]() {
    downloadedChangelog = true;
  });
  QObject::connect(&updater, &QtUpdater::changelogDownloadFailed, this, [&downloadedChangelog, &error]() {
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
  server.stop();
  t.join();

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
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.checkForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (!updater.updateAvailable()) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download changelog.
  auto downloadedChangelog = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::changelogDownloadFinished, this, [&downloadedChangelog]() {
    downloadedChangelog = true;
  });
  QObject::connect(&updater, &QtUpdater::changelogDownloadFailed, this, [&downloadedChangelog, &error]() {
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
  server.stop();
  t.join();

  QVERIFY(error);

  const auto changelogAvailable = updater.changelogAvailable();
  QVERIFY(!changelogAvailable);

  const auto& latestChangelog = updater.latestChangelog();
  QVERIFY(latestChangelog.isEmpty());
}

void Tests::test_validInstallerUrl() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });
  server.Get(INSTALLER_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    response.set_content(DUMMY_INSTALLER_DATA, CONTENT_TYPE_EXE);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.checkForUpdate();
  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }

  if (!updater.updateAvailable()) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download installer.
  auto downloadFinished = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::installerDownloadFinished, this, [&downloadFinished]() {
    downloadFinished = true;
  });
  QObject::connect(&updater, &QtUpdater::installerDownloadFailed, this, [&downloadFinished, &error]() {
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
  server.stop();
  t.join();

  if (error) {
    QFAIL("Can't download installer");
    return;
  }

  const auto installerAvailable = updater.installerAvailable();
  QVERIFY(installerAvailable);

  // Install update (synchronous).
  auto installationFailed = false;
  auto installationFinished = false;
  QObject::connect(&updater, &QtUpdater::installationFinished, this, [&installationFinished]() {
    installationFinished = true;
  });
  QObject::connect(&updater, &QtUpdater::installationFailed, this, [&installationFinished, &installationFailed]() {
    installationFailed = true;
    installationFinished = true;
  });
  updater.installUpdate(QtUpdater::InstallMode::ExecuteFile, {}, true, /*dry*/ true);
  QVERIFY(installationFinished);
  QVERIFY(!installationFailed);
}

void Tests::test_invalidInstallerUrl() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  // Check for updates.
  auto checked = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  updater.checkForUpdate();
  while (!checked) {
    QCoreApplication::processEvents();
  }

  if (!updater.updateAvailable()) {
    QFAIL("Update should be available before downloading changelog");
    return;
  }

  // Download installer.
  auto downloadFinished = false;
  auto error = false;
  QObject::connect(&updater, &QtUpdater::installerDownloadFinished, this, [&downloadFinished]() {
    downloadFinished = true;
  });
  QObject::connect(&updater, &QtUpdater::installerDownloadFailed, this, [&downloadFinished, &error]() {
    error = true;
    downloadFinished = true;
  });
  updater.downloadInstaller();
  while (!downloadFinished) {
    QCoreApplication::processEvents();
  }
  server.stop();
  t.join();

  QVERIFY(error);

  const auto installerAvailable = updater.installerAvailable();
  QVERIFY(!installerAvailable);

  // Install update (synchronous).
  auto installationFailed = false;
  auto installationFinished = false;
  QObject::connect(&updater, &QtUpdater::installationFailed, this, [&installationFailed, &installationFinished]() {
    installationFailed = true;
    installationFinished = true;
  });
  QObject::connect(&updater, &QtUpdater::installationFinished, this, [&installationFinished]() {
    installationFinished = true;
  });
  updater.installUpdate(QtUpdater::InstallMode::ExecuteFile, {}, true, /*dry*/ true);
  QVERIFY(installationFinished);
  QVERIFY(installationFailed);
}

void Tests::test_cancel() {
  // Server.
  httplib::Server server;
  server.Get(APPCAST_QUERY_REGEX, [](const httplib::Request&, httplib::Response& response) {
    // Sleep to let some time to cancel the download.
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    const auto appCast = getAppCast(LATEST_VERSION);
    response.set_content(appCast.toStdString(), CONTENT_TYPE_JSON);
  });

  // Start server in a thread.
  auto t = std::thread([&server]() {
    if (!server.listen(SERVER_HOST, SERVER_PORT)) {
      server.stop();
      QFAIL("Can't start server");
    }
  });

  // Configure updater.
  QtUpdater updater(SERVER_URL_FOR_CLIENT);
  updater.setFrequency(QtUpdater::Frequency::Never);

  // Check for updates.
  auto checked = false;
  auto cancelled = false;
  QObject::connect(&updater, &QtUpdater::checkForUpdateFinished, this, [&checked]() {
    checked = true;
  });
  QObject::connect(&updater, &QtUpdater::checkForUpdateCancelled, this, [&cancelled]() {
    cancelled = true;
  });
  updater.checkForUpdate();
  updater.cancel();

  if (!QTest::qWaitFor(
        [&checked]() {
          return checked;
        },
        updater.checkTimeout())) {
    QFAIL("Too late.");
  }
  server.stop();
  t.join();

  QVERIFY(cancelled);
}
