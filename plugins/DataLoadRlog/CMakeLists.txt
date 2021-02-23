include_directories( ../ ../../3rdparty/opendbc/can/ )

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DDYNAMIC_CAPNP)

file(GLOB opendbc_out_SRC "../../3rdparty/opendbc/can/dbc_out/*.cc")

SET(SRC
  rlog_parser.cpp
  dataload_rlog.cpp
  ../../3rdparty/opendbc/can/common.cc
  ../../3rdparty/opendbc/can/dbc.cc
  ../../3rdparty/opendbc/can/packer.cc
  ../../3rdparty/opendbc/can/parser.cc
  ${opendbc_out_SRC}
)

add_library(DataLoadRlog SHARED ${SRC} ${UI_SRC}  )

target_link_libraries(DataLoadRlog
  ${Qt5Widgets_LIBRARIES}
  ${Qt5Xml_LIBRARIES}
  -lcapnp
  -lcapnpc
  -lbz2
  -lkj
  plotjuggler_plugin_base)

install(TARGETS DataLoadRlog DESTINATION bin  )
