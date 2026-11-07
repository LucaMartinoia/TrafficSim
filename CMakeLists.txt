cmake_minimum_required(VERSION 3.20)

# Project name
project(TrafficSim)

include(CTest)

# C++ standard
set(CMAKE_CXX_STANDARD 20)

# Dependencies injections
find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Qml
    QuickControls2
    Graphs
    Location
    Positioning
    Network
)

# Files in the various folders
file(GLOB CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/*.cpp"
)

file(GLOB HEADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/headers/*.h"
)

file(GLOB QML_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.qml"
)

set(ICON_FILES
    resources/circle-chevron-right-solid-full.svg
    resources/floppy-disk-solid-full.svg
    resources/gear-solid-full.svg
    resources/location-crosshairs-solid-full.svg
)

set(APP_ICON resources/TrafficSim.ico)
set_source_files_properties(${APP_ICON} PROPERTIES
    QT_RESOURCE_ALIAS "TrafficSim.ico"
)

# Keep icon URLs independent of the source directory
foreach(ICON_FILE IN LISTS ICON_FILES)
    get_filename_component(ICON_NAME "${ICON_FILE}" NAME)
    set_source_files_properties("${ICON_FILE}" PROPERTIES
        QT_RESOURCE_ALIAS "${ICON_NAME}"
    )
endforeach()

# Qt settings
qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(TrafficSim WIN32 # Necessary to make the app start without console
    ${CPP_FILES}
    ${HEADER_FILES}
    resources/TrafficSim.rc
)

qt_add_resources(TrafficSim "TrafficSimIcons"
    PREFIX "/icons"
    FILES ${ICON_FILES}
)

qt_add_resources(TrafficSim "TrafficSimWindowIcon"
    PREFIX "/"
    FILES ${APP_ICON}
)

target_include_directories(TrafficSim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/headers
)

qt_add_qml_module(TrafficSim
    URI TrafficSim
    VERSION 1.0
    QML_FILES ${QML_FILES}
)

# Linking DLL
target_link_libraries(TrafficSim
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Graphs
        Qt6::Location
        Qt6::Positioning
        Qt6::Network
)

if(BUILD_TESTING)
    add_subdirectory(src/tests EXCLUDE_FROM_ALL)
endif()