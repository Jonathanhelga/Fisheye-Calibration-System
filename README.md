# MOIL Fisheye Calibration

Qt 6 / QML desktop client for the fisheye calibration rig.
Binary: **`moil_fisheye_cali`** (`.exe` on Windows).

The app is a **ROS 2 client**. It talks to the rig's camera / axis / monitor nodes on `ROS_DOMAIN_ID=42`, which run on the rig's machine, not in this repo.

| Platform | ROS 2 distro | Go to |
|---|---|---|
| **Windows 11** | **Lyrical**, installed with **pixi** | Part A |
| **Ubuntu 24.04** | **Jazzy**, from apt | Part B |
| macOS | none possible — Mac Wi-Fi drops the multicast DDS discovery needs | Part C |

Follow your own part top to bottom. Part D is shared reference.

---
---

# Part A — Windows 11

ROS 2 has no package for the plain Qt + MSVC toolchain. Windows uses **ROS 2 Lyrical**, the distro that ships a prebuilt Windows archive, installed into a **pixi** environment.

Thirteen steps, A1 to A13. Budget 60–90 minutes and ~30 GB for a fresh machine. Once it is set up, day to day you only run **A0**.

Everything goes in **PowerShell**.

> **When you need `pixi shell`, and when you don't:**
>
> | Doing | Need |
> |---|---|
> | `colcon build` — step A8 | `pixi shell` |
> | `ros2` CLI — step A13 | `pixi shell` |
> | configure, build, run the app | the `local_setup.ps1` + `PATH` lines. **No pixi shell.** |
>
> Both use the same environment. `pixi shell` puts its *commands* (`colcon`, `ros2`, `python`) on `PATH`; the app only needs its *DLLs*, which the `PATH` line supplies.

---

## A0. Everyday loop

**Already installed? This is the whole daily cycle.** Four commands, in this order:

```powershell
cd D:\moil_fiseheycali_cpp\moil-fisheye-calisys      # <- your clone

& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
pixi shell --manifest-path C:\dev\ros2-lyrical\pixi.toml
```

Wait for the prompt to read `(pixi_ros2_lyrical) PS ...`, then:

```powershell
$cmake = "C:\dev\ros2-lyrical\.pixi\envs\default\Library\bin\cmake.exe"
& $cmake --build build-win-ros --config RelWithDebInfo --parallel
.\tools\run_windows_ros.ps1
```

Four things about that sequence:

| | |
|---|---|
| **`Launch-VsDevShell` goes first** | it rewrites `PATH` wholesale. Run it after pixi and it knocks the ROS entries back out |
| **Split at `pixi shell`** | it starts a *new* interactive shell. Anything pasted after it sits buffered in the outer shell until you `exit` |
| **Re-set `$cmake` inside** | the pixi shell is a child process and does not inherit your variables. Inside it a bare `cmake` is already pixi's 3.28.3, so `cmake --build build-win-ros --config RelWithDebInfo --parallel` works too |
| **No reconfigure needed** | only re-run A10 after a `CMakeLists.txt` change, or after adding a new `.qml` file |

Everything below is the one-time setup that gets you to this point.

---

