# Worldbilder

A small Qt6 world builder and map viewer. Draw land, countries, terrain,
regions, roads and rivers with polygon control points, then explore the map
with click-to-zoom previews.

## Build

Requirements: CMake 3.21+, a C++20 compiler and Qt6 Widgets.

```sh
cmake -S . -B build
cmake --build build
./build/worldbilder
```

## Using it

Two modes, switched in the toolbar or with `F1` / `F2`:

- **Explore** - drag to pan, wheel to zoom. Click a country or region to zoom
  into it; regions and settlements appear once you are zoomed in. Click a
  settlement for its details. Wheel and the toolbar zoom out again; `F` fits
  the whole world.
- **Edit** - pick a tool and click map points; double-click, right-click or
  `Enter` closes the polygon, `Esc` cancels. Middle-drag pans while editing.
  Near a corner or border the cursor shows **click to snap** and uses that
  point. Regions are clipped to their country. `Ctrl+Z` / `Ctrl+Shift+Z` undo
  and redo map changes. With **Select / Move** active, click an item to select
  it, drag the item to move it, or drag one of its control points to reshape
  it (a point snaps to nearby borders and slides along them). `Esc` cancels a
  drag.

Tools and shortcuts:

| Tool | Key | Notes |
| --- | --- | --- |
| Select / Move | `M` | Click an item, drag it or one of its control points. |
| Draw Land | `L` | Adds a land polygon on the sea. |
| Country | `C` | Asks for a name, assigns a colour. |
| Terrain Area | `T` | Uses the terrain selected in the toolbar combo. |
| Region | `R` | Must be drawn inside a country. |
| Settlement | `S` | Click a point, then choose city, town or village and a name. |
| Road | `D` | Click points; terrain under the road weights its travel cost. |
| River | `V` | Click points; roads crossing a river are drawn with bridges. |

The **Info** dock shows the selected feature. **Edit...** changes its name and
description; renaming a country updates its regions. When a country or land
area is selected, **Add Random Regions...** / **Add Random Countries...** (or
`Generate > Fill Selected Area...`) fills the leftover space around the
existing children with a chosen number of non-overlapping pieces. Existing
regions and countries are kept. Visiting a settlement in Explore mode adds a
line to the **Travel Log** dock; `File > Export Travel Log...` or the dock's
**Export...** button saves it as a text file.

In Explore mode, click a settlement to start travelling. Click another
settlement, region or country to walk there; the log shows how many days the
terrain-weighted journey took (roughly 30 map units per day on grassland).

`File > New World...` starts from sea or a big land area. `File > Resize
World...` extends the map; features stay where they are. `Generate > Random
Additions...` adds a chosen number of land, countries, regions, terrain or
settlements for quick maps.

## World files

- `.world` - zlib-compressed JSON.
- `.json` - the same JSON, plain and readable.

`Save As...` lets you pick either extension the first time. Loading accepts
both. There is deliberately no backwards compatibility yet; invalid files
report what went wrong.

## Source layout

- `src/MapFeature.*` - map object (land, country, terrain, region, road, river,
  settlement), painting and JSON serialization.
- `src/MapView.*` - QGraphicsView canvas: pan, zoom, draft polygons, clicks.
- `src/MainWindow.*` - tools, dialogs, info dock, save/load.

Open work is tracked in `TODO.md`.
