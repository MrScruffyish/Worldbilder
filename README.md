# Worldbilder

A small Qt6 world builder and map viewer. Draw land, countries, terrain,
regions and settlements with polygon control points, then explore the map with
click-to-zoom previews.

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
  and redo map changes.

Tools and shortcuts:

| Tool | Key | Notes |
| --- | --- | --- |
| Draw Land | `L` | Adds a land polygon on the sea. |
| Country | `C` | Asks for a name, assigns a colour. |
| Terrain Area | `T` | Uses the terrain selected in the toolbar combo. |
| Region | `R` | Must be drawn inside a country. |
| Settlement | `S` | Click a point, then choose city, town or village and a name. |

The **Info** dock shows the selected feature. **Edit...** changes its name and
description; renaming a country updates its regions.

`File > New World...` starts from sea or a big land area. `File > Resize
World...` extends the map; features stay where they are.

## World files

- `.world` - zlib-compressed JSON.
- `.json` - the same JSON, plain and readable.

`Save As...` lets you pick either extension the first time. Loading accepts
both. There is deliberately no backwards compatibility yet; invalid files
report what went wrong.

## Source layout

- `src/MapFeature.*` - map object (land, country, terrain, region, settlement),
  painting and JSON serialization.
- `src/MapView.*` - QGraphicsView canvas: pan, zoom, draft polygons, clicks.
- `src/MainWindow.*` - tools, dialogs, info dock, save/load.

Open work is tracked in `TODO.md`.