## A1. Visual Studio 2022 Build Tools

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --override `
  "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

10–20 minutes, prints nothing while it works.

**Check** — must print `True`:

```powershell
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
```

> Must be MSVC, not MinGW or MSYS2. Qt's `msvc2022_64` build and the ROS 2 Windows binaries are both MSVC ABI.

---

## A2. CMake

```powershell
winget install Kitware.CMake
```

Close PowerShell, open a new window.

**Check:**

```powershell
cmake --version
```

---

## A3. Qt 6.8.1

Manual install, needs a free Qt account.

1. Installer from <https://www.qt.io/download-qt-installer>
2. Sign in → **Custom installation** (the default does *not* include the MSVC build)
3. **Qt** → **Qt 6.8.1** → tick **MSVC 2022 64-bit**
4. Leave the folder at `C:\Qt`
5. Install (~20 min, ~5 GB)

Untick everything else — you do not need Qt Creator, Android or WebAssembly.

**Check:**

```powershell
Test-Path C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
```

> **6.8 is a hard floor.** `CMakeLists.txt` sets `qt_policy(SET QTP0004 NEW)`, which needs Qt 6.8+. An older Qt fails with an error naming *the policy*, not the version.

---

## A4. OpenCV 4.10.0

1. Download `opencv-4.10.0-windows.exe` from <https://github.com/opencv/opencv/releases/tag/4.10.0>
2. Run it — it is a self-extracting archive, not an installer
3. Extract to **`C:\`**, just the drive root

> **`C:\`, not `C:\opencv`.** The archive creates the `opencv\` folder itself. Aim it at `C:\opencv` and you get `C:\opencv\opencv\build`, and every path below misses.

**Check:**

```powershell
Test-Path C:\opencv\build\x64\vc16\bin\opencv_world4100.dll
```

> `vc16` is the VS2019 toolset name but is ABI-compatible with VS2022. There is no `vc17`.

---

## A5. Eigen 3.4.0

Header-only, but you **cannot just unzip it** — `find_package(Eigen3)` reads a config file only the *install* step generates.

```powershell
Invoke-WebRequest https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip -OutFile $env:TEMP\eigen.zip
Expand-Archive $env:TEMP\eigen.zip -DestinationPath C:\src
cmake -S C:\src\eigen-3.4.0 -B C:\src\eigen-build -DCMAKE_INSTALL_PREFIX=C:\eigen3
cmake --install C:\src\eigen-build
```

**Check:**

```powershell
Test-Path C:\eigen3\share\eigen3\cmake\Eigen3Config.cmake
```

Then `C:\src\eigen-3.4.0` and `C:\src\eigen-build` can be deleted.

> "The C compiler is not able to compile a simple test program" here means the *path* is wrong, not the compiler. Keep it short and outside `%TEMP%` — MSBuild refuses to build under the temp directory.

---

## A6. Allow scripts, then install pixi

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
winget install prefix-dev.pixi
```

**Close PowerShell, open a new window**, then check:

```powershell
pixi --version
```

> pixi fetches what ROS itself needs. `rclcpp.dll` depends on spdlog, console_bridge, tinyxml2, yaml-cpp, OpenSSL, zstd and ICU, and none of those ship inside the ROS archive.

---

## A7. ROS 2 Lyrical

Download `ros2-lyrical-<date>-windows-AMD64.zip` from <https://github.com/ros2/ros2/releases> and unpack it so `pixi.toml` sits directly in `C:\dev\ros2-lyrical\`.

```powershell
cd C:\dev\ros2-lyrical
pixi install
pixi run python preinstall_setup_windows.py
```

> Keep it **outside the repo, on a short path**. Conda environments bake absolute paths in, so it cannot be moved after `pixi install`, and deep colcon trees hit the 260-character Windows path limit.

**Check:**

```powershell
Test-Path C:\dev\ros2-lyrical\local_setup.ps1
```

---

## A8. Build `moil_interfaces`

The `.srv` / `.msg` contracts the app uses.

> **The sources are not on this branch.** `ros/` holds only `build/`, `install/` and `log/`, all gitignored. Copy the package in from `v2.0_2026_main-cpp-ros` first:
>
> ```powershell
> # from a clone or worktree of v2.0_2026_main-cpp-ros
> Copy-Item -Recurse <v2.0-clone>\ros\moil_interfaces .\ros\
> ```
>
> Building in-tree means later edits to `ros/moil_interfaces/**` reach the app without a second workspace.

**This is the one step that needs `pixi shell`.**

Block 1 — in your normal window:

```powershell
cd D:\moil_fiseheycali_cpp\moil-fisheye-calisys      # <- your clone
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
pixi shell --manifest-path C:\dev\ros2-lyrical\pixi.toml
```

Block 2 — **only after the prompt reads `(pixi_ros2_lyrical) PS ...`**:

```powershell
$repo = "D:\moil_fiseheycali_cpp\moil-fisheye-calisys"   # <- set again; pixi shell is a child process
. C:\dev\ros2-lyrical\local_setup.ps1
cd $repo\ros
pwd            # must end in \ros
colcon build --merge-install --base-paths .
exit
```

> **Two blocks, not one.** `pixi shell` starts a new interactive shell; anything pasted after it stays buffered in the outer shell until you `exit`.

