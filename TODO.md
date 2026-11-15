Overall rule: Do everything very simple at first, no backwards compatability, commit with terse messages when one task is done. Avoid tests for now.

# TODO
31. should be possible to delete item and be asked: do you want to expand bordering items to fill the vacated space?
32. Should be possible to write some history, facts about items. what languages are talked there, etc. Add a few of these things, but not too much. It should also be possible to add global list of languages, characteristics to select from.
33. when exploring, should change the symbol to some walking legs or something?
34. Should be possible to add railroad also.
35. The settlement tool is cumbersome, perhaps not buttons at the top, but icons/buttons to the left like in many drawing programs, and you can select tool and subtool (like settlement -> town) and then that is what you place.
36. Add possibility to fill land with random terrain. but it cant be too random. a dessert can not have a forrest as neighbour, there need to be some intermediate thing.
37. Some terrain should be unchartered, the user should be able to change amount of unchartered. The unchartered areas should be like in a dessert, or large forrest, or mountains. but probably only in large areas. Or if there is supposed to be a lot unchartered, then perhaps fill the middle of the land with unchartered, like, dont even know if it is mountian or dessert or what.
38. Farming landscape is also a terrain type.
39. Glacier is also a terrain.
40. Some auto-fill with cities, towns, villages  (like for 1 city, 2-3 towns and 6-10 villages?) Cities are not supposed to end up too close to each other, otherwise fine to place stuff anywhere. Some stuff should be snapped to coast line. some few villages could be snapped to a region/country limit but clearly on one side of it.   Fill should be done per region or for a country (fill each region then). In harsh land, there should be much fewer settlements, if any.


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
25. Not sure about editing, I need to be able to select e.g. a country, and then add random regions to that. Or even edit it and say: add N random regions, they should not overlap, but fill the country with random drawn regions.
26. When starting, I get a rectangle green landmass with rounded corners. Dont use round shapes for land masses. 
27. It has to be clearer when editing, so I can select an item, like a region for example, or a river, or a city, and then move it, or move one of its controlpoints. If the point is snapped to something, it should move along that thing.
28. When selecting a country with some regions in it, or selecting a land with some countries in it, I need to be able to auto generate so the reminder of the item is filled  with countries or regions.
29. be  able to select a controlpoint and choose to replace it with a new control point (like you want to snap a point to something.)
30. When adding random items, there should be an  option to fill it completly with N random stuff.
