---
id: client-windows-native
slug: /installation/client/windows-native
title: Native Windows Build (ROS 2 Lyrical + MSVC)
---

# Native Windows Build (ROS 2 Lyrical + MSVC)

A real Windows `.exe` — no WSL and no Linux anywhere.

This works because ROS 2 **Lyrical** ships a prebuilt Windows binary archive
(installed through `pixi` / conda-forge) and the application's ROS surface is
small: only `rclcpp` and `sensor_msgs`.

<div className="custom-note">
  <div className="custom-note-title">📌 THIS BUILD MATCHES THE RIG'S ROS DISTRO</div>
  <div>
    The rig's nodes run <strong>Lyrical</strong> — all four launchers in <code>Server/v2.0.0/</code> run under <code>pixi --manifest-path C:\dev\lyrical</code> and source <code>C:\dev\lyrical\local_setup.bat</code>. So does this build. <strong>Same distro on both ends</strong>, which is the combination DDS supports.
  </div>
</div>

## Which client matches the rig

Worth knowing before you pick a route, because the repository's own READMEs are
inconsistent on this point:

| Side | ROS distro | Where that comes from |
|---|---|---|
| **The rig** | **Lyrical** | `Server/v2.0.0/*.bat` — all four |
| **This build** (native MSVC) | **Lyrical** | This page |
| Ubuntu / `setup.sh` | Jazzy | `ROS_DISTRO_WANT="${ROS_DISTRO:-jazzy}"` |
| Docker | Jazzy | `FROM ros:jazzy` |
| Packaged `.exe` | Jazzy | `Windows/v2.0.0/RELEASE-v2.0.0.md` |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE READMEs SAY THE RIG IS JAZZY — THE LAUNCHERS SAY LYRICAL</div>
  <div>
    <code>README.md:321</code> and <code>cpp/README.md:92</code> both warn that this build "will not see a Jazzy rig". That describes a rig configuration the launchers in this repository <strong>do not</strong> use, and the warning appears to predate the rig moving to Lyrical.
    <br /><br />
    Cross-distro DDS genuinely is unsupported, so <strong>which routes reach the rig depends on which distro the rig is actually running at your site</strong>. Confirm it with <code>ros2 node list</code> from the client you intend to use, before trusting either README.
  </div>
</div>

---

## 1. Prerequisites

| | |
|---|---|
| Windows 11 | |
| VS 2022 Build Tools | |
| Qt | `C:\Qt\6.8.1\msvc2022_64` |
| OpenCV | `C:\opencv\build` |
| Eigen | `C:\eigen3` |
| pixi | `winget install prefix-dev.pixi` |
| Execution policy | `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned` |

---

## 2. Install ROS 2 Lyrical

