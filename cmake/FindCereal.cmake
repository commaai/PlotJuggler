if (Cereal_LIBRARIES and Cereal_INCLUDE_DIRS)
  # in cache already
  set(Cereal_FOUND TRUE)
  message("Cereal found: ${Cereal_INCLUDE_DIRS}")
else (Cereal_LIBRARIES AND Cereal_INCLUDE_DIRS)
  message("cereal not found! searching...")
  message(${CMAKE_CURRENT_SOURCE_DIR})

  # find_path(Cereal_INCLUDE_DIRS /home/batman/openpilot/openpilot/cereal/messaging/messaging.h)

  find_path(Cereal_INCLUDE_DIRS
    NAMES
      bzlib.h
      capnp
      zmq
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(Cereal_LIBRARY
    NAMES
      cereal
    PATHS
      ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cereal
  )
  
  set(Cereal_INCLUDE_DIRS
    ${Cereal_INCLUDE_DIRS}
  )

  if (Cereal_LIBRARY)
    set(Cereal_LIBRARIES
        ${Cereal_LIBRARIES}
        ${Cereal_LIBRARY}
    )
  endif (Cereal_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Cereal DEFAULT_MSG Cereal_LIBRARIES Cereal_INCLUDE_DIRS)

  mark_as_advanced(Cereal_INCLUDE_DIRS Cereal_LIBRARIES)

endif(Cereal_LIBRARIES AND Cereal_INCLUDE_DIRS)

message("cereal include dirs: ${Cereal_INCLUDE_DIRS}")
message("cereal LIBs: ${Cereal_LIBRARIES}")