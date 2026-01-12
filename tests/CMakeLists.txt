set(TESTS_TARGET_NAME ${PROJECT_NAME}Tests)

find_package(Qt6
  REQUIRED
  COMPONENTS
    Core
    Network
    Test
    HttpServer
)

set(TESTS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/QtUpdaterTests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/QtUpdaterTests.cpp
)

qt_add_executable(${TESTS_TARGET_NAME}
  ${TESTS_SOURCES}
)

set_target_properties(${TESTS_TARGET_NAME}
  PROPERTIES
    INTERNAL_CONSOLE ON
    EXCLUDE_FROM_ALL ON
    FOLDER tests
)
target_include_directories(${TESTS_TARGET_NAME}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${TESTS_TARGET_NAME}
  PRIVATE
    ${PROJECT_NAMESPACE}::${PROJECT_NAME}
    Qt6::Core
    Qt6::Test
    Qt6::HttpServer
)
add_test(NAME ${TESTS_TARGET_NAME}
  COMMAND $<TARGET_FILE:${TESTS_TARGET_NAME}>
  WORKING_DIRECTORY $<TARGET_FILE_DIR:${TESTS_TARGET_NAME}>)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TESTS_SOURCES})

# target_deploy_qt(${TESTS_TARGET_NAME})
