# TrafficSim

TrafficSim is a Qt 6 traffic simulator. Its C++ backend retrieves road data from OpenStreetMap's Overpass API, builds a directed road graph, and provides the beginnings of a traffic model. The Qt Quick/QML interface displays the map and road geometry.

## Project setup

The project is built with CMake and requires Qt 6. Install the Qt components listed in the root `CMakeLists.txt` and a compatible C++ toolchain. In VS Code, install the **CMake Tools** and **C/C++** extensions, open the repository folder, and select a CMake kit when prompted.

Use the VS Code Command Palette (`Ctrl+Shift+P`) to select a CMake variant, such as Debug or Release, and use the CMake status bar or Command Palette to configure and build the project.

## Application flow

1. The user pans/zooms to a desired map area and clicks **Lock view**.
2. `MapCanvas` converts the visible map corners into a geographic bounding box. `Backend` expands each side by five percent of the visible latitude/longitude span and dispatches the request to `OverpassClient`.
3. `OverpassClient` sends the Overpass query, validates responses, and caches valid JSON under Qt's writable `AppDataLocation` in a `roads_data` directory, using a separate file per query.
4. On success, the backend builds the full `RoadNetwork`, dispatches road paths to the map, and starts a 50 ms `QTimer`. Each timer event advances one fixed simulation step and sends active car coordinates to the QML map.
5. On request failure, the backend logs the error. The QML map receives valid way geometry through the backend's `roadsReceived` signal.

The Overpass service and a network connection are required on a cache miss. Remove the corresponding JSON file in the app data cache directory to fetch that query again.

## Road network

`RoadNetwork::buildFromOverpassResponse` reads Overpass way geometry and creates a full adjacency list:

- `RoadNode` stores a Qt `QGeoCoordinate` and outgoing edges.
- `RoadEdge::destination` is the index of the next node; its source is the node whose adjacency list contains it.
- Each full edge stores segment length in meters, maximum velocity in meters per second, and estimated travel time in seconds. OSM `maxspeed` values in km/h (or unitless values) and mph are converted to meters per second; if a usable numeric value is absent, the parser assumes 50 km/h and converts it to meters per second.
- Coordinates shared between ways are merged. OSM `oneway` tags and supported implied one-way cases set edge direction.

Each edge stores its static `length`, `maxVelocity`, and `freeFlowTime`, plus a dynamic `adaptiveTime`. `RoutingMetric::Distance` selects length, `TravelTime` selects free-flow time, and `AdaptiveTime` selects the adaptive estimate. Route finding uses Dijkstra's algorithm with the selected edge cost. Adaptive-time routing currently uses free-flow travel time; occupancy-dependent costs are not implemented yet.

## Traffic model and geographic utilities

`RoadEdgeId` identifies an outgoing edge by source node index and local edge index. `RoadRoute` stores these references so a car can access the current segment directly. `RoadNetwork::findRoute` takes endpoint node indices and reports whether a directed route was found using the network's selected routing metric.

`Car` stores its route, current route segment and progress in meters, current `QGeoCoordinate`, velocity, and arrival state. Traffic movement carries remaining time across edge boundaries, updates geographic positions, and records edge occupancy. `Simulation::update` advances by 0.05 seconds and spawns cars up to its active-car target. The backend timer and QML event handling run on the GUI thread; keep each update small so map interaction stays responsive.

`GeoUtils` wraps Qt's `QGeoCoordinate` geodesic operations to calculate distance in meters and a coordinate at a distance along a segment.

## Build and run tests in VS Code

Tests use Qt Test and CTest. The test target and test registration are kept in `src/tests/CMakeLists.txt`, which the root CMake project includes when testing is enabled.

1. In VS Code, use the **CMake: Build Target** command and select `TrafficSimTests`.
2. Open the **CMake** view, expand **Tests**, and run a test or the full test suite.
3. Use the test results view to inspect failures and output.

The tests cover road parsing, coordinate validation, route-based car creation, fixed-step simulation movement, and `overpass_default_query`. The Overpass integration test issues a live request and needs network access, so it can fail when the service is unavailable.

## Qt deployment

For a self-contained Windows Release build, use the `windeployqt` tool from the Qt installation that matches the selected compiler and build:

```powershell
C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe `
  --release `
  --qmldir .\src\gui `
  .\build\Release\TrafficSim.exe
```
