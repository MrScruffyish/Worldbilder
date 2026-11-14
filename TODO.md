Overall rule: Do everything very simple at first, no backwards compatability, commit with terse messages when one task is done. Avoid tests for now.

# TODO


# Things to ignore for now.
Add a tool for smoothing the map (i.e. a little bit more smoth lines but still with som wiggle so it does not look like a triangle or square, but more a "natural" shape with some bends or shapes along the line.

# DONE
1. A Qt6 program where you can travel on the map (2d map) or create a map (2d map).
2. Start with either sea or big land area.
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
15. Indicate corners of regions with some small dots.
16. "snap" to lines and points in a resonable way. If I start with sea for eaxample, and draw a landmass (country), then tries to place a region in it, it should snap to the borders of the region, and not extend outside of it. Perhaps if Im close with mouse pointer it could display "click to snap" or something?
17. Undo/Redo options.
18. When drawing a country on a landmass, the last point should probably be equal to the starting point. Figure out if it was a fairly convex-shaped, then you can figure out how the border should fit to the underlaying item. Like following the coast line.
19. When drawing land on top of other land, merge them to one part or ask if it was supposed to be something else. A region?
20. Add a dialog to add stuff "with buttons: add X random item Y " for each  sensible Y. so one can quickly generate a map.
21. Add roads (either freehand draw or polygon) and take in to account terrain. some road can go in hills, some not has to make detour. 
22. Rivers would be nice to draw. They are also passible by bridges.
23. When you explore. Write a travel log in a window (exportable/saveable as test)
24. When exploring in a town, you can click on another town/region/something distinct to travel there. Calculate the travel distance, weighted with terrain and update log, simple. like "walked 24 days, came to town XAA"
