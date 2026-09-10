# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Hard rules

**Never write to git.** No `git commit`, `git push`, `git add`, `git stash`, or
anything else that touches the index or history — including `git checkout --`,
`git reset`, `git restore` and `git rebase`. Leave every change unstaged in the
working tree; diffs are reviewed and committed by hand. Read-only git
(`status`, `diff`, `log`, `show`, `ls-tree`, `branch`) is fine.

**Never run `git clean`.** Called out separately because it reads as harmless
housekeeping and would cost the most here. `Server/`, `ros/moil_interfaces/` and
`doc/` arrived in this tree by filesystem copy, not by checkout: until the
migration commit lands they are **untracked**, and `git clean -fd` deletes all
three — about 490 files, including the only copy of the auto-centre work in this
branch. There is no undo.

**There is exactly one copy of the engine, and it must stay that way.**
[Server/v2.1.0/common/engine/](Server/v2.1.0/common/engine/) holds the only
`moilcali_algorithm`, `CaliCompute`, `CaliMath`, `CaliRound`, `Regression`,
`Moildev` and pattern generator in this branch. This used to be a *duplicated*
engine — the old Widgets client carried its own copy of each file, the two were
meant to be byte-identical, and a `cmp` ritual plus three `*_client`/`*_server`
dump-pair targets existed to police the drift. That client is gone, so the ritual
is gone with it. **Do not reintroduce a second copy.** Anything that needs this
maths links `moil_common`; nothing reimplements it.

**Do not change what the `roi_exact` op answers.** `detect::kRoiExact`
([ComputeOps.h](Server/v2.1.0/common/engine/compute/ComputeOps.h)) is the fallback
a client drops to when a centre fit refuses. A centre fit returning `(-1,-1)` is
the correct answer on a badly aimed shot, not an error — the op must keep behaving
exactly as it does today, even when refactoring around it.

## What this repository is

A fisheye-camera calibration system. Four trees, and **one** GUI:

| Tree | Role |
| --- | --- |
| root (`src/`, `qml/`) | `moil_fisheye_cali` — the Qt Quick (QML) client. The only application built from this branch. |
| [ros/moil_interfaces/](ros/moil_interfaces/) | The **ROS 2 contract** (37 srv, 6 action, 4 msg). One copy, built by every side. |
| [Server/v2.1.0/](Server/v2.1.0/) | The rig **server**: one process, eight nodes (`/moil_axis /moil_camera /moil_monitor /moil_compute /moil_session /moil_measure3d /moil_jobs /moil_supervisor`), plus `common/`, the calibration engine. Runs on the rig's machine. |
| [doc/](doc/) | `auto_center_design.md` and the Docusaurus documentation site. |

Client and server find each other by plain LAN multicast DDS discovery on
`ROS_DOMAIN_ID=42`. **Without the rig's nodes the GUI opens but every device sits
idle — that is a missing rig, not a broken app.** Diagnose with
`ROS_DOMAIN_ID=42 ros2 node list` before suspecting the code.

### The old Widgets client is not here, and that has consequences

`cpp/` — `moilcali`, the Qt Widgets client — was migrated into this branch and
then removed again. All of it is still committed on `v2.0_2026_main-cpp-ros`.

**`subapp_3d_verification/` and `subapp_center_setup/` are NOT part of that, and
must not be removed with it.** Their sources are byte-identical to files in
`cpp/`, which makes them look like leftovers — they are not. They were ported
into this app deliberately (2026-08-28 and 2026-08-31), they are live CMake
targets, and the *3D Verification* and *Center Setup* buttons in `Main.qml` open
them. Byte-identity records where code came from, not whether it is still in use;
run `git log -- <path>` before concluding anything from it. This mistake has
already been made once on this branch and had to be reverted.

What went with it, and what that means for anyone picking up the auto-centre work:

- `session_ros_client` — the `autoCenter()` wrapper, `AutoCenterResult`, and the
  degrade path (`parseResult` / `describe` / `isMissingOpError`);
