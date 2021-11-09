set(TESTS_TARGET_NAME ${PROJECT_NAME}Tests)

find_path(CPP_HTTPLIB_INCLUDE_DIRS "httplib.h")
if(NOT ${CPP_HTTPLIB_INCLUDE_DIRS} STREQUAL "CPP_HTTPLIB_INCLUDE_DIRS-NOTFOUND")
  message("-- httplib found. Tests will be added.")
  find_package(Qt5
    REQUIRED
      Core
      Test
  )

  add_executable(${TESTS_TARGET_NAME})
  set_target_properties(${TESTS_TARGET_NAME}
    PROPERTIES
      AUTOMOC ON
      AUTORCC ON
      INTERNAL_CONSOLE ON
      EXCLUDE_FROM_ALL ON
      FOLDER tests
  )
  set(TESTS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QtUpdaterTests.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/QtUpdaterTests.cpp
  )
  target_sources(${TESTS_TARGET_NAME}
    PRIVATE
      ${TESTS_SOURCES}
  )
  target_include_directories(${TESTS_TARGET_NAME}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CPP_HTTPLIB_INCLUDE_DIRS} #cpp-httplib
  )
  target_link_libraries(${TESTS_TARGET_NAME}
    PRIVATE
      ${PROJECT_NAMESPACE}::${PROJECT_NAME}
      Qt5::Core
      Qt5::Test
  )
  add_test(NAME ${TESTS_TARGET_NAME}
    COMMAND $<TARGET_FILE:${TESTS_TARGET_NAME}>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${TESTS_TARGET_NAME}>)

  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TESTS_SOURCES})
else()
  message("-- Can't find httplib. Tests will be ignored.")
endif()

target_deploy_qt(${TESTS_TARGET_NAME})
