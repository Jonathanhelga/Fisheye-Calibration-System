# Performance and Bug Audit

Scope: everything under `qml/`, the C++ files (`Bridge.*`, `ServerProbe.*`, `main.cpp`), `CMakeLists.txt`, and `tools/*.py`.
The `build/` folder was skipped since it is generated output, not source.

Originally written 2026-08-20 against the flat `qml/` layout.
Re-verified 2026-08-26 against the current `qml/windows` + `qml/panels` + `qml/controls` tree, with line numbers and fix status brought up to date.

Total source size is about 7,500 lines across 42 QML files and 5 C++ files.
This is a static read-through, not a profiler trace, so most items below are "will bite you once real data and real image sizes show up," not "already slow today."

## Status at a glance

| Item | What | State |
|---|---|---|
| 1.1 | Histogram hover picking scans every point | open |
| 1.2 | Histogram curves rebuilt with per-point trig | open |
| 1.3 | Loupe requests an oversized texture | **fixed** |
| 1.4 | Previews never set `sourceSize` | open |
| 1.5 | Rarely-opened windows built eagerly | open |
| 1.6 | `CMAKE_BUILD_TYPE` never pinned | open |
| 2.1 | Hardcoded machine-specific path | **fixed** |
| 2.2 | `"file://" + path` breaks on Windows and on spaces | open |
| 2.3 | Curve colors are positional, not stable | open |
| 2.4 | ROS mode has no way out of "Checking" | open |
| 2.5 | `Bridge` is registered but never used | open |

## How to read this

Each item has a file and line number, what triggers it, why it costs what it costs, and a fix.
Items are grouped as performance (things that will make the app feel slow or use too much memory) and bugs (things that will produce a wrong result or a broken state).

---

## 1. Performance

### 1.1 Histogram hover picking scans every point on every mouse move

`qml/controls/HistogramPlotView.qml:135` (`nearestPoint`), wired up at `qml/controls/HistogramPlotView.qml:449` (`onPositionChanged`).

Every time the mouse moves over a histogram plot, `nearestPoint` loops over every curve and every point in that curve, doing a distance check on each one.
Each curve holds 880 points (`qml/panels/HistogramPanel.qml:158`, `const count = 880`), and up to 8 curves can be selected at once, so a single mouse move can trigger roughly 7,000 distance calculations, each with two divisions inside `toPxX`/`toPxY`.
Mouse move events fire very often (a smooth trackpad or a 1000Hz mouse can send hundreds per second), so this runs on nearly every frame while the cursor is over the plot.

Think of it like re-counting every grain of rice in a bowl each time someone taps the table near it, instead of only checking the handful of grains closest to the tap.

Fix: since curve points are already sorted by `x`, binary-search (or just index straight in, since `x` is evenly spaced) to the nearby points instead of a full linear scan.
That turns ~7,000 checks into a handful.

### 1.2 Histogram curves are regenerated with per-point trig math, and rebuilt on any selection change

`qml/panels/HistogramPanel.qml:151` (`sampleCurve`).

This function builds an 880-point array from scratch, calling `Math.pow` and `Math.sin` for every point, and it currently stands in for real histogram data.
It reruns whenever `curveSelection` changes, meaning any time a direction is toggled on or off, for every curve in the new selection, not just the one that changed.
With two `HistogramPanel` instances on screen at once (`qml/windows/Main.qml:128` and `:134`) plus their pop-out windows sharing the same `curveShapes`, this is real but currently bounded (max 16 curves x 880 points).

Note that a partial fix already landed: `curveShapes` was split so that a color-only edit no longer re-runs `sampleCurve` (see the comments at `qml/panels/HistogramPanel.qml:31` and `:64`).
Toggling a direction still rebuilds everything.

This is placeholder data standing in for the eventual real histogram, so it is worth flagging now rather than after real image data is wired in.
At that point it should move to C++ / typed arrays instead of building arrays of `{x, y}` JS objects, which are expensive for the JS engine to allocate and garbage-collect.

### 1.3 Loupe magnifier texture size (fixed)

