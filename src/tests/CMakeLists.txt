find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(TrafficSimTests
    roads_tests.cpp
    ../cpp/geo_utils.cpp
    ../cpp/OSMclient.cpp
    ../cpp/roads.cpp
    ../cpp/simulation.cpp
    ../cpp/traffic.cpp
    ../headers/OSMclient.h
)

target_include_directories(TrafficSimTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/headers
)

target_link_libraries(TrafficSimTests
    PRIVATE
        Qt6::Network
        Qt6::Positioning
        Qt6::Test
)

add_test(
    NAME road_network_parsing
    COMMAND TrafficSimTests roadNetworkBuildsAdjacencyList
)

add_test(
    NAME traffic_uses_meters_per_second
    COMMAND TrafficSimTests trafficUsesMetersPerSecond
)

add_test(
    NAME simulation_start_and_step
    COMMAND TrafficSimTests simulationStartsAndIntegratesTime
)

add_test(
    NAME overpass_default_query
    COMMAND TrafficSimTests defaultOverpassQueryBuildsRoadNetworks
)

set_tests_properties(overpass_default_query PROPERTIES
    LABELS "integration;network"
    TIMEOUT 70
)