> **Check `pwd` first.** `--base-paths .` builds whatever is under the current directory. In the wrong folder colcon starts rebuilding all of ROS 2 from source. If that happens: stop it, then `Remove-Item -Recurse -Force C:\dev\ros2-lyrical\build, C:\dev\ros2-lyrical\install, C:\dev\ros2-lyrical\log`

**Check:**

```powershell
Test-Path .\ros\install\local_setup.ps1
```

---

## A9. Check everything before building

Every line must print `True` except the last two:

```powershell
Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
Test-Path C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
Test-Path C:\opencv\build\x64\vc16\bin\opencv_world4100.dll
Test-Path C:\eigen3\share\eigen3\cmake\Eigen3Config.cmake
Test-Path C:\dev\ros2-lyrical\local_setup.ps1
Test-Path .\ros\install\local_setup.ps1
cmake --version
pixi --version
```

| Failed | Go back to |
|---|---|
| MSVC | A1 |
| `Qt6Config.cmake` | A3 |
| `opencv_world4100.dll` | A4 |
| `Eigen3Config.cmake` | A5 |
| `C:\dev\ros2-lyrical\local_setup.ps1` | A7 |
| `ros\install\local_setup.ps1` | A8 |
| `cmake --version` | A2 |
| `pixi --version` | A6 |

---

## A10. Configure

No pixi shell. Uses **Lyrical's own CMake**, and its own build directory.

```powershell
cd D:\moil_fiseheycali_cpp\moil-fisheye-calisys      # <- your clone

. C:\dev\ros2-lyrical\local_setup.ps1
. .\ros\install\local_setup.ps1

$cmake = "C:\dev\ros2-lyrical\.pixi\envs\default\Library\bin\cmake.exe"

& $cmake -S . -B build-win-ros -G "Visual Studio 17 2022" -A x64 `
  -DFISHEYE_ENABLE_ROS=ON `
  -DQt6_DIR="C:/Qt/6.8.1/msvc2022_64/lib/cmake/Qt6" `
  -DOpenCV_DIR="C:/opencv/build" `
  -DEigen3_DIR="C:/eigen3/share/eigen3/cmake" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.1/msvc2022_64;C:/eigen3;$env:CMAKE_PREFIX_PATH"
```

Four things there are load-bearing:

| Part | Why |
|---|---|
| `build-win-ros`, and **Lyrical's** `cmake.exe` | pixi ships CMake 3.28.3, A2 installed 4.x. One cannot read the other's build tree |
| the two `local_setup.ps1` lines | they set `AMENT_PREFIX_PATH` / `CMAKE_PREFIX_PATH` so `rclcpp` and `moil_interfaces` are found |
| explicit `Qt6_DIR` / `OpenCV_DIR` / `Eigen3_DIR` | pixi carries its own conda-forge Qt, OpenCV and Eigen; an explicit `*_DIR` beats prefix-path search |
| **appending** `$env:CMAKE_PREFIX_PATH` | `local_setup.ps1` just filled it with every ROS package — replacing it loses them |

A good configure ends with:

```
-- Found OpenCV 4.10.0 in C:/opencv/build/x64/vc16/lib
-- Found rclcpp: 32.0.2 (C:/dev/ros2-lyrical/share/rclcpp/cmake)
-- Using RMW implementation 'rmw_fastrtps_cpp' as default
-- Found moil_interfaces: 2.1.0 (.../ros/install/share/moil_interfaces/cmake)
```

If OpenCV names `.pixi/envs/default/Library` instead of `C:/opencv/build`, conda's copy won — fix `-DOpenCV_DIR` and reconfigure.

---

## A11. Build

```powershell
& $cmake --build build-win-ros --config RelWithDebInfo --parallel
```

**`RelWithDebInfo`, never `Debug`.** The ROS 2 Windows binaries are Release-only; a Debug build links the debug CRT against them and breaks at run time.

Output: `build-win-ros\RelWithDebInfo\moil_fisheye_cali.exe`

---

## A12. Run

```powershell
.\tools\run_windows_ros.ps1
```

That script sources both `local_setup.ps1` files, builds `PATH` in the right order, sets the Qt plugin path, and starts the app from the repo root. It checks each dependency first and names the step above if one is missing.

By hand, if you prefer:

