# Performance and Bug Audit

Scope: everything under `qml/`, the C++ bridge files (`Bridge.*`, `ServerProbe.*`, `main.cpp`), `CMakeLists.txt`, and `tools/*.py`.
The `build/` folder was skipped since it is generated output, not source.
This is a design-stage QML app (per project convention, no C++ wiring or app runs were done here), so this is a static read-through, not a profiler trace.

Total source size is small, about 4,800 lines across 26 QML files and 5 C++ files, so most items below are "will bite you once real data and real image sizes show up," not "already slow today."

## How to read this

Each item has a file and line number, what triggers it, why it costs what it costs, and a fix.
Items are grouped as performance (things that will make the app feel slow or use too much memory) and bugs (things that will produce a wrong result or a broken state).

---

## 1. Performance

### 1.1 Histogram hover picking scans every point on every mouse move

`qml/HistogramPlotView.qml:67-87` (`nearestPoint`), wired up at `qml/HistogramPlotView.qml:355` (`onPositionChanged: point = root.nearestPoint(...)`).

Every time the mouse moves over a histogram plot, `nearestPoint` loops over every curve and every point in that curve, doing a distance check on each one.
Each curve currently holds 880 points (`qml/HistogramPanel.qml:158`, `count = 880`), and up to 8 curves can be selected at once, so a single mouse move can trigger roughly 7,000 distance calculations, each with two divisions inside `toPxX`/`toPxY`.
Mouse move events fire very often (a smooth trackpad or a 1000Hz mouse can send hundreds per second), so this runs on nearly every frame while the cursor is over the plot.

Think of it like re-counting every grain of rice in a bowl each time someone taps the table near it, instead of only checking the handful of grains closest to the tap.

Fix: since curve points are already sorted by `x`, binary-search (or just index straight in, since `x` is evenly spaced) to the nearby points instead of a full linear scan.
That turns ~7,000 checks into a handful.

### 1.2 Histogram curves are regenerated with per-point trig math, and rebuilt on any selection change

`qml/HistogramPanel.qml:151-168` (`sampleCurve`).

