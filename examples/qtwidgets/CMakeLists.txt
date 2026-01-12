find_package(Qt6
  REQUIRED
    Core
    Network
    Widgets
)

qt_add_executable(QtWidgetsUpdaterExample WIN32 MACOSX_BUNDLE
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QtUpdateWidget.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/QtUpdateWidget.h
)

qt_add_resources(QtWidgetsUpdaterExample "resources"
  PREFIX "/example"
  BASE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    icon.ico
)
target_link_libraries(QtWidgetsUpdaterExample
  PRIVATE
    oclero::QtUpdater
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)

set_target_properties(QtWidgetsUpdaterExample PROPERTIES
  INTERNAL_CONSOLE OFF
  EXCLUDE_FROM_ALL ON
  FOLDER examples
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.oclero.QtWidgetsUpdaterExample"
  MACOSX_BUNDLE_BUNDLE_NAME "QtWidgetsUpdaterExample"
)

############# Minimal example ends here #############
target_deploy_qt(QtWidgetsUpdaterExample)
