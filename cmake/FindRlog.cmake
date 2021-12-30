if (Rlog_INCLUDE_DIRS AND Rlog_LIBRARIES)
  # in cache already
  set(Rlog_FOUND TRUE)
else (Rlog_INCLUDE_DIRS AND Rlog_LIBRARIES)

  # find capnp & capnpc
  find_package(capnp CONFIG QUIET)
  find_package(capnpc CONFIG QUIET)
  if (capnp_FOUND AND capnpc_FOUND)
    set(Rlog_capnp_INCLUDE_DIR ${capnp_INCLUDE_DIRS})
    set(Rlog_capnp_LIBRARIES ${capnp_LIBRARIES})

    set(Rlog_capnpc_INCLUDE_DIR ${capnpc_INCLUDE_DIRS})
    set(Rlog_capnpc_LIBRARIES ${capnpc_LIBRARIES})

  else(capnp_FOUND AND capnpc_FOUND)
    #### capnp
    find_path(Rlog_capnp_INCLUDE_DIR
      NAMES capnp/serialize.h
      PATHS /usr/include
            /usr/local/include
            /opt/local/include
            /opt/homebrew/include
            /sw/include
    )

    find_library(Rlog_capnp_LIBRARIES
      NAMES capnp
      PATHS /usr/lib
            /usr/local/lib
            /opt/local/lib
            /opt/homebrew/lib
            /sw/lib
    )

    #### capnpc
    find_library(Rlog_capnpc_LIBRARIES
      NAMES capnpc
      PATHS /usr/lib
            /usr/local/lib
            /opt/local/lib
            /opt/homebrew/lib
            /sw/lib
    )

  endif(capnp_FOUND AND capnpc_FOUND)

  # find bzlib
  find_package(bzip2 CONFIG QUIET)
  if(bzip2_FOUND)
    set(Rlog_bzip2_INCLUDE_DIR ${bzip2_INCLUDE_DIRS})
    set(Rlog_bzip2_LIBRARIES ${bzip2_LIBRARIES})

  else(bzip2_FOUND)
    find_path(Rlog_bzip2_INCLUDE_DIR
      NAMES bzlib.h
      PATHS /usr/include
            /usr/local/include
            /opt/local/include
            /opt/homebrew/include
            /sw/include
    )

    find_library(Rlog_bzip2_LIBRARIES
      NAMES bz2
      PATHS /usr/lib
            /usr/local/lib
            /opt/local/lib
            /opt/homebrew/lib
            /sw/lib
    )
  endif(bzip2_FOUND)

  # find opendbc common
  find_path(Rlog_opendbc_INCLUDE_DIR
    NAMES common.h
    PATHS ${CMAKE_SOURCE_DIR}/3rdparty/opendbc/can
          /usr/include
          /usr/local/include
          /opt/local/include
          /opt/homebrew/include
          /sw/include
  )

  set(Rlog_INCLUDE_DIRS
    ${Rlog_capnp_INCLUDE_DIR}
    ${Rlog_bzip2_INCLUDE_DIR}
    ${Rlog_opendbc_INCLUDE_DIR}
    CACHE INTERNAL "Rlog include dependencies"
    FORCE
  )

  set(Rlog_LIBRARIES
    ${Rlog_capnp_LIBRARIES}
    ${Rlog_capnpc_LIBRARIES}
    ${Rlog_bzip2_LIBRARIES}
    CACHE INTERNAL "Rlog link dependencies"
    FORCE
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Rlog DEFAULT_MSG Rlog_INCLUDE_DIRS)
  find_package_handle_standard_args(Rlog DEFAULT_MSG Rlog_LIBRARIES)

  mark_as_advanced(Rlog_INCLUDE_DIRS Rlog_LIBRARIES)

endif(Rlog_INCLUDE_DIRS AND Rlog_LIBRARIES)
