cmake_minimum_required(VERSION 3.19.0)
enable_testing()

# Set project information.
project("QtUpdater"
  LANGUAGES CXX
  VERSION 1.0.0.0
)
set(PROJECT_NAMESPACE "oclero")

# Global flags.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# # The library.
add_subdirectory(lib)

# # Tests.
add_subdirectory(tests)

# # Examples.
add_subdirectory(examples/basic)
