# TrafficSim

TrafficSim is an experimental traffic simulator and a learning project for C++, Qt Quick, and QML. It loads real road layouts from OpenStreetMap, builds a routable road network, and animates simulated cars on a map.

The project is intentionally focused on exploring how a desktop application can connect a QML interface, geographic data, networking, and a C++ simulation model. Its traffic behavior is a simplified model, not a prediction of real-world traffic.

## What you can do

- Explore a map and search for an address
- Save and restore a default map location and zoom level
- Load roads for the visible map area and view them on the map
- Start and stop a simulation with cars following routes through the road network
- Choose a routing preference and adjust the target traffic level
- Read application and simulation messages in the console
- Reuse cached road data or clear the cache from Settings

## How the application works

1. **Choose an area.** The map opens at the saved default view, if one is available. You can pan and zoom, search for an address, or return to the saved view.
2. **Request road data.** When you start a simulation, the interface sends the currently visible map bounds to the C++ backend. The road-data client checks its local cache and otherwise requests road geometry from the OpenStreetMap Overpass API. If an endpoint is unavailable, it tries another one.
3. **Prepare the road network.** The backend validates the response and turns the road geometry into a directed network. Road direction and other available road properties are used to prepare routes and travel estimates. The road layout is sent to the map for display.
4. **Run the simulation.** The simulation creates trips between reachable points in the network, moves cars along their routes, and tracks road usage. The selected routing preference and traffic target influence the simulation.
5. **Update the map.** The backend sends car positions and road data to the QML interface, which updates the map and console. Stopping clears the current simulation and cancels any in-progress road request.

## Main components

- **QML interface (`src/gui`)** — map, search and navigation controls, toolbar, settings, and console
- **`Backend`** — connects the interface to road-data requests, settings, and the simulation
- **`OSMClient`** — requests and caches road data from Overpass
- **`RoadNetwork`** — validates road geometry, represents roads as a routable network, and finds routes
- **`Simulation` and `Traffic`** — manage the simulation lifecycle, trips, car movement, and road usage
- **`Settings`** — stores the default map view using Qt application settings
- **`GeoUtils`** — shared geographic validation and coordinate operations

## Build

The project uses CMake and requires CMake 3.20 or newer, a C++20-compatible compiler, and Qt 6 with the Quick, QML, Quick Controls 2, Graphs, Location, Positioning, and Network modules. Building the tests also requires Qt Test.

If CMake cannot find your Qt installation, set `CMAKE_PREFIX_PATH` to the Qt kit that matches your compiler.

```powershell
cmake -S . -B build
cmake --build build --config Release --target TrafficSim
```

In VS Code, the CMake Tools and C/C++ extensions can be used to select a compiler kit, configure the project, and build the `TrafficSim` target.

## Tests

Build and run the test suite with:

```powershell
cmake --build build --config Release --target TrafficSimTests
ctest --test-dir build -C Release --output-on-failure
```

The tests cover road-network parsing, geographic input validation, route-based car creation, and simulation movement. The Overpass integration test makes a live network request and may fail if the service or internet connection is unavailable.

## Windows deployment

For a self-contained Windows build, run `windeployqt` from the Qt kit used to build the application:

```powershell
<Qt-kit>\bin\windeployqt.exe `
  --release `
  --qmldir .\src\gui `
  .\build\Release\TrafficSim.exe
```