```powershell
. C:\dev\ros2-lyrical\local_setup.ps1
. .\ros\install\local_setup.ps1
$env:PATH = "C:\Qt\6.8.1\msvc2022_64\bin;C:\opencv\build\x64\vc16\bin;C:\dev\ros2-lyrical\.pixi\envs\default\Library\bin;$env:PATH"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "C:\Qt\6.8.1\msvc2022_64\plugins\platforms"
.\build-win-ros\RelWithDebInfo\moil_fisheye_cali.exe
```

These variables live only in the current PowerShell window. Close it and they are gone — that is why the launcher exists.

The `.exe` **cannot be double-clicked.** Without this environment it exits instantly with no message at all. That silence is a missing environment, not a broken build.

| Missing | Symptom |
|---|---|
| either `local_setup.ps1` | instant silent exit — ROS / typesupport DLLs unresolved |
| pixi `Library\bin` on `PATH` | instant silent exit — spdlog / tinyxml2 / yaml-cpp / openssl unresolved |
| Qt `bin` on `PATH` | "Qt6Quick.dll was not found" |
| OpenCV `bin` on `PATH` | "opencv_world4100.dll was not found" |
| `QT_QPA_PLATFORM_PLUGIN_PATH` | "could not load the Qt platform plugin windows" |

Allow the Windows Firewall prompt on **private** networks — DDS discovery needs it.

---

## A13. Connect to the rig

**The app does not connect on startup.** It creates its ROS context when you press **Update** in the Server panel.

**First, confirm the rig is reachable.** This needs `pixi shell`:

```powershell
pixi shell --manifest-path C:\dev\ros2-lyrical\pixi.toml
```

then inside it:

```powershell
. C:\dev\ros2-lyrical\local_setup.ps1
. D:\moil_fiseheycali_cpp\moil-fisheye-calisys\ros\install\local_setup.ps1
$env:ROS_DOMAIN_ID = "42"
ros2 node list
exit
```

Expect `/moil_axis`, `/moil_camera`, `/moil_monitor`.

- **Listed** → the network is fine. Run the app, set the domain to **42** in the Server panel, press **Update**, and watch the three dots.
- **Empty** → the app will not connect either, and no code change will help. Same LAN? Firewall allowed on private? Rig powered on and its nodes running?

> The domain is **not** an environment variable for the app. It sets it in code from the Server panel field — default **42**, range 0–232. The `ROS_DOMAIN_ID` export above is only for the `ros2` CLI.

### Expected noise

Pressing Update prints a wall of these:

```
[XTYPES_TYPE_REPRESENTATION Error] Type ... already registered locally
rcutils_set_error_state() ... This error state is being overwritten
```

**Cosmetic.** `RosServerProbe` and `AxisController` each create their own `rclcpp::Context`, so the same message types get registered twice into Fast DDS's process-global `TypeObjectRegistry`. Lyrical ships Fast DDS 3.6, which has that registry and complains; Jazzy's Fast DDS 2.x does not, which is why Linux is silent. Seeing these means the app *is* talking to DDS.

`QWindowsWindow::setGeometry: Unable to set geometry` is also cosmetic — the window is larger than your screen work area.

### Next time

Everything from here on is the four-command loop in **A0** at the top of this part.

---
---

# Part B — Ubuntu 24.04

ROS is on by default here, so there is one build directory and one set of commands.

---

## B1. apt packages

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build libopencv-dev libeigen3-dev ros-jazzy-desktop
```

---

## B2. Qt 6.8+ from the Qt installer, not apt

apt ships **Qt 6.4.2**, too old: `CMakeLists.txt` sets `qt_policy(SET QTP0004 NEW)`, which needs **6.8+**.

Install Qt 6.11.1 with the online installer from <https://www.qt.io/download-qt-installer>, kit **Desktop gcc 64-bit**. It lands at `~/Qt/6.11.1/gcc_64`.

**Check:**

```bash
ls ~/Qt/6.11.1/gcc_64/lib/cmake/Qt6/Qt6Config.cmake
```

> `qmake6` on `PATH` still points at the apt 6.4.2, so it is not a reliable check. Always pass `-DCMAKE_PREFIX_PATH` explicitly, or CMake silently finds 6.4.2 and fails on the policy.

---

## B3. Build `moil_interfaces`

```bash
mkdir -p ~/moil_ros_ws/src
# copy or symlink the moil_interfaces package sources into ~/moil_ros_ws/src/, then:
cd ~/moil_ros_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select moil_interfaces
```

**Check:**

```bash
ls ~/moil_ros_ws/install/moil_interfaces
```

> On the miniPC this already exists. There are ~30 stale copies of `moil_interfaces` under `~`; `~/moil_ros_ws` is the canonical one. See `docs/MINIPC_ROS_CONNECT.md`.

---

## B4. Configure

The two `source` lines are **required**. `rclcpp` comes from `/opt/ros/jazzy`, but `moil_interfaces` is a per-user overlay CMake cannot see unless sourced.

```bash
cd ~/Fisheye-Calibration-System        # <- your clone

