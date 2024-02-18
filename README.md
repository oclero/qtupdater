<div align="center">
	<img height="50" src="logo.svg">
</div>

# QtUpdater

[![License: MIT](https://img.shields.io/badge/license-MIT-green)](https://mit-license.org/)
[![CMake version](https://img.shields.io/badge/CMake-3.19+-064F8C?logo=cmake)](https://www.qt.io)
[![C++ version](https://img.shields.io/badge/C++-17-00599C?logo=++)](https://www.qt.io)
[![Qt version](https://img.shields.io/badge/Qt-5.15.2+-41CD52?logo=qt)](https://www.qt.io)

Updater for Qt5 (auto-updates).

---

### Table of Contents

- [Requirements](#requirements)
- [Features](#features)
- [Usage](#usage)
- [Server Specifications](#server-specifications)
- [Example](#example)
- [Author](#author)
- [License](#license)

---

## Requirements

- Platform: Windows, MacOS, Linux (except for installer auto-start).
- [CMake 3.19+](https://cmake.org/download/)
- [Qt 5.15+](https://www.qt.io/download-qt-installer)
- [cpphttplib](https://github.com/yhirose/cpp-httplib) (Only for unit tests)

## Features

This library contains:

- A core: `QtUpdater`
- A controller: `QtUpdateController`, that may be use with QtWidgets or QtQuick/QML.
- A widget: `QtUpdateWidget`, that may be used as a `QWidget` or inside a `QDialog`.

It provides these features:

- Get latest update information.
- Get changelog.
- Get installer.
- Execute installer.
- Temporarly stores the update data in the `temp` folder.
- Verify checksum after downloading and before executing installer.

## Usage

1. Add the library as a dependency with CMake FetchContent.

   ```cmake
   include(FetchContent)
   FetchContent_Declare(QtUpdater
    GIT_REPOSITORY "https://github.com/oclero/qtupdater.git"
   )
   FetchContent_MakeAvailable(QtUpdater)
   ```

2. Link with the library in CMake.

   ```cmake
   target_link_libraries(your_project oclero::QtUpdater)
   ```

3. Include the only necessary header in your C++ file.

   ```c++
   #include <oclero/QtUpdater.hpp>
   ```

## Server Specifications

### Protocol

The protocol is the following:

1. The client sends a request to the endpoint URL of your choice. Example (with curl):

   ```bash
   curl http://server/endpoint?version=latest
   ```

2. The server answers by sending back an _appcast_: a JSON file containing the necessary information. The _appcast_ must look like the following:

   ```json
   {
     "version": "x.y.z",
     "date": "dd/MM/YYYY",
     "checksum": "418397de9ef332cd0e477ff5e8ca38d4",
     "checksumType": "md5",
     "installerUrl": "http://server/endpoint/package-name.exe",
     "changelogUrl": "http://server/endpoint/changelog-name.md"
   }
   ```

3. The client downloads the changelog from `changelogUrl`, if any provided (facultative step).

4. The client downloads the installer from `installerUrl`, if any provided.

5. The client installs the installer:
   - The client may start the installer and quit, if necessary.
   - It may also move the downloaded file to some location.

## Example

### Server

A _very basic_ server written in Python is included as testing purposes. Don't use in production environment!

```bash
# Start with default config.
python examples/dev_server/main.py

# ... Or set your own config.
python examples/dev_server/main.py --dir /some-directory --port 8000 --address 127.0.0.1
```

Some examples of valid requests for this server:

```bash
# The client must be able to retrieve the latest version.
curl http://localhost:8000?version=latest

# This is equivalent to getting the latest version.
curl http://localhost:8000

# If the following version exist, the request is valid.
curl http://localhost:8000?version=1.2.3

# If the file exist, the request is valid.
curl http://localhost:8000/v1.1.0.exe
```

### Client

```c++
// Create an updater.
oclero::QtUpdater updater("https://server/endpoint");

// Subscribe to all necessary signals. See documentation for complete list.
QObject::connect(&updater, &oclero::QtUpdater::updateAvailabilityChanged,
                 &updater, [&updater]() {
  if (updater.updateAvailability() == oclero::QtUpdater::UpdateAvailable::Available) {
    qDebug() << "Update available! You have: "
      << qPrintable(updater.currentVersion())
      << " - Latest is: "
      << qPrintable(updater.latestVersion());
  } else if (updater.updateAvailability() == oclero::QtUpdater::UpdateAvailable::UpToDate) {
    qDebug() << "You have the latest version.";
  } else {
    qDebug() << "Error.";
  }
});

// Start checking.
updater.checkForUpdate();
```

## Author

**Olivier Cléro** | [email](mailto:oclero@pm.me) | [website](https://www.olivierclero.com) | [github](https://www.github.com/oclero) | [gitlab](https://www.gitlab.com/oclero)

## License

**QtUpdater** is available under the MIT license. See the [LICENSE](LICENSE) file for more info.
