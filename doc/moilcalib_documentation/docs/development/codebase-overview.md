---
id: codebase-overview
slug: /development/codebase-overview
title: Codebase Overview
---

# Codebase Overview

How the version 2.0 client is put together: the source layout, the ROS layer, the
compute core and its tests, and how the application is built and packaged.

<div className="custom-note">
  <div className="custom-note-title">📖 WHAT THIS PAGE IS</div>
  <div>
    <strong>Internal structure</strong>, not a usage guide. If you only need to install and run the application, go to the <a href="../installation/client.md">Client Installation Guide</a> instead.
  </div>
</div>

---

## 1. What changed from version 1.1

The v1.1 framing was *"Python to C++"*. That comparison is now two releases old.
The v2.0 story is **HTTP to ROS 2**:

| Change | Effect |
|---|---|
| `cpp_server/` is gone | The rig-side nodes are in `Server/v2.0.0/` and are not built here |
| ROS 2 layer added | `ros/moil_interfaces/` and `cpp/src/models/device/` |
| Four build routes instead of two | Ubuntu, WSL2, Docker, native Windows/MSVC |
| New packaging chain | Portable `.7z`, Docker image, WSL rootfs, Inno Setup installer |
| New modules | `PctRecommend`, `SharePointSync`, `CbDetect`, `PanelCornerRecovery` |

The C++ / Qt6 / OpenCV / Eigen toolchain itself is unchanged from v1.1.

---

## 2. Source layout

MVC-layered, `core/` outward. Nothing in `core/` touches Qt widgets, so it is
testable headless.

| Layer | Path | Contains |
|---|---|---|
| **Core** (no GUI) | `cpp/src/core/` | `cali/`, `measure3d/`, `pattern/`, `moildev/`, `algorithm/` |
| **I/O** | `cpp/src/io/` | `XlsxIO`, `SharePointSync`, `HttpSync` |
| **Models** | `cpp/src/models/device/` | The three ROS clients |
| **Controllers** | `cpp/src/controllers/` | One per window |
| **Views** | `cpp/src/views/` | `Point3dGlView`, `HistogramPlot`, `theme`, `widgets/` |
| **Forms** | `cpp/ui/` | Qt Designer `.ui`, compiled by `uic` at build time (AUTOUIC) |
| **Entry point** | `cpp/app/` | `main.cpp`, `Help.cpp` |
| **Tests** | `cpp/tests/` | Standalone `*_test` binaries |
| **Packaging** | `cpp/packaging/` | `.7z` builder, launchers, Inno Setup script |
| **Reference scripts** | `cpp/tools/` | Python originals, kept for cross-checking the port |

`cpp/tools/` is worth knowing about: when a number disagrees with what the Python
tool produced, those scripts are how you find out which one is wrong.

### Core sub-modules

| Path | Holds |
|---|---|
| `core/cali/` | `CaliCompute`, `CaliMath`, `CaliRound`, `Regression` |
| `core/pattern/` | `moil_pattern_gennerator`, `PctRecommend` |
| `core/measure3d/` | `moil_3d_algorithm`, `Measure3dTypes`, `AnypointChessboard`, `CbDetect`, `PanelCornerRecovery`, `PlaneFit3dViz` |
| `core/moildev/` | The camera model — anypoint and panorama maps |
| `core/algorithm/` | `moilcali_algorithm` — node detection, centring, histograms |

---

## 3. The ROS 2 layer

Architecture is on its own page: [ROS 2 Architecture](../ros/architecture.md).
What belongs *here* is the build-order rule:

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ BUILD ORDER: moil_interfaces FIRST, AND THE APP USES THE *INSTALLED* COPY</div>
  <div>
    The application compiles against the interfaces in <code>~/moil_ros_ws</code> on Linux, or <code>ros/install</code> on Windows — <strong>not</strong> the ones in the source tree. Editing a <code>.srv</code> and rebuilding only the app changes nothing.
    <br /><br />
    <code>cd ~/moil_ros_ws && colcon build --packages-select moil_interfaces</code>
  </div>
</div>

| Device | File |
|---|---|
| Camera | `cpp/src/models/device/camera_ros_client.{h,cpp}` |
| Axis | `cpp/src/models/device/axis_ros_client.{h,cpp}` |
| Monitor | `cpp/src/models/device/monitor_ros_client.{h,cpp}` |
| Domain ID | `cpp/src/models/device/ros_domain_env.h` |

`ROS_DOMAIN_ID=42` is forced in the ROS client constructors — the user never
exports it for the app.

---

## 4. The HELP button contract

`cpp/app/Help.cpp` hard-codes **this documentation site's URL** and nine page
paths.

```cpp
const QString kDocsBase =
    "https://perseverance-tech-tw.github.io/moilcalib_documentation/docs/v2.0";
```

