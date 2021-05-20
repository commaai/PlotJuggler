include_directories(../)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DDYNAMIC_CAPNP)

QT5_WRAP_UI ( UI_SRC  datastream_cereal.ui  )

SET(SRC
  datastream_cereal.cpp
  ../DataLoadRlog/rlog_parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/opendbc/can/common.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/opendbc/can/dbc.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/opendbc/can/packer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/opendbc/can/parser.cc
)

add_library(DataStreamCereal SHARED ${SRC} ${UI_SRC}  )

execute_process(COMMAND cp opendbc/SConstruct .
  COMMAND cp -r opendbc/site_scons .
  COMMAND scons -j8
  COMMAND ls -lah
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
)

target_link_libraries(DataStreamCereal
    ${Qt5Widgets_LIBRARIES}
    ${Cereal_LIBRARIES}  # has libcereal.a and libmessaging.a
    zmq
    capnpc.a
    capnp.a
    kj.a
    plotjuggler_plugin_base
)