source /opt/ros/jazzy/setup.bash
source ~/moil_ros_ws/install/setup.bash

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
```

Skip them and you get `Could not find a package configuration file provided by "moil_interfaces"`.

---

## B5. Build

```bash
cmake --build build -j6
```

---

## B6. Run

```bash
./build/moil_fisheye_cali
```

**No sourcing at run time.** `CMakeLists.txt` passes `-Wl,--disable-new-dtags` when ROS is on, turning RUNPATH into an RPATH that propagates to transitive dependencies. Trade-off: the absolute path to `~/moil_ros_ws/install/moil_interfaces/lib` is baked in, so if that overlay moves, delete `build/` and reconfigure.

Needs a real display session — at the machine or over X/Wayland forwarding, not a plain `ssh moilpc`.

---

## B7. Connect to the rig

Confirm discovery from the shell first:

```bash
source /opt/ros/jazzy/setup.bash
ROS_DOMAIN_ID=42 ros2 node list
```

Expect `/moil_axis`, `/moil_camera`, `/moil_monitor`. Then run the app, set domain **42** in the Server panel, press **Update**, watch the dots.

Empty list means it is the network:

| Symptom | Fix |
|---|---|
| different subnet | same LAN on both |
| firewall blocks DDS | `sudo ufw allow proto udp from <subnet>` |
| multicast blocked (office Wi-Fi) | `export ROS_STATIC_PEERS=<rig-ip>` on **both** ends |
| rig uses a Fast DDS discovery server | `export ROS_DISCOVERY_SERVER=<rig-ip>:11811` + `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`, both ends |
| RMW mismatch | `export RMW_IMPLEMENTATION=rmw_fastrtps_cpp` on both |

### Ubuntu daily loop

```bash
cmake --build build -j6 && ./build/moil_fisheye_cali
```

Works from a fresh shell with no sourcing — `build/CMakeCache.txt` remembers where `moil_interfaces` is, and the baked RPATH covers runtime. Sourcing only returns when you reconfigure.

---
---

# Part C — macOS

**macOS cannot reach the rig.** Mac Wi-Fi drops inbound multicast, which is exactly what ROS 2 discovery relies on. See `docs/BACKEND_INTEGRATION.md`. This part is for UI work only.

```bash
brew install cmake ninja qt6 opencv eigen
xcode-select --install