| Window key | Page slug |
|---|---|
| `main_window`, `curve_color` | `system-overview/main-window` |
| `pattern_generator` | `calibration/pct-pattern-generator` |
| `monitor_viewer` | `calibration/monitor-viewer` |
| `captured_image` | `calibration/camera-calibration` |
| `cali_result` | `calibration/cali-result` |
| `center_setup` | `verification/setup-center` |
| `measure3d` | `verification/3d-verification` |
| `database` | `database/database-overview` |
| *(unmapped)* | `intro` |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ CHANGING A SLUG BREAKS EVERY BINARY ALREADY SHIPPED</div>
  <div>
    An old build keeps requesting the old URL and lands on a 404. Change <code>Help.cpp</code> and the docs site <strong>in the same release</strong>, and bump the version segment in <code>kDocsBase</code> when the site publishes a new version.
  </div>
</div>

`Help::attach()` reuses a HELP button already declared in a `.ui` — `center_setup.ui`
has one — rather than creating a second. Under WSL it hands the URL to the
Windows host, because a WSL distro usually ships no browser and no `xdg-open`.

---

## 5. Threading

The axis stage is polled by a background worker — one `QtConcurrent` task per
axis. Each task reads the limit sensors, the origin sensor, the motion state and
the position, then updates the LEDs, coordinates and alpha/beta live.

Relative moves and homing are also dispatched off the UI thread, so the window
never blocks on a device call. The axis safety-lock is applied on the UI thread
*before* the call is sent, so there is no window in which a second command could
be issued.

Two behaviours that fall out of this:

- The **move LED blinks** on a 500 ms timer while an axis is moving.
- `All HOME` **blocks per axis**, polling every 100 ms until the origin sensor
  and move sensor both read correctly twice running, with a 120-second timeout.

---

## 6. Tests

Standalone verification binaries that build with the app:

```bash
for t in cpp/build/*_test; do echo "== $t =="; "$t" || break; done
```

```powershell
Get-ChildItem cpp\build-win\RelWithDebInfo\*_test.exe | ForEach-Object { "== $_ =="; & $_ }
```

Covered: `CaliCompute`, `CaliMath`, `CaliRound`, `Database`, `Measure3d`,
`Moil3d`, `MoilCali`, `PatternGen`, `Regression`, `Xlsx`.

> These exercise `core/`, which is where every number the application reports
> comes from. On the [native Windows build](../installation/client-windows-native.md),
> which cannot reach a rig, they are the main way to know the build works.

---

## 7. Packaging

| Artefact | Built by |
|---|---|
| Portable Linux `.7z` — bundles Qt, OpenCV, plugins | `bash cpp/packaging/make_linux_7z.sh 2.0.0` |
| Docker image `moilcali-ros` | `Dockerfile`, `Windows/v2.0.0/source/docker/` |
| WSL root filesystem | `Windows/v2.0.0/source/packaging/build_wsl_rootfs.sh 2.0.0` |
| Windows installer | `Windows/v2.0.0/source/packaging/build_installer.ps1 -Version 2.0.0` |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE WINDOWS INSTALLER IS NOT CROSS-BUILT FROM LINUX ALONE</div>
  <div>
    The Linux side stages the app out of the Docker image into a WSL root filesystem; <strong>Inno Setup then wraps it on Windows</strong>. Two machines, two steps, in that order.
    <br /><br />
    <strong>Read <code>Windows/v2.0.0/DEVELOPING.md</code> before changing any of it</strong> — it documents the build chain and the traps that have already cost someone a day.
  </div>
</div>

The Docker build context expects the application source at `./src` and the seed
patterns at `./seed/image_cali`. **`seed/image_cali/` is not in git** —
`image_cali/` is ignored repo-wide. The build succeeds with the folder empty, but
the monitor's w/n/s/e patterns are derived from `pattern_circle_side.png`, so
without it that step silently does nothing.

---

## 8. Runtime dependencies that are not linked

`zip` and `unzip`, shelled out to by `cpp/src/io/XlsxIO.cpp`.

| Missing | Symptom |
|---|---|
| `unzip` | *Load All Excel* reports "No round subfolders (1..10) with .xlsx found" even when the files are there |
| `zip` | Saving results fails |

Neither is installed by `setup.sh`, and neither exists on Windows by default. The
packaged installer bundles both.

---

## 9. Where the runtime data lives

| Path | Holds |
|---|---|
| `cpp/config/cali_system/*.json` | Rig profiles for the Select Cali System combo |
| `cpp/config/camera_parameters.json` | The camera model Setup Center reads and writes |
| `cpp/database/cali_system_v2.db` | The bundled SQLite catalogue |
| `cpp/image_cali/` | Captures and rendered patterns. **Ignored by git** |
| `pattern_json/` | Exported pattern configurations |
| `~/.config/moilcali/sharepoint.json` | Graph credentials, if the user chose to save them |

---

## Diagrams this page needs

No screenshots — this is structure. Two diagrams would help:

| File | What it should show |
|---|---|
| `codebase-layer-diagram.png` | The MVC layers with the arrows pointing outward from `core/`, and which directory holds each |
| `codebase-build-chain.png` | The packaging chain: source → Docker image → WSL rootfs → Inno Setup → `.exe`, marking which step runs on which OS |

---

## Source material

| What | Where |
|---|---|
| Repository map | `README.md` (repo root) |
| Build and layout detail | `cpp/README.md` → *Layout* |
| Packaging chain and its traps | `Windows/v2.0.0/DEVELOPING.md`, `Windows/v2.0.0/source/packaging/README.md` |
| HELP wiring | `cpp/app/Help.cpp` |
