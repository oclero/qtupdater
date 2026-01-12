cmake_minimum_required(VERSION 3.21.0)
enable_testing()

set(QT_CREATOR_SKIP_MAINTENANCE_TOOL_PROVIDER ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Set project information.
project("QtUpdater"
  LANGUAGES CXX
  VERSION 2.0.0.0
)

include(DeployQt)
include(GNUInstallDirs)
set(PROJECT_NAMESPACE "oclero")

# Global flags.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find Qt.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

# qt_standard_project_setup() is only available in Qt >= 6.3.
if(COMMAND qt_standard_project_setup)
  qt_standard_project_setup()
else()
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTORCC ON)
endif()

# The library.
add_subdirectory(src)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED OFF)
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")

  # Tests.
  add_subdirectory(tests)

  # Examples.
  add_subdirectory(examples/basic)
  add_subdirectory(examples/qtwidgets)
endif()