cd ~/Desktop/Fisheye_Calibration-Jojo_Version      # <- your clone
/opt/homebrew/bin/cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6"
/opt/homebrew/bin/cmake --build build -j6
./build/moil_fisheye_cali
```

`-DCMAKE_PREFIX_PATH` is required — Homebrew keeps Qt outside CMake's default search path.

---
---

# Part D — Reference

## D1. What to rebuild after what

| Changed | Configure | Build |
|---|---|---|
| `CMakeLists.txt`, or **added** a `.qml` file | yes | yes |
| any `.cpp` / `.h` | — | yes |
| any **existing** `.qml` file | — | yes |

**Editing QML requires a rebuild.** `qt_add_qml_module` compiles the `.qml` files into the binary; the app never reads them from disk. A **new** `.qml` file must also be listed under `QML_FILES` in `CMakeLists.txt` — leave it out and the build passes, then the engine fails at run time.

---

## D2. The 3D Verification sub-app

`subapp_3d_verification/` is the old QWidget + uic dialog, copied verbatim from branch `v2.0_2026_main-cpp-ros`. Clicking *3D Verification* calls `SubAppWindows::openMeasure3d()`, which opens it as its own top-level window. It is an offline image-file workflow and needs no rig.

**Its sources are byte-identical to files that also exist in `cpp/` on the v2.0 branch, and that is not a sign they are dead.** They were ported here deliberately — `subapp_3d_verification/` on 2026-08-28, `subapp_center_setup/` on 2026-08-31 — and both are live CMake targets. Byte-identity says where the code came from, not whether it is still in use; check `git log -- <path>` before concluding anything from it.

```
subapp_3d_verification/
├── app/                 Help.h  Help.cpp
├── controllers/         controller_auto_3d_measurement.h  .cpp
├── core/
│   ├── measure3d/       compute engine -- no Qt, only OpenCV + Eigen
│   └── moildev/         Moildev.h  Moildev.cpp
├── views/
│   ├── Point3dGlView.h  .cpp       QOpenGLWidget 3D point viewer
│   └── widgets/         UI3d_measurement.h  UiFullScreen.h
└── ui/                  UI3d_measurement.ui   <- Designer reference only
```

- **`ui/UI3d_measurement.ui` is not a CMake source, deliberately.** `views/widgets/UI3d_measurement.h` is already the committed `uic` output, named without a `ui_` prefix so AUTOUIC ignores it. List the `.ui` as a source and AUTOUIC generates a competing `ui_UI3d_measurement.h`.
- **To change the layout:** edit the `.ui` in Designer, then regenerate straight to the non-prefixed name:
  ```powershell
  C:\Qt\6.8.1\msvc2022_64\bin\uic.exe subapp_3d_verification\ui\UI3d_measurement.ui -o subapp_3d_verification\views\widgets\UI3d_measurement.h
  ```
  The committed header came from uic 6.4.2, so the first regeneration produces a large version-formatting diff on top of your edit.
- `SubAppWindows::openMeasure3d()` passes `nullptr` for the axis and camera clients — the dialog stores and never dereferences them.

`subapp_center_setup/` is the same arrangement for the *Center Setup* dialog, except that its `ui/center_setup.ui` **is** an AUTOUIC source and works only because the target sets `AUTOUIC_SEARCH_PATHS` — the `.cpp` lives in `controllers/`, the `.ui` in `ui/`.

The app is `QApplication`, not `QGuiApplication`, so these dialogs can exist. That is also why `Qt6::Widgets` is a dependency.

Output goes to `image_cali/output_3D/`, relative to the working directory the app started from.

---

## D3. When it fails

| Error | Cause | Fix |
|---|---|---|
| `Could NOT find Qt6` | CMake cannot see Qt | pass `-DCMAKE_PREFIX_PATH` |
| `QTP0004` policy error | Qt older than 6.8 | point at a 6.8+ install |
| `Could NOT find OpenCV` | wrong `OpenCV_DIR`, or extracted to the wrong place | A4 |
| `Could not find ... "Eigen3"` | Eigen unzipped rather than installed | A5 |
| `Could not find ... "moil_interfaces"` | ROS overlay not sourced | A10's two lines, or B4's |
| `'AxisController': undeclared identifier` in `*_qmltyperegistrations.cpp` | `target_include_directories(moil_fisheye_cali PRIVATE src)` is missing — the generated file guards includes with `__has_include`, so an unreachable header fails *silently* | restore that line |
| `Project file does not exist. Switch: <target>.vcxproj` | target does not exist, usually an unsaved `CMakeLists.txt` | save, reconfigure |
| `Cannot open compiler generated file ... Permission denied` | two builds running at once | wait, rebuild |
| `does not match the generator used previously` | build dir made by a different generator or CMake | delete it, reconfigure |
| exe exits instantly, **no message** | ROS environment missing | all of A12 |
| `Qt6Quick.dll was not found` | Qt `bin` not on `PATH` | A12 |
| `opencv_world4100.dll was not found` | OpenCV `bin` not on `PATH` | A12 |
| dots red, *"this build has no ROS 2 support"* | configured without `-DFISHEYE_ENABLE_ROS=ON` | A10 |
| `XTYPES_TYPE_REPRESENTATION Error ... already registered locally` | duplicate type registration from two `rclcpp::Context`s | cosmetic, see A13 |
| `error while loading shared libraries: libmoil_interfaces__rosidl_generator_c.so` | built before the RPATH fix | delete `build/`, reconfigure, rebuild |

Copy the **whole** error, not the last line — the useful part of a CMake error is usually in the middle.

---

## D4. Source layout

```
src/                     QML app C++: HttpServerProbe (HTTP dots), RosServerProbe (ROS dots),
                         AxisState + AxisController (live axis, jog, stop),
                         CameraController (captures, slots, live preview),
                         MonitorController (brightness, images, screen mapping),
                         PatternController (render + show a pattern spec),
                         ComputeController (/compute/detect: centres, curves, nodes),
                         CalibrationController (Excel, the cali pipeline, plot series),
                         ImageStore (the captures, once, shared by camera and compute),
                         PatternIo (local file I/O and path<->URL),
                         SubAppWindows (C++/QML touchpoint, owns the two sub-app windows)
