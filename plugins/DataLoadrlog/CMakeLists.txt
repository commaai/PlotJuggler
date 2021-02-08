
include_directories(  ../ )

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

QT5_WRAP_UI ( UI_SRC  ../selectlistdialog.ui  )

SET( SRC
    FileReader_DR.cpp
    dataload_rlog.cpp
    )

add_library(DataLoadrlog SHARED ${SRC} ${UI_SRC}  )

target_link_libraries(DataLoadrlog
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Xml_LIBRARIES}
    -lcapnp
    -lcapnpc
    -lbz2
    -lkj
    plotjuggler_plugin_base)

if(COMPILING_WITH_CATKIN)
    install(TARGETS DataLoadrlog
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION} )
elseif(COMPILING_WITH_AMENT)
    install(TARGETS DataLoadrlog
            DESTINATION  lib/${PROJECT_NAME} )
else()
    install(TARGETS DataLoadrlog DESTINATION bin  )
endif()

