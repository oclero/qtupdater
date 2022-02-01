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

## Usage

1. Add the library's repository as a Git submodule.

   ```bash
   git submodule add git@github.com:oclero/QtUpdater.git submodules/qtupdater
   ```

2. Download submodules.

   ```bash
   git submodule update --init --recursive
   ```

3. Add the library to your CMake project.

   ```cmake
   add_subdirectory(submodules/qtupdater)
   ```

4. Link with the library in CMake.

   ```cmake
   target_link_libraries(your_project oclero::QtUpdater)
   ```

5. Include the only necessary header in your C++ file.

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
curl http://localhist:8000?version=latest

# This is equivalent to getting the latest version.
curl http://localhist:8000

# If the following version exist, the request is valid.
curl http://localhist:8000?version=1.2.3

# If the file exist, the request is valid.
curl http://localhist:8000/v1.1.0.exe
```

### Client

```c++
// Create an updater.
oclero::QtUpdater updater("https://server/endpoint");

// Subscribe to all necessary signals. See documentation for complete list.
QObject::connect(&updater, &oclero::QtUpdater::updateAvailableChanged,
                 &updater, [&updater]() {
  if (updater.updateAvailable()) {
    qDebug() << "Update available! You have: "
      << qPrintable(updater.currentVersion())
      << " - Latest is: "
      << qPrintable(updater.latestVersion());
  } else {
    qDebug() << "You have the latest version.";
  }
});

// Start checking.
updater.checkForUpdate();
```

## Author

**Olivier Cléro** | [email](mailto:oclero@pm.me) | [website](https://www.olivierclero.com) | [github](https://www.github.com/oclero) | [gitlab](https://www.gitlab.com/oclero)

## License

**QtUpdater** is available under the MIT license. See the [LICENSE](LICENSE) file for more info.
