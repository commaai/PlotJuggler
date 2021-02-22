include_directories(  ../ )

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

SET( SRC
  rlog_parser.cpp
  dataload_rlog.cpp
  parser.cc
  packer.cc
  common.h
  common.cc
  common_dbc.h
  dbc.cc
  cereal_gen_cpp_copy/log.capnp.h
  cereal_gen_cpp_copy/log.capnp.c++
  cereal_gen_cpp_copy/car.capnp.h
  cereal_gen_cpp_copy/car.capnp.c++
  cereal_gen_cpp_copy/legacy.capnp.h
  cereal_gen_cpp_copy/legacy.capnp.c++
  dbc_out/honda_accord_s2t_2018_can_generated.cc
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
