# TrafficSim

TrafficSim is a Qt 6 traffic simulator. Its C++ backend retrieves road data from OpenStreetMap's Overpass API, builds a directed road graph, and provides the beginnings of a traffic model. The Qt Quick/QML interface displays the map and road geometry.

## Project setup

The project is built with CMake and requires Qt 6. Install the Qt components listed in the root `CMakeLists.txt` and a compatible C++ toolchain. In VS Code, install the **CMake Tools** and **C/C++** extensions, open the repository folder, and select a CMake kit when prompted.

Use the VS Code Command Palette (`Ctrl+Shift+P`) to select a CMake variant, such as Debug or Release, and use the CMake status bar or Command Palette to configure and build the project.

## Application flow

1. The user pans/zooms to a desired map area and clicks **Lock view**.
2. `MapCanvas` converts the visible map corners into a geographic bounding box. `Backend` expands each side by five percent of the visible latitude/longitude span and dispatches the request to `OverpassClient`.
3. `OverpassClient` sends the Overpass query, validates responses, and caches valid JSON under Qt's writable `AppDataLocation` in a `roads_data` directory, using a separate file per query.
4. On success, the backend starts `Simulation`, which builds the full `RoadNetwork`, constructs a reduced routing graph, logs graph information, and resets simulation time and traffic state before the backend dispatches road coordinate paths to the QML map.
5. On request failure, the backend logs the error. The QML map receives valid way geometry through the backend's `roadsReceived` signal.

The Overpass service and a network connection are required on a cache miss. Remove the corresponding JSON file in the app data cache directory to fetch that query again.

## Road network

`RoadNetwork::buildFromOverpassResponse` reads Overpass way geometry and creates a full adjacency list:

- `RoadNode` stores a Qt `QGeoCoordinate` and outgoing edges.
- `RoadEdge::destination` is the index of the next node; its source is the node whose adjacency list contains it.
- Each full edge stores segment length in meters, maximum velocity in meters per second, and estimated travel time in seconds. OSM `maxspeed` values in km/h (or unitless values) and mph are converted to meters per second; if a usable numeric value is absent, the parser assumes 50 km/h and converts it to meters per second.
- Coordinates shared between ways are merged. OSM `oneway` tags and supported implied one-way cases set edge direction.

Each edge stores its static `length`, `maxVelocity`, and `freeFlowTime`, plus a dynamic `adaptiveTime`. `RoutingMetric::Distance` selects length, `TravelTime` selects free-flow time, and `AdaptiveTime` selects the traffic-dependent estimate. Adaptive weights are refreshed from an occupancy snapshot; this calculation and route finding are not implemented yet.

## Traffic model and geographic utilities

`RoadEdgeId` identifies an outgoing edge by source node index and local edge index. `RoadRoute` stores these references so a car can access the current segment directly. `RoadNetwork::findRoute` takes endpoint node indices and a `RoutingMetric`, and reports whether a directed route was found.

`TrafficCar` stores its route, current route segment and progress in meters, current `QGeoCoordinate`, velocity, and arrival state. `Traffic::edgeOccupancy` derives per-edge vehicle counts from cars' current route edges. `Simulation::step` is structured to move cars against one occupancy snapshot, refresh adaptive edge weights from the resulting occupancy, and spawn cars up to the requested active-car count using the selected routing metric. Car movement, adaptive-weight calculation, and route finding remain stubs. The simulation uses a 1/60-second timestep.

`GeoUtils` wraps Qt's `QGeoCoordinate` geodesic operations to calculate distance in meters and a coordinate at a distance along a segment.

## Build and run tests in VS Code

Tests use Qt Test and CTest. The test target and test registration are kept in `src/tests/CMakeLists.txt`, which the root CMake project includes when testing is enabled.

1. In VS Code, use the **CMake: Build Target** command and select `TrafficSimTests`.
2. Open the **CMake** view, expand **Tests**, and run a test or the full test suite.
3. Use the test results view to inspect failures and output.

The current tests cover:

- `road_network_parsing`: parses an inline Overpass JSON fixture and checks the full adjacency list, shared nodes, one-way direction, speed conversion to meters per second, and reduced cumulative edge weights.
- `traffic_uses_meters_per_second`: verifies the road speed limit and simulation movement use meters per second and meters.
- `simulation_start_and_step`: verifies starting prepares the network without advancing time, and an explicit simulation step integrates one configured time step.
- `overpass_default_query`: issues the live default Overpass query and validates that the parsed full and reduced graphs contain nodes and edges. This integration test needs network access and can fail when Overpass is unavailable.

## Qt deployment

For a self-contained Windows Release build, use the `windeployqt` tool from the Qt installation that matches the selected compiler and build:

```powershell
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe `
  --release `
  --qmldir .\src\gui `
  .\build\Release\TrafficSim.exe
```
