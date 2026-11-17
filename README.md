# Worldbilder

A small Qt6 world builder and map viewer. Draw land, countries, terrain,
regions, roads, railroads and rivers with polygon control points, then explore
the map with click-to-zoom previews.

## Build

Requirements: CMake 3.21+, a C++20 compiler and Qt6 Widgets.

```sh
make
./build/worldbilder
```

Or without the Makefile:

```sh
cmake -S . -B build
cmake --build build
./build/worldbilder
```

`make run` builds and starts the app. `make clean` cleans the build
directory; `make BUILD_DIR=build-debug` uses another build directory.

## Using it

Two modes, switched in the toolbar or with `F1` / `F2`:

- **Explore** - drag to pan, wheel to zoom; the cursor is a walking-legs
  symbol. Click a country or region to zoom
  into it; regions and settlements appear once you are zoomed in. Click a
  settlement for its details. Wheel and the toolbar zoom out again; `F` fits
  the whole world.
- **Edit** - pick a tool from the left palette and click map points;
  double-click, right-click or `Enter` closes the polygon, `Esc` cancels.
  Middle-drag pans while editing. Near a corner or border the cursor shows
  **click to snap** and uses that point. Regions are clipped to their country.
  `Ctrl+Z` / `Ctrl+Shift+Z` undo and redo map changes. With **Select / Move**
  active, click an item to select it, drag the item to move it, or drag one of
  its control points to reshape it (a point snaps to nearby borders and slides
  along them). Moving a point also moves every other feature point at the same
  spot and splits the border of any feature whose line passes through the
  point, so connected borders stay joined. Right-click a control point and choose
  **Replace Control Point**, then click the new position to move that point
  there; `Esc` cancels. Right-click a border between two control points and
  choose **Add Control Point** to insert a point there; the border is highlighted
  to show the new point snaps onto the line, and shared borders get the new
  point too. `Esc` also cancels a drag.

Tools are in the left palette; each has a shortcut.

| Tool | Key | Notes |
| --- | --- | --- |
| Select / Move | `M` | Click an item, drag it or one of its control points. |
| Draw Land | `L` | Adds a land polygon on the sea. |
| Country | `C` | Asks for a name, assigns a colour. |
| Terrain Area | `T` | Uses the terrain chosen in the left palette. |
| Region | `R` | Must be drawn inside a country. |
| Settlement | `S` | Places the type chosen in the left palette; rename it in the Info dock. |
| Road | `D` | Click points; terrain under the road weights its travel cost. |
| Railroad | `I` | Click points; drawn as a dark line with light ties. |
| River | `V` | Click points; roads and railroads crossing a river get bridges. |

The **Info** dock shows the selected feature. **Edit...** changes its name and
description; renaming a country updates its regions. The same dialog has tabs
for **History and facts** and **Culture**, where languages and characteristics
can be ticked. Edit > **Languages and Characteristics...** manages the global
choices; they are saved with the world. **Delete...** (or the
`Delete` key with Select / Move active) removes it. Deleting a country also
deletes its regions. For land, countries, regions and terrain areas you can
choose to let the bordering item with the longest shared border expand into
the vacated space. When a country or land
area is selected, **Add Random Regions...** / **Add Random Countries...** (or
`Generate > Fill Selected Area...`) fills the leftover space around the
existing children with a chosen number of non-overlapping pieces. Existing
regions and countries are kept. Corners of generated areas that land on a
border become control points on that border, so the pieces stay connected when
the border is edited. Visiting a settlement in Explore mode adds a
line to the **Travel Log** dock; `File > Export Travel Log...` or the dock's
**Export...** button saves it as a text file.

In Explore mode, click a settlement to start travelling. Click another
settlement, region or country to walk there; the log shows how many days the
terrain-weighted journey took (roughly 30 map units per day on grassland).

`File > New World...` starts from sea or a big land area. `File > Resize
World...` extends the map; features stay where they are. `Generate > Random
Additions...` adds a chosen number of land, countries, regions, terrain or
settlements for quick maps. Tick **Fill the target area completely** to tile
the selected (or first) land or country with the chosen number of countries,
regions or terrain areas instead of placing separate blobs. Random terrain
only meets compatible types, so desert and forest always keep grassland,
mountains or water between them. Glacier terrain forms next to mountains,
water or uncharted land. Set **Uncharted terrain** to a percentage to leave
that share of the generated area unknown; filled areas keep the uncharted part
in one large central block. **Add random settlements** fills the selected
region, or every region of a country, with roughly one city, a few towns and
more villages. Cities keep their distance from each other, some settlements
sit on the coast or just inside a border, and harsh terrain gets far fewer
settlements or none at all.

## World files

- `.world` - zlib-compressed JSON.
- `.json` - the same JSON, plain and readable.

`Save As...` lets you pick either extension the first time. Loading accepts
both. There is deliberately no backwards compatibility yet; invalid files
report what went wrong.

## Source layout

- `src/MapFeature.*` - map object (land, country, terrain, region, road, railroad,
  river, settlement), painting and JSON serialization.
- `src/MapView.*` - QGraphicsView canvas: pan, zoom, draft polygons, clicks.
- `src/MainWindow.*` - tools, dialogs, info dock, save/load.

Open work is tracked in `TODO.md`.
