find_package(Qt5
  REQUIRED
    Core
    Network
    Widgets
)

add_executable(QtWidgetsUpdaterExample WIN32 MACOSX_BUNDLE)
target_sources(QtWidgetsUpdaterExample PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QtUpdateWidget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QtUpdateWidget.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
)
target_link_libraries(QtWidgetsUpdaterExample
  PRIVATE
    oclero::QtUpdater
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
)

set_target_properties(QtWidgetsUpdaterExample PROPERTIES
  INTERNAL_CONSOLE OFF
  EXCLUDE_FROM_ALL ON
  FOLDER examples
  AUTOMOC ON
	AUTORCC ON
)

############# Minimal example ends here #############
target_deploy_qt(QtWidgetsUpdaterExample)
