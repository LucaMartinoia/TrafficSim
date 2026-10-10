cmake_minimum_required(VERSION 3.20)

project(TrafficSim)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS
    Quick
    Qml
    QuickControls2
)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(TrafficSim WIN32
    src/cpp/main.cpp
    resources/TrafficSim.rc
)

qt_add_qml_module(TrafficSim
    URI TrafficSim
    VERSION 1.0
    QML_FILES
        src/gui/Main.qml
)

target_link_libraries(TrafficSim
    PRIVATE
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
)