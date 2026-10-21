# Project plan

1. Implement a class to perform Overpass API queries on Open Street Map data.
2. Query OSM to gather, for a given bounding box, the roads and nodes information (geometries and tags).
3. From OSM data, reconstruct a sort of road network for the given bbox.
4. Plot the given network again the QML tilemap to see if spot error/see overlap.
5. Write algorithm for spawning/despawning cars inside a given bbox. Each car gets a starting and ending point.
6. Implement Dijkestra algorithm to compute the shortest path for each car.
7. Implement algorithm to update car position along the Dijkestra path.
8. Scale car speed with traffic and road properties.
9. Connect the callback between QML and C++.
10. Cache the OSM query.
11. Implement local OSM queries from protobuf.osm files (libosmium library).
12. Implement flow-like traffic representation instead of agent-based mean-field representation.
13. Implement graphs and toggle to scale the number of created cars or other parameters.

For future reference: this is NOT a production software or anything like that. The only goal of this project is for me to become a bit more comfortable with C++ and learn C++ and QML, while also developing something rather hard and hopefully fun/nice.