This function builds an 880-point array from scratch, calling `Math.pow` and `Math.sin` for every point, and it currently stands in for real histogram data (there's a comment above `curveShapes` at line 52 noting this is "the expensive part").
It reruns whenever `curveSelection` changes, i.e. any time a direction is toggled on or off, for every curve in the new selection, not just the one that changed.
With two `HistogramPanel` instances on screen at once (`qml/Main.qml:126-137`) plus their pop-out windows sharing the same `curveShapes`, this is real but currently bounded (max 16 curves x 880 points).

This is placeholder data standing in for the eventual real histogram, so it is worth flagging now rather than after real image data (which could be much larger than 880 samples) is wired in: at that point this should move to C++ / typed arrays instead of building arrays of `{x, y}` JS objects, which are expensive for the JS engine to allocate and garbage-collect.

### 1.3 Magnifier loupe can request an image far bigger than any GPU texture allows

`qml/ImagePreview.qml:216-225`.

The loupe magnifier draws a second `Image` sized `sourceWidth * pixelScale` by `sourceHeight * pixelScale`, where `pixelScale = zoom * 18` (`loupeMagnification`, line 27).
For a placeholder image this stays small, but for a real camera frame (the sample generator in `tools/dummy_image.py` makes 1600x1200 already) at zoom 1, that's a requested image size of 28,800 x 21,600 pixels.
Most GPUs cap texture size at 8,192 or 16,384 pixels per side, so this will either silently fail to render, or force Qt into an expensive software-scaled fallback.

Fix: don't scale the `Image` item itself. Keep the loupe `Image` at a fixed on-screen size and use a shader/`transform: Scale` or, simpler, use the same source at a fixed `sourceSize` and pan/zoom via `Image.PreserveAspectCrop` plus a `Scale` transform on a small viewport, so the pixel count Qt actually rasterizes stays bounded.

### 1.4 Preview images never set `sourceSize`, so the full-resolution image is always decoded

`qml/ImagePreview.qml:50-59` and the loupe `Image` at line 218.

Neither `Image` element sets `sourceSize`, which means Qt decodes the source file at its native resolution even though it's then displayed shrunk to fit a small panel.
For placeholder 1600x1200 PNGs this is minor, but for real camera captures (which are often several megapixels), every preview, every magnifier, and every monitor slot thumbnail will decode the full frame just to show a thumbnail-sized image.

Fix: bind `sourceSize` to the panel's own pixel size (e.g. `sourceSize: Qt.size(width, height)`), so Qt's image loader downsamples during decode instead of after.

### 1.5 The magnifier window and its `ImagePreview` are always built, even when never opened

`qml/CameraPanel.qml:289-316`.

The `magnifier` `Window` is a direct child, not behind a `Loader`, and its inner `ImagePreview` binds `source: preview.source` unconditionally.
Because Qt Quick builds an item's full object tree (and starts loading bound image sources) as soon as the parent scene graph is created, this second full-resolution image load happens immediately at startup, and again every time the main image changes, whether or not the user ever clicks "Magnify."

The same pattern shows up for `MonitorViewerWindow` (`qml/Main.qml:204`) and `MonitorDirectionDialog` (nested inside it): both are built and kept alive for the app's whole lifetime instead of created on first use.

Fix: wrap rarely-opened windows in a `Loader { active: false }` that flips `active: true` the first time they're requested, so their (potentially expensive) contents don't exist until needed.

### 1.6 Build type is never pinned, so a plain build can end up unoptimized

`CMakeLists.txt:1-64`.

There is no `CMAKE_BUILD_TYPE` set, so with a single-config generator (Makefiles, Ninja) an unspecified build defaults to no optimization flags at all, which is slower than even a normal Debug build in most toolchains.
Since this is a Qt Quick app, an unoptimized build also disables Qt's own release-only fast paths (e.g. QML JIT tiering, some scene graph batching).
Any "is this actually fast" testing should be done with `-DCMAKE_BUILD_TYPE=Release`, and it's worth adding a default in `CMakeLists.txt` so a plain `cmake -B build` doesn't silently produce the slowest possible binary.

---

## 2. Bugs / error-prone code

### 2.1 Hardcoded machine-specific absolute path

`qml/Main.qml:57`.

```
singlePath: "/Users/jonathanhelga/Desktop/Fisheye_Calibration-Jojo_Version/tools/sample_shot.png"
```

This only exists on the machine and folder path it was written on.
Rename the project folder, or open it on a different machine (or a teammate's checkout), and the Camera panel starts with a missing image instead of the sample shot.
This should point at a path relative to the app (a Qt resource, or a path built from `applicationDirPath()`), not a literal absolute path.

### 2.2 `"file://" + path` breaks on Windows-style paths

`qml/CameraPanel.qml:211`, `qml/MonitorSlotPanel.qml:72`, `qml/LiveCameraPanel.qml:125`.

All three build an image URL by string-concatenating `"file://"` onto a raw filesystem path.
This happens to work for POSIX-style paths (`/Users/...`), but a Windows path like `C:\Users\jonathan\shot.png` turns into the malformed URL `file://C:\Users\jonathan\shot.png`, which `Image` will fail to load.
Even on macOS/Linux, any path containing a space or a `#` character will also break, since raw concatenation does no URL-escaping.

Fix: use `Qt.resolvedUrl(path)` (if `path` is already a file path) or, better, do the conversion once in C++ with `QUrl::fromLocalFile(path)` and expose that as the value QML binds to.

### 2.3 Auto-assigned curve colors are positional, not stable per curve

`qml/HistogramPanel.qml:38-46` (`curveSelection`).

When no comparison direction is active, each selected curve's automatic color comes from `Theme.curvePalette[k % length]`, where `k` is that curve's position in the current selection array.
Toggling a different, unrelated direction on or off shifts every later curve's index by one, which reassigns its color even though the user never touched it.
From the user's side, an already-picked curve appears to randomly change color whenever they add or remove a different one.

Fix: derive the palette index from something stable per curve, e.g. a hash of `direction + side`, or the fixed position of that direction in `directionOrder`, instead of its position within the currently-selected subset.

### 2.4 ROS mode has no way out of "Checking"

`qml/ServerConfigPanel.qml:200-217` (the `Update` button's `onClicked`).

In ROS mode, clicking "Update" sets `rosAxisStatus`/`rosMonitorStatus`/`rosCameraStatus` to `ServerProbe.Checking` and emits `rosUpdateRequested`, but nothing in the QML layer ever moves those three properties back to `Ok`/`Failed`.
Right now there's no C++ ROS bridge listening for that signal, so this is expected to be unfinished, but it's worth flagging clearly here: once real ROS wiring lands, whatever handles `rosUpdateRequested` must eventually set those three status properties, or every ROS status dot will sit on "Checking" forever after the first click. This is exactly the kind of silent stuck-state that's easy to ship by accident.

### 2.5 `Bridge` is registered but never used

`main.cpp:5`, `Bridge.h`, `Bridge.cpp`.

`Bridge` is included in `main.cpp` and compiled into the module, but nothing in `qml/` references it (confirmed by search: the only match for "Bridge" outside its own files is the `#include`).
It's a harmless leftover scaffold (a `clickCount` counter) rather than a bug, but if it's not going to be the eventual QML/C++ handshake point, it's worth deleting so it doesn't get mistaken for a wired-up feature later.

---

## 3. Suggested priority order

1. Fix 1.3 (loupe texture size) and 2.1 (hardcoded path) first. Both are cheap to fix and 1.3 can crash or blank-render the magnifier on a real camera frame, not just "run slow."
2. Fix 1.4 and 1.5 together (`sourceSize` + lazy windows) since they're the same underlying habit, image objects should be created and sized for where they're actually shown, not for their source resolution.
3. Fix 2.2 (`file://` concatenation) before this app is ever run on Windows or with paths containing spaces.
4. Treat 1.1 and 1.2 as "fix before real histogram data lands," since with only 8 synthetic curves the app should stay usable today, but real per-image histograms may be denser and this will get worse silently.
5. 2.3 and 2.4 are correctness/UX issues worth a quick pass whenever those panels get their next real pass.
