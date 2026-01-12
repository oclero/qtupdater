# Deploys Qt libraries besides the target executable.
function(target_deploy_qt TARGET_NAME)
  if(WIN32 OR APPLE)
    # Use Qt6's built-in deployment script generation.
    qt_generate_deploy_app_script(
      TARGET ${TARGET_NAME}
      OUTPUT_SCRIPT deploy_script
      NO_UNSUPPORTED_PLATFORM_ERROR
    )

    # Install the deployment script to run at install time.
    install(SCRIPT ${deploy_script})

    # For development builds, also deploy at build time.
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt dependencies for ${TARGET_NAME}..."
      COMMAND ${CMAKE_COMMAND} -DQT_DEPLOY_PREFIX=$<TARGET_FILE_DIR:${TARGET_NAME}>
                               -DQT_DEPLOY_BIN_DIR=.
                               -P ${deploy_script}
      COMMENT "Running Qt deployment for ${TARGET_NAME}"
    )
  endif()
endfunction()
