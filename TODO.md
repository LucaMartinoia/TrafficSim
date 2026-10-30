# Project plan

1. Write algorithm for spawning/despawning cars inside a given bbox. Each car gets a starting and ending point.
2. Implement Dijkstra algorithm to compute the shortest path for each car.
3. Implement algorithm to update car position along the Dijkstra path.
4. Scale car speed with traffic and road properties.
5. Connect the callback between QML and C++.
6. Implement flow-like traffic representation instead of agent-based mean-field representation.
7. Implement toggles to scale the number of created cars or other parameters.
8. Implement window menu to toggle between distance metric, time metric or adaptive time metric (which considers traffic) in computing Dijkstra.
9. Add search bar to the GUI to find locations fast, but also button "center to default".
10. Implement a Theme.qml to style buttons and all the rest.

Remember to remove qInfo() debug calls.
