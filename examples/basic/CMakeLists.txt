find_package(Qt6 REQUIRED Core Network)

qt_add_executable(BasicUpdaterExample
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
target_link_libraries(BasicUpdaterExample PRIVATE oclero::QtUpdater Qt6::Core Qt6::Network)

set_target_properties(BasicUpdaterExample PROPERTIES
  INTERNAL_CONSOLE ON
  EXCLUDE_FROM_ALL ON
  FOLDER examples
)

############# Minimal example ends here #############
target_deploy_qt(BasicUpdaterExample)
