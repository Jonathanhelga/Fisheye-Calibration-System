# Fisheye Calibration - Jojo Version

Ground-up QML rebuild of the calibration client that lives at
`../moil-fisheye-calibration-system/cpp`.
That app is QWidget + `.ui` files.
This one is Qt Quick (QML) from the start, not a widget host with QML bolted on.

## What is here

- `main.cpp` boots a `QQmlApplicationEngine` and exposes one `Bridge`
  (`Bridge.h` / `Bridge.cpp`) into QML as the C++/QML touchpoint.
  Grow `Bridge` or split it into one bridge per panel as real panels get built.
- `qml/Main.qml` is a wireframe of the old app's panel layout: HTTP Server URL,
  Axis Control Panel, Camera Panel, Centering, Monitor/Pattern, Calibration
  Result, and the two histograms.
  Every box is a `PanelPlaceholder` right now.
- `qml/HistogramPanel.qml` replaces the old app's two hand-copied
  "Histogram1"/"Histogram2" blocks with one component parameterized by
  `channel`.
  This is the pattern to follow for every other panel: one `.qml` file,
  instantiated wherever it is needed.

## Build (macOS)

```bash
/opt/homebrew/bin/cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6"
/opt/homebrew/bin/cmake --build build -j6
./build/fisheye_cali_jojo
```

## Porting a panel

1. Create `qml/<PanelName>.qml`.
2. Add it to `QML_FILES` in `CMakeLists.txt`.
3. Swap the matching `PanelPlaceholder { ... }` in `Main.qml` for the real component.
4. If the panel needs live data (axis position, camera image, calibration
   results), add the state to `Bridge` (or a new panel-specific bridge class)
   as `Q_PROPERTY`/`Q_INVOKABLE`, the same way `ControllerMain` in the old
   app owns that state today.
# Fisheye-Calibration-System
