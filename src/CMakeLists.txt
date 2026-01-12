set(LIB_TARGET_NAME ${PROJECT_NAME})

find_package(Qt6
  REQUIRED
    Core
    Network
)

set(HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qtupdater/AppCast.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qtupdater/Downloader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qtupdater/Controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qtupdater/Types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/oclero/qtupdater/Updater.h
)

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/AppCast.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/Downloader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/Controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/Types.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/Updater.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/utils/EnumUtils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/utils/FileUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/utils/FileUtils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source/oclero/qtupdater/utils/SettingsUtils.h
)

# Configure target.
qt_add_library(${LIB_TARGET_NAME} STATIC
  ${HEADERS}
  ${SOURCES}
)
add_library(${PROJECT_NAMESPACE}::${LIB_TARGET_NAME} ALIAS ${LIB_TARGET_NAME})

target_include_directories(${LIB_TARGET_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source>
)

target_link_libraries(${LIB_TARGET_NAME}
  PRIVATE
    Qt6::Core
    Qt6::Network
)

set_target_properties(${LIB_TARGET_NAME}
  PROPERTIES
    OUTPUT_NAME ${LIB_TARGET_NAME}
    PROJECT_LABEL ${LIB_TARGET_NAME}
    FOLDER lib
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    DEBUG_POSTFIX _debug
)

target_compile_options(${LIB_TARGET_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Create source groups.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES
  ${HEADERS}
  ${SOURCES}
)

# Select correct startup project in Visual Studio.
if(WIN32)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${LIB_TARGET_NAME})
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install library
install(TARGETS ${LIB_TARGET_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

# Install CMake config files
set(CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAMESPACE}::
  DESTINATION ${CONFIG_INSTALL_DIR}
)

# Create config version file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

# Create config file
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

# Install config files
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION ${CONFIG_INSTALL_DIR}
)
