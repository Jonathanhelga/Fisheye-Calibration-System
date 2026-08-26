# Fisheye Calibration - Jojo Version

Ground-up QML rebuild of the calibration client that lives at
`../moil-fisheye-calibration-system/cpp`.
That app is QWidget + `.ui` files.
This one is Qt Quick (QML) from the start, not a widget host with QML bolted on.

## What is here

- `main.cpp` boots a `QQmlApplicationEngine`, picks the Basic control style,
  and loads `Main` from the `FisheyeCaliJojo` module.
  `ServerProbe.h` / `ServerProbe.cpp` backs the reachability checks behind the
  connection status dots, and is the only C++ QML currently uses.
  `Bridge.h` / `Bridge.cpp` is registered but not yet wired to anything; it is
  the intended C++/QML touchpoint, to grow or split per panel as panels get built.
- `qml/` holds every QML source, split three ways:
  - `qml/windows/` are the top-level `ApplicationWindow`s: `Main.qml`,
    `PatternAndMonitor.qml`, `MoilCalibrationResult.qml`.
  - `qml/panels/` are the big feature blocks that fill those windows, one file
    per panel.
  - `qml/controls/` are the small reusable pieces the panels are built from,
    such as `ActionButton`, `ValueField`, and `SegmentedControl`.
- `qml/Theme.qml` is a QML singleton holding the shared colors, spacing, and
  font sizes. Everything else reads from it instead of hardcoding values.
- `qml/panels/HistogramPanel.qml` replaces the old app's two hand-copied
  "Histogram1"/"Histogram2" blocks with one component parameterized by
  `channel`.
  This is the pattern to follow for every other panel: one `.qml` file,
  instantiated wherever it is needed.

Every `.qml` file has to be listed in `QML_FILES` in `CMakeLists.txt`, which
compiles them into the binary rather than reading them from disk at runtime.

## Documentation

| Doc | What it is for |
|---|---|
| `RUNNING.md` | building and running on the MacBook and the miniPC |
| `PCT_AND_ICT.md` | what the calibration numbers mean, in plain language |
| `PATTERN_GENERATOR.md` | the Pattern Generator spec we are porting from |
| `BACKEND_INTEGRATION.md` | ROS 2 from zero, and why talking to the rig is currently blocked |
| `PERFORMANCE_NOTES.md` | open performance and correctness findings |
| `QML_LINT_DEBT.md` | open `qmllint` findings |
| `tools/DUMMY_SERVER.md` | faking the three services so the connection dots can be tested |

## Build

```bash
# macOS
/opt/homebrew/bin/cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6"
/opt/homebrew/bin/cmake --build build -j6
./build/fisheye_cali_jojo
```

The project is developed on two machines.
See `RUNNING.md` for the miniPC (Ubuntu) build, which needs an explicit Qt prefix, and for what to re-run after what.

## Porting a panel

1. Create `qml/panels/<PanelName>.qml`.
2. Add it to `QML_FILES` in `CMakeLists.txt`.
3. Swap the matching placeholder in the relevant window for the real component.
4. If the panel needs live data (axis position, camera image, calibration
   results), add the state to `Bridge` (or a new panel-specific bridge class)
   as `Q_PROPERTY`/`Q_INVOKABLE`, the same way `ControllerMain` in the old
   app owns that state today.
