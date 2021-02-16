include_directories(  ../ )

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

QT5_WRAP_UI ( UI_SRC  ../selectlistdialog.ui  )

SET( SRC
  rlog_parser.cpp
  dataload_rlog.cpp
  )

add_library(DataLoadRLog SHARED ${SRC} ${UI_SRC}  )

target_link_libraries(DataLoadRLog
  ${Qt5Widgets_LIBRARIES}
  ${Qt5Xml_LIBRARIES}
  -lcapnp
  -lcapnpc
  -lbz2
  -lkj
  plotjuggler_plugin_base)

if(COMPILING_WITH_CATKIN)
  install(TARGETS DataLoadRLog
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION} )
elseif(COMPILING_WITH_AMENT)
  install(TARGETS DataLoadRLog
    DESTINATION  lib/${PROJECT_NAME} )
else()
  install(TARGETS DataLoadRLog DESTINATION bin  )
endif()