- `src/core/centering/center_loop` — the 5-axis loop's decision core;
- **every test target.** `autocenter_test`, `center_loop_test`,
  `session_client_test`, and the three dump pairs. There is no C++ test suite in
  this branch at all.

**The `auto_center` cascade itself survives**, server-side, in
[ComputeDetectOps.cpp](Server/v2.1.0/common/engine/compute/ComputeDetectOps.cpp),
and builds clean.

**Updated 2026-09-07 — the QML app now drives it.**
[ComputeController](src/ComputeController.h) calls `auto_center`, `roi_exact`,
`histogram_8dir` and `nodes_8dir` over `/compute/detect`, and the Centering panel's
*Find Pos* / *Find Neg* buttons are its callers. The app still has **no session
layer**: there is no `RunCompute`, no `SessionState`, and captures are held
client-side in [ImageStore](src/ImageStore.h) and re-sent with each op rather than
resolved from a server session slot. So the *cascade* is wired; the *session* is
still not, and anything wanting `/session/run_compute` remains future work.

The degrade path that lived in `session_ros_client` — `parseResult` / `describe` /
`isMissingOpError` — did **not** come back. `ComputeController` reports a failed op
by its message; it does not fall back to `roi_exact` when `auto_center` is missing.

### Centring is a control signal, not a display value

The rig carries the camera on five axes (X/Y/Z/Pitch/Yaw) and moves it until the
calibration pattern is centred in the capture; the pattern centre is the origin
every ICT node is measured from. Centre-finding code (`moilcali_algorithm`, the
detect ops) therefore feeds hardware positioning: **a fit that is not trustworthy
must be reported as "no centre" — never a plausible-looking wrong one.**
Thresholds and the `auto_center` cascade are specified in
[doc/auto_center_design.md](doc/auto_center_design.md); do not re-derive them
locally.

## Build and run

### The server

```bat
Server\v2.1.0\build_server.bat   :: output goes to MOIL_WS_BASE
Server\v2.1.0\run_server.bat
Server\v2.1.0\check_server.bat   :: PASS/FAIL per rung; runs from a client machine too
```

No script needs editing. All host paths (Qt, OpenCV, Eigen, ROS distro, MSVC) come
from `Server\v2.1.0\scripts\moil_env.bat`, which **searches** rather than assumes.
Inspect with `Server\v2.1.0\scripts\moil_env.bat --check`; override in
`Server\v2.1.0\deps.local.bat` (gitignored — never hardcode a path into a tracked
script in that tree).

**`MOIL_WS_BASE` is overridden in this checkout, and that is deliberate.**
`moil_env.bat` defaults it to `%SystemDrive%\moil_ws` and keys it with a
`.moil_source` stamp so two checkouts cannot silently overwrite each other's build
tree. On this machine that default is already claimed by another checkout, so
`deps.local.bat` points this tree at `C:\moil_ws_v21`. Without it, alternating
between trees costs a full rosidl rebuild every time.

Note `check_server.bat`'s rung 3 has a quoting bug — its `| findstr` reaches
`ros2` as arguments and the rung reports `unrecognized arguments`. Rungs 2 and 4
are the ones that tell you whether the rig is alive.

### The QML app

`README.md` is the authoritative setup guide — Part A (Windows 11 / ROS 2 Lyrical
/ pixi), Part B (Ubuntu 24.04 / Jazzy), Part C (macOS, UI work only — Mac Wi-Fi
drops the multicast DDS discovery needs).

```powershell
# Windows: VsDevShell first (it rewrites PATH wholesale), then pixi shell, then inside it:
& $cmake --build build-win-ros --config RelWithDebInfo --parallel
.\tools\run_windows_ros.ps1
```

```bash
# Ubuntu: no sourcing needed after the first configure
cmake --build build -j6 && ./build/moil_fisheye_cali
```

- **The `.exe` cannot be double-clicked.** Without the environment
  `tools/run_windows_ros.ps1` builds it exits instantly with no message. Silence
  is a missing environment, not a broken build. The script decodes the exit code
  for you, which is the single most useful fact when it disappears.