subapp_3d_verification/  the copied QWidget 3D dialog (D2)
subapp_center_setup/     the copied QWidget centre-setup dialog (D2)
qml/windows/             top-level ApplicationWindows
qml/panels/              feature blocks, one file per panel
qml/controls/            small reusable pieces
qml/Theme.qml            singleton: colours, spacing, font sizes
Server/v2.1.0/           the rig server: one process, eight ROS nodes, plus common/,
                         the calibration engine. Built by its own build_server.bat,
                         NOT by this app's CMakeLists
ros/moil_interfaces/     the ROS 2 contract -- 37 srv, 6 action, 4 msg. SOURCE, and
                         tracked. build/ install/ log/ beside it are colcon output
                         and ignored
doc/                     auto_center_design.md + the Docusaurus documentation site
tools/                   run_windows_ros.ps1 -- the Windows launcher
assets/                  images compiled into the binary
docs/                    long-form notes (D6) -- note the plural; not doc/
```

Build directories are gitignored and per-machine — different architectures and Qt installs cannot share one.

---

## D5. Documentation

| Doc | For |
|---|---|
| `docs/RUNNING.md` | the full build and run reference for both machines |
| `docs/MINIPC_ROS_CONNECT.md` | how the miniPC reaches the rig over ROS 2 |
| `docs/BACKEND_INTEGRATION.md` | ROS 2 from zero, and the network findings the other two build on |
| `Server/README.md`, `Server/v2.1.0/README.md` | the rig server: build, run, and the eight nodes |
| `doc/moilcalib_documentation/` | the Docusaurus site (run it locally; the Pages copy is stale) |
| `cpp/README.md` on `v2.0_2026_main-cpp-ros` | the original Lyrical + pixi write-up Part A is based on |

The rest of `docs/` is local-only and does not ship -- `.gitignore` keeps only these three.

---

## D6. Porting a panel

1. Create `qml/panels/<PanelName>.qml`.
2. Add it to `QML_FILES` in `CMakeLists.txt`.
3. Replace the matching placeholder in the relevant window.
4. Live data goes on an existing controller singleton, or a new one, as `Q_PROPERTY` / `Q_INVOKABLE`.

There is no `setContextProperty`. C++ types reach QML through `QML_ELEMENT`, which is what lets `qmllint` see them statically. `SubAppWindows`, `HttpServerProbe`, `RosServerProbe`, `AxisController`, `CameraController`, `MonitorController`, `PatternController`, `ComputeController`, `CalibrationController` and `PatternIo` are `QML_SINGLETON` too, so QML calls them directly — `SubAppWindows.openMeasure3d()` — rather than instantiating them.

---

## D7. What the UI is wired to

**Wired 2026-09-07 — every panel now reaches a backend.** Before this, roughly half
the UI emitted signals nothing listened to: the buttons were enabled, the click did
nothing, and no message said so. The audit and the wiring are recorded here because
"looks connected" is the failure mode this whole app is prone to.

| UI | Goes to |
|---|---|
| Server panel → **Update** | opens *all six* ROS links at once (axis, camera, pattern, monitor, detect, cali) plus the probe |
| Axis Control panel | `AxisController` — jog, drive-to-limit, home, stop |
| Camera panel → Capture / Pos / Neg | `/camera/capture`, into the `single` / `positive` / `negative` slots |
| Camera panel → **Pair Shot** | `ShowPrepared("positive")` → capture → `ShowPrepared("negative")` → capture, sequenced in C++ |
| Camera panel → Open Img | reads a file into the slot the view is showing, so a pair can be re-analysed with no rig |
| Camera panel → **Direction Diff** | `histogram_8dir` + `nodes_8dir` on the current pair |
| Camera panel → FOV | `CameraController.fov`, shared with the parameter box |
| Live Camera → Go Live / Snapshot | subscribes `/camera/image_raw/compressed` while on; Snapshot keeps a frame, *labelled as a preview frame* |
| Centering → **Find Pos / Find Neg** | `auto_center`. A refused fit clears the centre — it is never rounded up into a plausible coordinate |
| Centering → click in Manual | `roi_exact`, seeded by the click and settled by the rig |
| Centering → **Edge** checkbox / Radius / Colour / Thickness | draws the edge ring on the camera preview, the magnifier and the live view, per polarity |
| Pattern panels → **Auto Update** | `qml/controls/AutoRefresh.qml` — re-renders when the spec fingerprint changes, debounced by `Theme.autoUpdateDelay`, and only while the compute link is up. The fingerprint includes `layerRevision` because a `ListModel` edit is invisible to a binding |
| Histogram panels ×2 | the curves and crossings from `histogram_8dir`. Empty until a Direction Diff has run |
| Monitor slots → Update / Turn off / Browse / brightness | `set_brightness`, `show_pattern`, `close_pattern`. Turn off closes the pattern only — it does **not** zero the brightness, which is a monitor hardware setting and would leave a black screen indistinguishable from a failed close |
| Setup Monitor Direction | `describe_screens`, `set_display_direction`, `show_display_number` |
| **Prepare Patterns** | `prepare_patterns` — required before Pos/Neg/Pair will work |
| Pattern panels → Update / Show | `/compute/render_pattern`, `/monitor/show_pattern_spec` |
| Cali Result → Load/Save Excel | `/compute/xlsx` — the bytes go to the rig, a grid comes back |
| Cali Result → Calculate / Aggr / Clean Noise / Update Table | `/compute/cali` |
| Cali Result → the plots and the six coefficients | `/compute/series` |

**How to check this yourself, and how the first pass got it wrong.** The audit that
produced this table originally enumerated `signal` declarations and checked each
had an `on…` handler. That finds a button whose click goes nowhere. It does *not*
find a control whose handler works fine and writes to a property nothing reads —
which looks identical:

```qml
CheckBox { checked: root.positiveEdge; onToggled: root.positiveEdge = checked }
```

Eight Centering controls and three Auto Update switches passed the signal test and
were dead. The check that catches them is the other direction: for every declared
property, find a *consumer* — a binding that renders it or a controller call that
sends it. `grep` for the property name and subtract the declaration and the
control's own two lines; if nothing is left, it is a dead control.

Also wired, 2026-09-07:

| UI | Goes to |
|---|---|
| Camera → **Pos / Neg** | now a *measurement*: `ShowPrepared(polarity)` then capture, so a shot named after a polarity is taken against it. They previously grabbed whatever was already on the glass and filed it under the wrong name |
| **Calibration System** combo | applies that rig's pixel sizes and H/V gaps to the table, from the profiles in `Server/v2.1.0/config/cali_system/` (mirrored in `qml/panels/CaliSystems.js`) |
| Parameter tab → pixel sizes, H/V gaps ×4 | the fields the combo writes, now visible and editable so they can be confirmed before computing |
| **Overlap** tab | `global_ict_alpha` — every enabled round pooled, which is the only view that shows rounds disagreeing with *each other* |
| **Aggregation** tab | the three `CaliJob` distance searches, with live progress and a working Cancel |
| **Graphs** tab | `ict_zfl` per round, full-tab |
| **Stop** | now cancels a running search for real; still advisory for the plain services |

One thing deliberately did **not** become a button that does something:

- **Stop on a plain service.** With a `CaliJob` search running, Stop cancels it
  properly. With only `/compute/cali` or `/compute/xlsx` outstanding there is
  nothing to cancel — those have no cancel in the protocol — so Stop drops the
  replies this client is waiting for and says exactly that. The op still finishes
  on the rig; claiming otherwise would be the lie.
