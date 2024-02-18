cmake_minimum_required(VERSION 3.21.0)
enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DeployQt)

# Set project information.
project("QtUpdater"
  LANGUAGES CXX
  VERSION 1.5.0.0
)
set(PROJECT_NAMESPACE "oclero")

# Global flags.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Library dependencies.
include(FetchContent)
FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
  GIT_SHALLOW TRUE
)
FetchContent_Declare(qtutils
  GIT_REPOSITORY https://github.com/oclero/qtutils.git
)
FetchContent_MakeAvailable(httplib qtutils)

# The library.
add_subdirectory(src)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  # Tests.
  add_subdirectory(tests)

  # Examples.
  add_subdirectory(examples/basic)
  add_subdirectory(examples/qtwidgets)
endif()
