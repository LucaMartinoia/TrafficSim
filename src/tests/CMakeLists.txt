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
    NAME bounding_box_validates_coordinates
    COMMAND TrafficSimTests boundingBoxRequiresValidOrderedCoordinates
)

add_test(
    NAME map_view_validates_coordinate
    COMMAND TrafficSimTests mapViewRequiresValidCoordinateAndZoom
)

add_test(
    NAME traffic_skips_car_without_route
    COMMAND TrafficSimTests trafficSkipsCarWhenRouteIsUnavailable
)

add_test(
    NAME simulation_defers_until_traffic_update
    COMMAND TrafficSimTests simulationDefersUntilTrafficUpdateIsImplemented
)

add_test(
    NAME overpass_default_query
    COMMAND TrafficSimTests defaultOverpassQueryBuildsRoadNetworks
)

set_tests_properties(overpass_default_query PROPERTIES
    LABELS "integration;network"
    TIMEOUT 70
)
