if (Cereal_INCLUDE_DIRS)
  # in cache already
  set(Cereal_FOUND TRUE)
  message("Cereal found!")
else (Cereal_LIBRARIES AND Cereal_INCLUDE_DIRS)
  message("cereal not found! searching...")
  message(${CMAKE_CURRENT_SOURCE_DIR})

  find_path(Cereal_INCLUDE_DIRS /home/batman/openpilot/openpilot/cereal/messaging/messaging.h)

  find_path(Rlog_INCLUDE_DIRS
    NAMES
      messaging.h
    PATHS
      ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cereal
  )
  
  set(Cereal_INCLUDE_DIRS
    ${Cereal_INCLUDE_DIRS}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Cereal DEFAULT_MSG Cereal_INCLUDE_DIRS)

  mark_as_advanced(Cereal_INCLUDE_DIRS)

endif(Cereal_INCLUDE_DIRS)
