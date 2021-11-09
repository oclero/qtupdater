find_package(Qt5 REQUIRED Core)

add_executable(BasicUpdaterExample)
target_sources(BasicUpdaterExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(BasicUpdaterExample PRIVATE oclero::QtUpdater Qt5::Core)

set_target_properties(BasicUpdaterExample PROPERTIES
  INTERNAL_CONSOLE ON
  EXCLUDE_FROM_ALL ON
  FOLDER examples
)

############# Minimal example ends here #############
target_deploy_qt(BasicUpdaterExample)
