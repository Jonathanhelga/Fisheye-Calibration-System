# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## What this repository is

A ground-up QML rebuild of the fisheye calibration client.
The reference implementation, `../moil-fisheye-calibration-system`, is the complete, working version of this app: QWidget + `.ui` files, built as `moilcali` from `moil-fisheye-calibration-system/cpp`.
This repo is Qt Quick (QML) from the start, not a widget host with QML bolted on, and its goals beyond the technology swap are to modernize the Qt setup and improve runtime performance versus the old app.

When porting a feature or panel, treat `../moil-fisheye-calibration-system/cpp` as the source of truth for behavior (controller logic, axis safety interlocks, calibration math), and `../moil-fisheye-calibration-system/CLAUDE.md` as the reference for how that old codebase is organized.

**Server communication: copy the ROS method, not HTTP.** The old client supports both HTTP and ROS device backends. Ours should follow the ROS path:
- Client-side ROS device clients: `../moil-fisheye-calibration-system/cpp/src/models/device/axis_ros_client.{h,cpp}`, `camera_ros_client.{h,cpp}`, `monitor_ros_client.{h,cpp}`.
- Message/service contracts: `../moil-fisheye-calibration-system/ros/moil_interfaces/msg/` and `srv/` (e.g. `AxisMove.srv`, `AxisSensor.srv`, `SetBrightness.srv`, `ShowPattern.srv`).
- The HTTP clients (`axis_http_client.*` etc.) in the same `device/` directory are the old/alternate path; do not use them as the reference for new work, even though the old app's default `initUrls()` wiring is HTTP-based.

## What is here

- `main.cpp` boots a `QQmlApplicationEngine` and exposes one `Bridge` (`Bridge.h` / `Bridge.cpp`) into QML as the C++/QML touchpoint.
  Grow `Bridge` or split it into one bridge per panel as real panels get built.
- `qml/` holds the QML sources, organized into `controls/`, `panels/`, and `windows/`.
- `qml/panels/HistogramPanel.qml`-style components replace the old app's hand-copied per-instance blocks with one component parameterized by props (e.g. `channel`). This is the pattern to follow for every panel: one `.qml` file, instantiated wherever needed.

## Build (macOS)

```bash
/opt/homebrew/bin/cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6"
/opt/homebrew/bin/cmake --build build -j6
./build/fisheye_cali_jojo
```

## Porting a panel

1. Create `qml/panels/<PanelName>.qml`.
2. Add it to `QML_FILES` in `CMakeLists.txt`.
3. Swap the matching placeholder in the relevant window for the real component.
4. If the panel needs live data (axis position, camera image, calibration results), add the state to `Bridge` (or a new panel-specific bridge class) as `Q_PROPERTY`/`Q_INVOKABLE`, the same way `ControllerMain` in the old app owns that state today.