`qml/controls/ImagePreview.qml:218-229`.

The original finding was that the loupe drew a second `Image` sized `sourceWidth * pixelScale` by `sourceHeight * pixelScale`.
With `pixelScale = zoom * 18` and a 1600x1200 frame, that asked for a 28,800 x 21,600 texture, well past the 8,192 or 16,384 cap most GPUs enforce.

This is now fixed the way the note recommended.
The `Image` is sized at `root.sourceWidth` by `root.sourceHeight`, meaning source resolution, and the magnification is applied as `scale: loupe.pixelScale` with `transformOrigin: Item.TopLeft`.
A `scale` is a transform on an already-rasterized item, so the texture Qt actually allocates stays bounded by the source size no matter how far you zoom.

Kept here as a record of the reasoning, since the same trap applies anywhere else a zoomed view gets built.

### 1.4 Preview images never set `sourceSize`, so the full-resolution image is always decoded

`qml/controls/ImagePreview.qml:50` (the main image) and `:218` (the loupe image).

Neither `Image` element sets `sourceSize`, which means Qt decodes the source file at its native resolution even though it is then displayed shrunk to fit a small panel.
For the 1600x1200 placeholder this is minor, but for real camera captures (the rig's sensor is 3040x3040, see `BACKEND_INTEGRATION.md`), every preview, every magnifier, and every monitor slot thumbnail will decode the full frame just to show a thumbnail-sized image.

Note the two images want opposite treatment.
The main preview should set `sourceSize` to its own pixel size so Qt downsamples during decode.
The loupe deliberately wants full resolution, since showing real pixels is the entire point of a magnifier, so it should be left alone or given an explicit cap.

### 1.5 Rarely-opened windows and their contents are always built

`qml/panels/CameraPanel.qml:293` (the `magnifier` window), plus `qml/windows/Main.qml:204` (`MoilCalibrationResult`) and `:208` (`PatternAndMonitor`).

The `magnifier` `Window` is a direct child, not behind a `Loader`, and its inner `ImagePreview` binds `source: preview.source` unconditionally.
Because Qt Quick builds an item's full object tree (and starts loading bound image sources) as soon as the parent scene graph is created, this second full-resolution image load happens immediately at startup, and again every time the main image changes, whether or not the user ever clicks Magnify.

`MoilCalibrationResult` and `PatternAndMonitor` are the same pattern at a larger scale.
Both are entire windows, each with its own panel tree, built and kept alive for the app's whole lifetime instead of created on first use.

Fix: wrap rarely-opened windows in a `Loader { active: false }` that flips `active: true` the first time they are requested, so their contents do not exist until needed.

### 1.6 Build type is never pinned, so a plain build can end up unoptimized

`CMakeLists.txt`.

There is no `CMAKE_BUILD_TYPE` set, so with a single-config generator (Makefiles, Ninja) an unspecified build defaults to no optimization flags at all, which is slower than even a normal Debug build in most toolchains.
Since this is a Qt Quick app, an unoptimized build also disables Qt's own release-only fast paths, such as QML JIT tiering and some scene graph batching.

Any "is this actually fast" testing should be done with `-DCMAKE_BUILD_TYPE=Release`, and it is worth adding a default in `CMakeLists.txt` so a plain configure does not silently produce the slowest possible binary.

---

## 2. Bugs / error-prone code

### 2.1 Hardcoded machine-specific absolute path (fixed)

`qml/windows/Main.qml:58-59`.

This used to read:

```qml
singlePath: "/Users/jonathanhelga/Desktop/Fisheye_Calibration-Jojo_Version/tools/sample_shot.png"
```

which only existed on one machine and one folder path.

It is now:

```qml
singlePath: "qrc:/qt/qml/FisheyeCaliJojo/assets/sample_shot.png"
```

backed by `assets/sample_shot.png` in the `RESOURCES` block of `CMakeLists.txt`.
The image is compiled into the binary, so it resolves identically on the MacBook and the miniPC.
This is what commit `b012c3f` was for.

### 2.2 `"file://" + path` breaks on Windows-style paths

`qml/panels/CameraPanel.qml:39`, `qml/panels/MonitorSlotPanel.qml:72`, `qml/panels/LiveCameraPanel.qml:13`.

All three build an image URL by string-concatenating `"file://"` onto a raw filesystem path.
This happens to work for POSIX-style paths, but a Windows path like `C:\Users\jonathan\shot.png` turns into the malformed URL `file://C:\Users\jonathan\shot.png`, which `Image` will fail to load.
Even on macOS and Linux, any path containing a space or a `#` character will also break, since raw concatenation does no URL-escaping.

This matters more than it did when the note was first written.
The rig is a Windows machine (see `BACKEND_INTEGRATION.md`), so real capture paths coming back from it will be Windows-shaped.

Fix: use `Qt.resolvedUrl(path)`, or better, do the conversion once in C++ with `QUrl::fromLocalFile(path)` and expose that as the value QML binds to.

### 2.3 Auto-assigned curve colors are positional, not stable per curve

`qml/panels/HistogramPanel.qml:32-45` (`curveSelection`).

When no comparison direction is active, each selected curve's automatic color comes from `Theme.curvePalette[k % Theme.curvePalette.length]`, where `k` is that curve's position in the current selection array.
Toggling a different, unrelated direction on or off shifts every later curve's index by one, which reassigns its color even though the user never touched it.
From the user's side, an already-picked curve appears to randomly change color whenever they add or remove a different one.

Fix: derive the palette index from something stable per curve, such as the fixed position of that direction in `directionOrder`, instead of its position within the currently-selected subset.

### 2.4 ROS mode has no way out of "Checking"

`qml/panels/ServerConfigPanel.qml:206-217` (the Update button's `onClicked`).

In ROS mode, clicking Update sets `rosAxisStatus` / `rosMonitorStatus` / `rosCameraStatus` to `ServerProbe.Checking` and emits `rosUpdateRequested`, but nothing ever moves those three properties back to `Ok` or `Failed`.
Grepping `qml/` finds zero handlers for that signal, and `qml/windows/Main.qml:51` instantiates the panel bare as `ServerConfigPanel { Layout.fillWidth: true }`.

Note that ROS is the **default** mode (`currentIndex: root.modeRos` at `qml/panels/ServerConfigPanel.qml:67`), so this is the path a user lands on first, not an edge case.

This is expected to be unfinished, since there is no C++ ROS bridge yet, and `BACKEND_INTEGRATION.md` explains why that work is blocked.
It is flagged here because whatever eventually handles `rosUpdateRequested` must set those three status properties, or every ROS status dot sits on Checking forever after the first click.
That is exactly the kind of silent stuck-state that is easy to ship by accident.

### 2.5 `Bridge` is registered but never used

`main.cpp:5`, `Bridge.h`, `Bridge.cpp`.

`Bridge` is included in `main.cpp` and compiled into the module, but nothing in `qml/` references it.
It is a harmless leftover scaffold (a `clickCount` counter) rather than a bug, but if it is not going to be the eventual QML/C++ handshake point, it is worth deleting so it does not get mistaken for a wired-up feature later.


---

## 3. Suggested priority order

1. Fix 2.2 (`file://` concatenation) first.
   It is cheap, and it is the one that will actually fail against the real Windows rig rather than in theory.
2. Fix 1.4 and 1.5 together (`sourceSize` plus lazy windows), since they are the same underlying habit: image objects should be created and sized for where they are actually shown, not for their source resolution.
3. Treat 1.1 and 1.2 as "fix before real histogram data lands."
   With only 8 synthetic curves the app stays usable today, but real per-image histograms may be denser and this gets worse silently.
4. 2.3 and 2.4 are correctness and UX issues worth a quick pass whenever those panels get their next real pass.
5. 1.6 is a one-line change to `CMakeLists.txt` and should happen before anyone measures anything.

## See also

- `QML_LINT_DEBT.md` for the separate list of `qmllint` findings, which does not overlap with this one.
- `BACKEND_INTEGRATION.md` for why the ROS side of 2.4 is blocked.
