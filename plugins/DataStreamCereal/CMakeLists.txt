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

file(GLOB dbc_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/opendbc/*.dbc)
foreach(dbc ${dbc_FILES})
  string(REPLACE ".dbc" ".cc" dbc_out ${dbc})
  string(REPLACE "/3rdparty/opendbc/" "/3rdparty/opendbc/can/dbc_out/" dbc_out ${dbc_out})
  set(SRC ${SRC} ${dbc_out})
endforeach()

add_library(DataStreamCereal SHARED ${SRC} ${UI_SRC}  )

target_link_libraries(DataStreamCereal
    ${Qt5Widgets_LIBRARIES}
    ${Cereal_LIBRARIES}  # has libcereal.a and libmessaging.a
    zmq
    capnpc.a
    capnp.a
    kj.a
    plotjuggler_plugin_base
    )


if(COMPILING_WITH_CATKIN)  # todo: copied from datastream plugin, remove?
    install(TARGETS DataStreamCereal
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION} )
elseif(COMPILING_WITH_AMENT)
    install(TARGETS DataStreamCereal
            DESTINATION  lib/${PROJECT_NAME} )
else()
    install(TARGETS DataStreamCereal DESTINATION bin  )
endif()