Unpack `ros2-lyrical-<date>-windows-AMD64.zip` from
[ros2/ros2 releases](https://github.com/ros2/ros2/releases) so that `pixi.toml`
sits in `C:\dev\ros2-lyrical\`, then:

```powershell
cd C:\dev\ros2-lyrical
pixi install
pixi run python preinstall_setup_windows.py
```

---

## 3. Set up the environment — order matters

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ Launch-VsDevShell REWRITES PATH WHOLESALE, SO IT GOES FIRST</div>
  <div>
    Run it after <code>pixi shell</code> and it discards the pixi environment. Also: <strong>one <code>pixi shell</code> per window</strong> — nesting corrupts <code>PATH</code>.
  </div>
</div>

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cd C:\dev\ros2-lyrical
pixi shell
. C:\dev\ros2-lyrical\local_setup.ps1

# Your clone. Later commands use $repo, so set it here — inside the pixi shell,
# which is a child process and does not inherit variables from the outer one.
$repo = "D:\moil_fiseheycali_cpp\moil-fisheye-calisys"   # <- change to your path
```

---

## 4. Build the interfaces, then the app

```powershell
cd $repo\ros
colcon build --merge-install --base-paths .
. $repo\ros\install\local_setup.ps1

cmake -S $repo\cpp -B $repo\cpp\build-win -G "Visual Studio 17 2022" -A x64 `
  -DQt6_DIR="C:/Qt/6.8.1/msvc2022_64/lib/cmake/Qt6" `
  -DOpenCV_DIR="C:/opencv/build" `
  -DEigen3_DIR="C:/eigen3/share/eigen3/cmake" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.1/msvc2022_64;C:/eigen3"

cmake --build $repo\cpp\build-win --config RelWithDebInfo
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE EXPLICIT *_DIR PINS ARE REQUIRED, NOT OPTIONAL</div>
  <div>
    The pixi environment ships its <strong>own</strong> Qt6, OpenCV and Eigen from conda-forge, and would otherwise win the search. Omit the pins and you get a build against the wrong libraries.
  </div>
</div>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ BUILD RelWithDebInfo, NEVER Debug</div>
  <div>
    The ROS 2 Windows binaries are <strong>Release-only</strong>. A Debug build will not link.
  </div>
</div>

On Windows the interfaces are built **in-tree**, into `ros/install`, rather than
into a separate workspace as on Linux.

---

## 5. Run — use the launcher, not the `.exe`

```powershell
& $repo\cpp\packaging\moilcali-run.ps1
```

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ DOUBLE-CLICKING moilcali.exe FAILS SILENTLY — THE SINGLE BIGGEST TIME SINK ON THIS BUILD</div>
  <div>
    Without the ROS 2 and pixi environments it needs for its DLLs, the binary <strong>exits instantly with no error message whatsoever</strong>. The prompt simply comes back:
    <br /><br />
    <code>PS D:\...\cpp&gt; .\build-win\RelWithDebInfo\moilcali.exe</code><br />
    <code>PS D:\...\cpp&gt;</code>
    <br /><br />
    Nothing is broken. The environment just was not set up. Do not start it from a plain PowerShell window either.
  </div>
</div>

### What `moilcali-run.ps1` does

The Windows counterpart of the Linux `moilcali-run` wrapper:

- sources both `local_setup.ps1` files,
- orders `PATH` so **your** Qt and OpenCV beat the conda-forge copies,
- sets `QT_QPA_PLATFORM_PLUGIN_PATH`,
- starts the app **from `cpp\`**, so `config/`, `database/` and `image_cali/`
  resolve,
- and **reports which piece is missing** instead of failing silently.

Override paths with `-Ros2`, `-Qt`, `-OpenCvBin`, `-Config`.

---

## 6. Known gaps

| Gap | Detail |
|---|---|
| **Excel** | Needs `zip.exe` and `unzip.exe` on `PATH` — neither exists on Windows by default. `choco install zip unzip` |
| **No Debug builds** | The ROS Windows binaries are Release-only |
| **Distro must match the rig** | Confirm with `ros2 node list` — see the table above |

---

## 7. Run the tests

```powershell
Get-ChildItem cpp\build-win\RelWithDebInfo\*_test.exe | ForEach-Object { "== $_ =="; & $_ }
```

Covers `CaliCompute`, `CaliMath`, `CaliRound`, `Database`, `Measure3d`, `Moil3d`,
`MoilCali`, `PatternGen`, `Regression`, `Xlsx`.

They exercise the compute core — every number the application reports — without
needing a rig, so they are the fastest way to confirm a build is sound before you
take it to hardware.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| `moilcali.exe` exits instantly, no message | The environment is not set up. Use `moilcali-run.ps1` |
| CMake picked the wrong Qt or OpenCV | The `*_DIR` pins were omitted; conda-forge's copies won |
| Link errors in a Debug build | ROS Windows binaries are Release-only. Build `RelWithDebInfo` |
| `PATH` behaving strangely | `Launch-VsDevShell` was run after `pixi shell`, or two pixi shells are nested |
| `$repo` is empty inside the pixi shell | It was set in the outer shell. Set it inside |
| Excel import/export fails | `zip` / `unzip` are not on `PATH` |
| The rig is never discovered | Check the rig's distro matches — `ros2 node list`. If it does, it is an ordinary network problem: [Connect the App to the Rig](../ros/connect-to-rig.md) |

---

## Screenshots this page needs

Terminal output rather than UI:

| File | What to capture |
|---|---|
| `native-win-silent-exit.png` | The prompt coming straight back after `.\moilcali.exe` — the failure that costs an hour |
| `native-win-launcher-ok.png` | `moilcali-run.ps1` reporting each piece it found, then the app starting |
| `native-win-tests.png` | The test loop passing |

---

## Source material

| What | Where |
|---|---|
| Full sequence with verification checks | `cpp/README.md` → *Windows 11 (native, ROS 2 Lyrical — no WSL)* |
| Condensed version | `README.md` → *Windows 11 — native MSVC build* |
| The launcher | `cpp/packaging/moilcali-run.ps1` |
