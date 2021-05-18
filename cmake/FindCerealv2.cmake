if (Cerealv2_INCLUDE_DIRS)
  # in cache already
  set(Cerealv2_FOUND TRUE)
else (Cerealv2_LIBRARIES AND Cerealv2_INCLUDE_DIRS)
  message("finding cerealv2 libs...")
  
  find_path(Cerealv2_INCLUDE_DIRS
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
  
  set(Cerealv2_INCLUDE_DIRS
    ${Cerealv2_INCLUDE_DIRS}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Cerealv2 DEFAULT_MSG Cerealv2_INCLUDE_DIRS)

  mark_as_advanced(Cerealv2_INCLUDE_DIRS)

endif(Cerealv2_INCLUDE_DIRS)
