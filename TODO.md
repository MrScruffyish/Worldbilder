Overall rule: Do everything very simple at first, no backwards compatability, commit with terse messages when one task is done. Avoid tests for now.

# TODO
3. You should be able to draw a map (like drawing a polygon or mixed with freehand (better use controlpoints?))
4. You can create countries.
5. You can place different terrain in areas.
6. You can subdivide countries into regions.
7. You can place cities, towns, villages on map
8. You can name countries, regions and towns etc.
9. You can preview the map, and click on things. And zoom into stuff (like, click on contry, and it zooms in and start to show subparts of it (regions/towns)). And zoom out of course. Not sure about UI, should you scroll with wheel?
10. you can save worlds  fileextension: .world  (probably a gzip'ed json file?)
11. You can load a world. Ignore backwards compatability for now and just write what the problem was.
12. You can write descriptions for items on the map.
13. You can resize the map as needed (ie make larger to put more on it.)
14. the program should also be able to work with .json files (when saving first time, let choose between .world or .json)

# DONE
1. A Qt6 program where you can travel on the map (2d map) or create a map (2d map).
2. Start with either sea or big land area.
