cmake_minimum_required(VERSION 3.20)

# Project name
project(TrafficSim)

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

# Qt settings
qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(TrafficSim #WIN32 # WIN32 make the program execute without a terminal
    src/cpp/main.cpp
    src/cpp/backend.cpp
    src/cpp/overpassclient.cpp
    src/cpp/traffic.cpp
    resources/TrafficSim.rc
)

qt_add_qml_module(TrafficSim
    URI TrafficSim
    VERSION 1.0
    QML_FILES
        src/gui/Main.qml
        src/gui/MapCanvas.qml
        src/gui/Cars.qml
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