- **Editing any `.qml` requires a rebuild.** `qt_add_qml_module` compiles QML into
  the binary; nothing is read from disk at run time.
- **Adding a new `.qml` also requires a reconfigure**, and it must be listed under
  `QML_FILES` in `CMakeLists.txt`. Omit it and the build still passes — the engine
  then fails at run time.
- Build directories are per-machine and gitignored (`build/`, `build-win/`,
  `build-win-ros/`). Windows uses Lyrical's own CMake 3.28.3; a tree made by a
  different CMake or generator cannot be reused.

Two CMake options **switch themselves off** with a `message(STATUS ...)` rather
than erroring when their dependencies are missing, so a machine with none of it
still gets a browsable UI:

| Option | Needs | Off means |
|---|---|---|
| `FISHEYE_ENABLE_ROS` (default ON on Linux, OFF elsewhere) | `rclcpp`, `rclcpp_action`, `moil_interfaces` | `FISHEYE_ROS_ENABLED` undefined; `RosServerProbe`, `AxisController`, `CameraController`, `MonitorController`, `PatternController`, `ComputeController` and `CalibrationController` compile to inert stubs that report "this build has no ROS 2 support" rather than failing silently; rig controls do nothing |
| `FISHEYE_ENABLE_SUBAPPS` (default ON) | Qt Widgets/Concurrent/OpenGLWidgets, OpenCV **4**, Eigen3 | `FISHEYE_SUBAPPS_ENABLED` undefined; 3D Verification and Center Setup unavailable, and `src/main.cpp` falls back from `QApplication` to `QGuiApplication` |

Keep the `#ifdef FISHEYE_ROS_ENABLED` and `#ifdef FISHEYE_SUBAPPS_ENABLED` guards
intact, and make sure both off-paths still compile and behave inertly.

### Not in this branch

| Left behind | Where it is |
| --- | --- |
| `cpp/` — the Widgets client, its tests, `build_client.bat`, `run_client.bat` | `v2.0_2026_main-cpp-ros` |
| `Server/v2.0.0/` — the older three-launcher Python server | same |
| `docker/`, v2.0's `tools/` (`docs.sh`), `setup.sh`, `Windows/`, `design/` | same |

This tree's `tools/` is the **QML app's** and holds only `run_windows_ros.ps1`.

### Documentation site

The source is in-repo at [doc/moilcalib_documentation/](doc/moilcalib_documentation/);
the `tools/docs.sh` wrapper that built and served it did not migrate — run
Docusaurus directly, or take the script from the v2.0 branch. Served, it answers on
<http://127.0.0.1:3000/moilcalib_documentation/docs/v2.0/intro>. The GitHub Pages
site is stale and no longer published to.

**Much of that site still describes the v2.0 system** — `cpp/`, `Server/v2.0.0/`,
`Windows/v2.0.0/` — and about 35 of its references point at paths that are no
longer in this branch. It is archival documentation of a shipped version; treat it
as history, not as a description of this tree.

Note the singular `doc/`. `/docs/` at the repo root is the QML app's notes
directory and is gitignored except for three force-tracked files.

## Tests

**There is no C++ test suite in this branch.** Every `*_test` target lived in
`cpp/CMakeLists.txt`, which was removed. What remains:

- `src/compute_selftest.cpp` is compiled into the `moil_server` binary — a
  server-side self-check, not a standalone target.
- `qt_add_qml_module` generates a `moil_fisheye_cali_qmllint` target for static
  QML checks.
- `Server/v2.1.0/check_server.bat` is the end-to-end rig check.

If you change engine maths, there is no longer a mechanical check that catches it.
That is a real gap, and the honest mitigation is to port a test target back from
`v2.0_2026_main-cpp-ros` rather than to trust a careful reading.

## Architecture

### Compute ops are actions, not services

Client→server image analysis goes through the **`RunCompute` action on
`/session/run_compute`**. `DetectOp.srv` is **not** on that path — it is served at
`/compute/detect` by the compute node. Consequences worth knowing before designing
anything here:

- Op names are plain strings in
  [ComputeOps.h](Server/v2.1.0/common/engine/compute/ComputeOps.h) (`nodes_8dir`,
  `roi_exact`, `pattern_center`, `ring_center`, `auto_center`, …) and `params` is
  free-form JSON, so **a new optional parameter needs no interface change** — and
  no rebuild of both sides (see the type-hash trap below).
- Captures live in named server slots: analysis names a slot, it never uploads an
  image. `CaliSession::captureMat` caches the decoded `Mat`, so repeated ops on one
  slot are cheap.
- Detect ops are the one path with **no progress and no cancel**
  ([session_services.cpp:370](Server/v2.1.0/common/engine/session/session_services.cpp#L370)).

### The `moil_interfaces` type-hash trap

ROS matches services by **type hash**. A `.srv` differing by one character makes
every call of that service fail to connect while topics keep flowing — the rig
looks alive, no button works, and nothing is logged beyond "service unavailable".
This is why there is exactly one copy at [ros/moil_interfaces/](ros/moil_interfaces/).
There used to be a second, twelve-file copy inside `Server/v2.0.0/packages/`, and
one of those twelve had already drifted from the canonical definition; it went with
that server. Edit an interface → rebuild **both** sides: `build_server.bat` on the
rig, `colcon build --merge-install --base-paths .` in `ros/` for the client.

### The QML app (`src/`, `qml/`)

**C++/QML boundary: singletons, not context properties.** There is no `Bridge`
class and no `setContextProperty`. `src/main.cpp` is ~30 lines: it constructs the
app, forces the `Basic` QuickControls style and a light color scheme, and calls
`engine.loadFromModule("FisheyeCaliJojo", "Main")`. Every C++ type reaches QML
through `QML_ELEMENT`, which is what lets `qmllint` see them statically. Four are
also `QML_SINGLETON`, so QML calls them by type name:

| Type | Role |
|---|---|
| `AxisController` + `AxisState` | the live rig: connection state, capabilities, jog, drive-to-limit, home, stop |
| `CameraController` | `/camera/capture` into named slots, the live topic subscription, FOV |
| `MonitorController` | brightness, `show_pattern`, `close_pattern`, screen mapping, `prepare`/`show_prepared` |
| `PatternController` | renders a pattern spec and shows it — the spec editor's controller, not a device one |
| `ComputeController` | `/compute/detect`: centres, histogram curves, nodes |
| `CalibrationController` | `/compute/xlsx`, `/compute/cali`, `/compute/series` — the Cali Result window |
| `PatternIo` | local file read/write, and path↔URL conversion QML must not do by hand |
| `RosServerProbe` | the three ROS status dots in the Server panel |
| `HttpServerProbe` | the HTTP tab's host/port config and status dots |
| `SubAppWindows` | opens the two ported QWidget sub-apps as top-level windows |
| `ProbeStatus` | `Q_NAMESPACE` enum (`Unknown/Checking/Ok/Failed/Partial`) shared by every controller |

**Each controller owns its own `rclcpp::Context`, node and executor**, exactly as
`AxisController` does, and each threads a `generation` counter through every
callback so a reconnect invalidates in-flight replies. That is six contexts in one
process now, which makes the Windows `XTYPES_TYPE_REPRESENTATION` wall of text
longer — still cosmetic, still a sign DDS is working.

**`ImageStore` is not a QML type.** It is a plain namespace holding the captures
once, keyed by slot (`single`, `positive`, `negative`, `live`), because two
different pieces of code want two different representations of the same frame:
`CameraController` wants a `QImage` to paint, `ComputeController` wants the
**original compressed bytes** to send back to `/compute/detect`. Re-encoding a
`QImage` to PNG would hand the detect ops a picture that is not the one the camera
produced, and every centre fit here is a measurement of exact pixel values.

`AxisState` is `QML_UNCREATABLE` — instances are owned by `AxisController` and
exposed as `AxisController.x`, `.y`, `.z`, `.yaw`, `.pitch`, `.allAxes`.

**The probe units are probes, not transports.** Neither carries data:

- `RosServerProbe` opens its own `rclcpp::Context` and node
  (`fisheye_cali_jojo_probe`), then only ever asks `service_is_ready()` and
  `count_publishers()`. It never sends a request and runs no executor.
- `HttpServerProbe` is a `QNetworkAccessManager` reachability check plus the
  committed host/port config. **HTTP is not a transport here** — there is no HTTP
  data path in this app. Its statuses are a receipt of the last `probeAll()`, which
  is why editing a field voids them to `Unknown`.

**The app does not connect on startup.** `ServerConfigPanel` emits
`rosUpdateRequested`, `Main.qml` handles it by calling `RosServerProbe.probeAll(...)`
and `AxisController.connectTo(...)`. Pressing *Update* is what creates the ROS
context.

**`AxisController`: worker threads + a generation counter.**
`src/AxisController.cpp` (~1500 lines) is the most intricate file here.

- Two detached `std::thread`s per session, each owning its **own**
  `rclcpp::Context` and `SingleThreadedExecutor`: `worker` (node
  `fisheye_cali_jojo_axis` — sensor/position/watch polling) and `commandWorker`
  (node `fisheye_cali_jojo_axis_cmd` — move/command/sensor services plus the home
  and limit-move actions). They are split so a long-running poll never blocks a
  stop command.
- **Every callback carries a `generation`, and every `apply*` drops the result if
  it does not match `d_->generation`.** That is how a reconnect or disconnect
  invalidates in-flight replies from the previous session. New async paths must
  thread the generation through the same way.
- The polling rates are derived, not magic numbers: `kSerialRoundTripsPerSecond`
  (5.7), `kRoundTripsPerAxisSample` (5) and `kLinkBudget` (0.85) feed `constexpr`
  `watchHzFor()` / `pollGapMsFor()`. The rig's serial link is the bottleneck.
- Two `rclcpp::Context`s in one process is why Windows prints a wall of
  `XTYPES_TYPE_REPRESENTATION Error ... already registered locally` on Update.
  Cosmetic — Lyrical's Fast DDS 3.6 has a process-global type registry that
  complains, Jazzy's 2.x does not. Seeing it means DDS is working.

**QML layout system.** `qml/Theme.qml` is a `pragma Singleton` (registered via
`QT_QML_SINGLETON_TYPE` in `CMakeLists.txt`) holding **every** spacing, size, font
and color, derived from `FontMetrics.height` (`unit`) and `averageCharacterWidth`
(`charUnit`) so the whole UI scales with the system font. Never hardcode a color or
a pixel size in a panel — add it to `Theme.qml`. `qml/controls/ScaledCanvas.qml`
wraps each window's content in a uniformly down-scaled `Item`: windows declare a
design size and the canvas shrinks to fit smaller screens instead of reflowing. One
component per panel, parameterized by properties. Three top-level windows:
`Main.qml`, `PatternAndMonitor.qml`, `MoilCalibrationResult.qml`.

**Adding a panel:** create `qml/panels/<PanelName>.qml`; add it to `QML_FILES` in
`CMakeLists.txt` and reconfigure; replace the matching `PanelPlaceholder` in the
relevant window; put live data on an existing singleton or a new one (`QObject`
with `QML_ELEMENT`/`QML_SINGLETON`, added under `SOURCES`). If a new C++ type shows
up as `'X': undeclared identifier` in `*_qmltyperegistrations.cpp`, the cause is
`target_include_directories(moil_fisheye_cali PRIVATE src)` going missing — the
generated file guards its includes with `__has_include`, so an unreachable header
fails *silently*.

## Things that fail silently

Almost every trap in this project produces no error message. In rough order of time
lost:

- **`moil_fisheye_cali.exe` double-clicked, or started from a plain shell** → exits
  instantly with no output. It needs the ROS and pixi environments for its DLLs.
  Always use `tools\run_windows_ros.ps1`.
- **`AMENT_PREFIX_PATH` unset** → the app opens normally and simply cannot find the
  rig.
- **conda-forge Qt/OpenCV winning over the intended ones.** The pixi ROS
  environment ships its own; `-DQt6_DIR` alone is not enough because
  `Qt6Config.cmake` re-searches per component through `CMAKE_PREFIX_PATH`. Confirm
  the configure output names *your* OpenCV, not `.pixi/envs/...`.
- **Windows firewall rule scoped `Domain,Private`.** A laptop's Wi-Fi is usually
  Public; the rule lists as enabled and allows nothing. The rig can then ping you
  while you hear silence from it.
- **A leftover `ROS_DISCOVERY_SERVER` / Fast DDS profile** makes discovery find
  nothing at all. The launchers blank these deliberately.
- **`qt_policy(SET QTP0004 NEW)` stopped the app from starting at all.** NEW writes
  an extra `qmldir` into every subdirectory holding QML files, each saying
  `prefer :/qt/qml/FisheyeCaliJojo/`. The engine then finds the module twice and
  refuses to load with `"FisheyeCaliJojo" is ambiguous. Found in
  qrc:/qt/qml/FisheyeCaliJojo/ and in qrc:/qt/qml/FisheyeCaliJojo/` — the two
  paths are identical because both copies report the *prefer* target rather than
  where they were found, which makes the message read like nonsense. It is now
  `OLD` ([CMakeLists.txt](CMakeLists.txt)); nothing here imports a subdirectory as
  a module.
- **On Windows Qt sends `qWarning` to OutputDebugString, not to stderr.** A QML
  error that kills the app therefore prints *nothing*: you get `Exit code -1` and
  an empty console. `tools/run_windows_ros.ps1` now sets
  `QT_FORCE_STDERR_LOGGING=1`; without it, diagnosing any QML failure on Windows
  is guesswork.
- **The pixi environment ships a complete conda-forge Qt 6 in `Library\bin`.**
  Anything that puts that directory ahead of `C:\Qt\...\bin` swaps the whole
  toolkit underneath an app built against the other one. It can even appear to
  work. `run_windows_ros.ps1` puts `$Qt\bin` first and appends pixi last for this
  reason — do not reorder them.
- **`slots` cannot be used as a C++ identifier.** It is a Qt keyword macro that
  expands to nothing, so a parameter named `slots` silently loses its name and any
  `for (x : slots)` over it fails to compile in a way that points at the wrong
  line. Same for `signals`, `emit` and `foreach`.
- **A QML binding cannot see a `ListModel` edit.** `layerModel.setProperty()`
  emits no signal a binding depends on, so `JSON.stringify(panel.specJson())`
  re-evaluates when a scalar property changes and *never* when a layer's radius,
  shape or centre does — the edit an operator actually makes. Auto Update looked
  correct and ignored the layer table. The pattern panels carry a `layerRevision`
  counter, bumped by `setLayer()`, and the fingerprint includes it; a
  `setProperty` that skips `setLayer()` is a change nothing will notice.
- **`"file://" + path` is right on Linux and wrong on Windows.** A Windows path
  starts with a drive letter, so the two-slash form makes `C:` the *host* and the
  `Image` renders nothing, with no error. Use `PatternIo.toFileUrl()` /
  `toLocalPath()` from QML rather than building URLs by string concatenation.
- **A QML image URL that does not change serves the cached picture.** A new
  capture into the same slot looks like a button that did nothing. That is what
  `ImageStore`'s per-slot revision counter is for, and why `clear()` bumps it
  instead of resetting it.
- **`concentric_positive` and `concentric_negative` show different ring counts.**
  The negative swaps the colour pair, and `renderConcentric` draws on a white
  canvas — an outermost white layer produces no outer edge. A count measured on the
  positive image and applied to a negative capture **drops every ray**. Measure the
  slot-matched file.
- **`MoilCali::set_raw_nodes(true)` disables `blur_gray` process-wide**
  ([moilcali_algorithm.h](Server/v2.1.0/common/engine/algorithm/moilcali_algorithm.h)).
  A toggle meant for node extraction silently moves every centre fit while it is
  set. Compute paths that depend on the blur must pin the flag around themselves;
  `auto_center` does this with `RawNodesPin`.
- **Never touch a `Q_PROPERTY` or a QML object from a ROS executor thread.** Doing
  it from a worker thread on Win32 *deadlocks* rather than merely misbehaving.
  `AxisController` solves it by routing every worker-thread result back through
  `QMetaObject::invokeMethod(this, "applyX", Qt::QueuedConnection, ...)` into a
  private `Q_INVOKABLE apply*` that runs on the GUI thread. That is why those
  methods are `Q_INVOKABLE` despite being private — do not "clean that up".
- **`&&` does not chain commands in Windows PowerShell 5.1** — use `;`.
- Two ROS 2 installs on one machine → `LoadLibrary error: 127` at the first ROS
  client. Build and run against the same one.
- **A `.qml` edit with no rebuild changes nothing**, and a new `.qml` missing from
  `QML_FILES` still builds — it fails at run time instead.
- **A deep build path overflows `MAX_PATH`.** MSBuild's FileTracker fails with
  `FTK1011` and rosidl nests ~120 characters on its own; that is why
  `MOIL_WS_BASE` is short and outside the tree.

## Build-configuration constraints

- **Never build `Debug`** — the ROS 2 Windows binaries are Release-only.
  `RelWithDebInfo` is the *shipping* configuration and what packaging reads.
- **`/fp:fast` and `/arch:AVX2` are deliberately not set.** The whole output is
  numbers from a least-squares fit; changing FP semantics changes results, and with
  the dump-pair targets gone there is now nothing that would catch it.
- **Pass `-j2` on Linux**, not `-j$(nproc)` — a bare `-j` gets the compiler
  OOM-killed (exit 137).
- zlib for `XlsxIO` comes from Qt's bundled copy or a standalone `ZLIB::ZLIB`.
  Override with `-DQT_ZLIB_INCLUDE_DIR` or `-DZLIB_ROOT`.
- MSVC settings the engine cannot compile without, declared PUBLIC on `moil_common`
  and needed by anything compiling those headers directly: `/bigobj /utf-8` and
  `_USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN`.

## Conventions

- **Source comments are labels, not explanations. Eight words, hard maximum.**
  A comment in `src/`, `qml/` or a script says *what this is* and stops:
  `// Requests still expected to answer.` No paragraphs, no dated migration
  notes, no worked examples, no reproduction of a bug report. Changed
  2026-09-10; the long block comments that used to be the convention here were
  removed on that date and the code reads better without them.
- **The *why* goes in [docs/CODE_NOTES.md](docs/CODE_NOTES.md), not above the
  function.** That file is where the diagnoses live, one section per source
  file, and it holds everything the old block comments said. It is still true
  that a silent failure must be written down — write it down *there*. When you
  fix one, add the section; when you change behaviour a section describes,
  update it. Deleting an explanation without moving it is the one thing that is
  not allowed, because almost every trap in this project produces no error
  message and the note is the only record that it exists.
- **Commit messages are plain sentences describing the effect**, imperative and
  outcome-shaped: "Stop the sensor probe freezing the GUI for five seconds".
- READMEs are the real documentation and carry dated entries for behaviour changes
  ("Fixed 2026-09-01 — ..."). Update them alongside the code.

## Repository layout

```
src/                     QML app C++ -- one flat directory, no subfolders
qml/windows/             top-level ApplicationWindows
qml/panels/              feature blocks, one file per panel
qml/controls/            small reusable pieces
qml/Theme.qml            singleton: colours, spacing, font sizes
subapp_3d_verification/  the ported QWidget 3D dialog -- LIVE, not a leftover
subapp_center_setup/     the ported QWidget centre-setup dialog -- LIVE
Server/v2.1.0/           the rig server (C++, eight nodes) + common/, the engine
ros/moil_interfaces/     the ROS 2 contract. One copy, built by every side
doc/                     auto_center_design.md + the Docusaurus site
tools/run_windows_ros.ps1  the Windows launcher
assets/                  images compiled into the binary
docs/CODE_NOTES.md       why the code in src/ and qml/ is shaped the way it is
docs/                    QML-app notes; gitignored except the force-tracked files
```
