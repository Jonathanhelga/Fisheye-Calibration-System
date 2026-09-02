---
id: server
slug: /installation/server
title: Server Installation (ROS 2 Rig Nodes)
---

# Server Installation (ROS 2 Rig Nodes)

Bringing up the three ROS 2 nodes on the **rig's machine**, so the client can
find them.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⛔ THIS IS NOT THE VERSION 1.1 SERVER</div>
  <div>
    Version 1.1 ran <strong>Python 3.8.10 + FastAPI</strong> on ports 8000, 8001 and 8002, and was explicitly unchanged from version 1.0. Version 2.0 replaces it entirely with ROS 2 nodes.
    <br /><br />
    An existing v1.1 server does <strong>not</strong> work with a v2.0 client, and a v2.0 client cannot be pointed at one. There is no URL to point.
  </div>
</div>

---

## 1. What the rig must expose

Three nodes. This is the contract the client is built against:

| Node | Provides |
|---|---|
| `/moil_camera` | topic `/camera/image_raw/compressed`, service `/camera/capture` |
| `/moil_axis` | services `/axis/move`, `/axis/command`, `/axis/sensor` |
| `/moil_monitor` | services `/monitor/show_pattern_spec`, `/monitor/set_brightness`, … |

All three run in **LAN / multicast mode** — no discovery server — so they only
work within the same subnet as the client.

---

## 2. What is in the folder, and what is not

`Server/v2.0.0/` contains **only** what these three launchers need:

```
run_axis_lan.bat          axis node
run_monitor_lan.bat       monitor node
run_camera_ros_lan.bat    camera node
build_ros.bat             rebuild after code changes
packages/
  moil_interfaces/        custom .srv / .msg types
  moil_axis_ros/          axis node
  moil_monitor_ros/       monitor node (includes pattern_render.py)
  moil_camera_ros/        camera node
build/  install/  log/    created by build_ros.bat — not source
```

### Three things live outside, and must stay where they are

| Requirement | Location | Used by |
|---|---|---|
| ROS 2 Lyrical + `pixi` | `C:\dev\lyrical` | all three |
| **Axis hardware module** | `mvc_model/moil_axis` in the calibration repo, via `MOIL_AXIS_PATH` | `run_axis_lan.bat` |
| **Monitor hardware module** | `mvc_model/moil_monitor` in the calibration repo, via `MOIL_MONITOR_PATH` | `run_monitor_lan.bat` |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE NODES ARE ONLY A TRANSPORT LAYER</div>
  <div>
    The actual hardware logic — the axis serial protocol, the win32/DDC-CI display control — stays in the calibration repository. <strong>If that repo is moved or deleted, the axis and monitor stop working</strong>, even though the nodes start normally and report no error.
    <br /><br />
    The paths are written <strong>directly inside</strong> <code>run_axis_lan.bat</code> and <code>run_monitor_lan.bat</code>. If the location changes, edit them there.
    <br /><br />
    The camera does not need the repo — its code is self-contained.
  </div>
</div>

---

## 3. Build

The first time, and after any code change:

```
build_ros.bat
```

It builds four packages. Three rules decide whether it succeeds:

### Stop every node first

If a node is still running, the build fails with **`Permission denied`**, because
the `moil_interfaces` DLL is in use. The error names the DLL, not the running
node, so it reads as a corrupt build when it is not.

### The build must live at a short path

Windows limits paths to 260 characters and `rosidl` creates deeply nested
directories. The supplied folder is short enough — **moved somewhere much deeper,
the build fails with "Cannot open source file"**.

### The camera does not need rebuilding

`run_camera_ros_lan.bat` sets `PYTHONPATH` at `packages\moil_camera_ros`, so
camera changes take effect immediately. **The axis and monitor run from
`install\`, so both must be built.**

---

## 4. Run

Double-click each launcher. **One window per node — leave all three open.**

```
run_axis_lan.bat          axis node
run_monitor_lan.bat       monitor node
run_camera_ros_lan.bat    camera node
```

Closing a window stops that node's hardware.

---

## 5. Operating rules

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ STOP WITH Ctrl+C, NOT TASK MANAGER</div>
  <div>
    This matters for the axis: force-killing skips <code>close_serial()</code>, and the next start reads sensor values that are <strong>garbage but look plausible</strong>. You get numbers, they are wrong, and nothing flags it.
  </div>
</div>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ ONLY ONE run_axis_lan.bat AT A TIME</div>
  <div>
    Two instances fight over COM3/COM4 and the second gets <strong>"Access is denied" permanently</strong> — not until the first exits, permanently, until the port is released.
  </div>
</div>

---

## 6. Verify — on the rig, before blaming the client

```powershell
ros2 node list
```

On domain 42, all three should be listed. Do this **on the rig itself** first: if
they are missing here, the problem is the rig, and nothing on the client side
will help.

Once they are up, move to the client and follow
[Connect the App to the Rig](../ros/connect-to-rig.md).

A verified reference run, from the repository:

```
build_ros.bat            4 packages completed
run_axis_lan.bat         axis hardware ready (yuanman)
run_monitor_lan.bat      monitor hardware ready + display mapping restored
run_camera_ros_lan.bat   streaming /camera/image_raw/compressed
```

---

## 7. First-time rig setup

After the nodes are up, one more step before any calibration means anything:

**Map the physical displays to compass directions.** Until you do, patterns go to
the wrong screens. The procedure is in
[Monitor Viewer → Setup Monitor Direction](../calibration/monitor-viewer.md#2-setup-monitor-direction),
and it is done from the client, not here.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Build fails with `Permission denied` | A node is still running. Stop all three first |
| Build fails with "Cannot open source file" | The folder is too deep. Windows' 260-character path limit |
| Axis sensor values are plausible but wrong | The previous run was force-killed and skipped `close_serial()`. Restart properly |
| Axis reports "Access is denied" | Another `run_axis_lan.bat` holds COM3/COM4 |
| Node starts but the hardware does nothing | The hardware module outside the repo has moved. Check `MOIL_AXIS_PATH` / `MOIL_MONITOR_PATH` in the `.bat` |
| Camera code change had no effect | It should — the camera runs from source. For the axis and monitor, rebuild |
| Nodes visible on the rig, invisible from the client | A network problem. [Connect the App to the Rig](../ros/connect-to-rig.md) |

---

## Screenshots this page needs

Terminal output rather than UI:

| File | What to capture |
|---|---|
| `server-three-launchers.png` | The three launcher windows side by side, each showing its ready line |
| `server-node-list.png` | `ros2 node list` on the rig returning all three |

---

## Source material

| What | Where |
|---|---|
| Full procedure, folder contents, technical notes | `Server/v2.0.0/README.md` |
| The launchers | `Server/v2.0.0/run_{axis,monitor,camera_ros}_lan.bat`, `build_ros.bat` |
| The `.msg`/`.srv` contract | `ros/moil_interfaces/` |
| Node packages | `Server/v2.0.0/packages/moil_{axis,monitor,camera}_ros/` |
