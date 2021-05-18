
include_directories(../)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

QT5_WRAP_UI ( UI_SRC  datastream_cereal.ui  )

SET( SRC datastream_cereal.cpp )

add_library(DataStreamCereal SHARED ${SRC} ${UI_SRC}  )

target_include_directories(
    DataStreamCereal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/cereal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/cereal/messaging
    )

message("debug! ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/cereal/messaging")

add_library(cereal STATIC IMPORTED)
# set_property(TARGET cereal PROPERTY IMPORTED_LOCATION "/home/batman/openpilot/PlotJuggler/3rdparty/cereal/libcereal_shared.so")
set_target_properties(cereal PROPERTIES IMPORTED_LOCATION "/home/batman/openpilot/PlotJuggler/3rdparty/cereal/libcereal.a")

target_link_libraries(DataStreamCereal
    ${Qt5Widgets_LIBRARIES}
    capnpc.a
    capnp.a
    kj.a
    # -lcereal
    cereal
    # -lmessaging
    plotjuggler_plugin_base
    )


if(COMPILING_WITH_CATKIN)
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


