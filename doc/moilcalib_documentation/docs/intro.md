---
id: intro
slug: /intro
title: Calibration Result System
---

# Calibration Result System (v2.0)

The **Calibration Result System** calibrates fisheye cameras from end to end.
A full run moves through five steps, and the client drives all of them:

1. **Show the pattern.** Generate a calibration pattern and push it to the monitors surrounding the camera.
2. **Move into position.** Drive the 5-axis stage until the camera sits at the spot the current round calls for.
3. **Capture.** With the camera in place, take the positive and negative shots.
4. **Compute.** Derive the camera parameters from the captured images across all rounds.
5. **Verify and store.** Confirm the parameters can be trusted, then save them.

What you actually launch and work in is a **desktop client** — the same C++17 / Qt6
application as version 1.1. The client never touches the hardware directly.

What changed in version 2.0 is **how it reaches the hardware**. The client no
longer sends HTTP requests to three FastAPI servers. Both sides are now fully
**ROS 2**: every device on the rig is a ROS node, and the client finds them by
ROS 2 discovery on `ROS_DOMAIN_ID=42`.

Version 2.0 tracks the [`calibration_inROS`](https://github.com/perseverance-tech-tw/moil-fisheye-calisys/tree/calibration_inROS) branch of the `moil-fisheye-calisys` repository.

<div className="custom-note custom-warning">
  <div className="custom-note-title">🚧 VERSION 2.0 IS STILL IN DEVELOPMENT</div>
  <div>
    The application is roughly 90&nbsp;% complete and these docs are being written alongside it. The page tree and sidebar are final; most pages are still skeletons. For a complete, finished reference use <a href="/moilcalib_documentation/docs/v1.1/intro"><strong>version 1.1</strong></a> — everything about the calibration procedure itself is unchanged.
  </div>
</div>

---

## The Big Change: HTTP Is Gone

In version 1.1 the client held three URLs and posted to ports `8000`, `8001` and
`8002`. There are **no URLs to fill in any more.** The client and the rig find
each other by ROS 2 discovery, on a domain ID the application sets itself.

| | Version 1.1 | Version 2.0 |
|---|---|---|
| **Client** | C++17 + Qt6 | **C++17 + Qt6 — unchanged in kind** |
| **Transport** | HTTP to ports 8000 / 8001 / 8002 | **ROS 2 / DDS on `ROS_DOMAIN_ID=42`** |
| **Server** | Python 3.8.10 + FastAPI on Windows 11 | **ROS 2 nodes** (`Server/v2.0.0/`) on the rig machine |
| **How they connect** | You type the rig's URL into the client | Automatic discovery — no address to type (except on the Windows installer, see below) |
| **Client platforms** | Ubuntu, macOS | Ubuntu 24.04, **Windows 11 (packaged installer)**, Docker, native Windows/MSVC |

```
   ┌──────────────────────────┐   ROS 2 / DDS    ┌───────────────────────────┐
   │  moilcali (client)       │  domain ID 42    │  Rig machine (servers)    │
   │  Windows .exe            │ <──────────────> │  /moil_camera             │
   │  or Ubuntu build         │   same subnet    │  /moil_axis               │
   │  or Docker               │                  │  /moil_monitor            │
   └──────────────────────────┘                  └───────────────────────────┘
```

The three nodes the rig must expose:

| Node | What it provides |
| --- | --- |
| `/moil_camera` | publishes `/camera/image_raw/compressed` |
| `/moil_axis` | services under `/axis/{move,command,sensor}` |
| `/moil_monitor` | services under `/monitor/*` |

**Without those nodes running, the application opens but every device stays
idle** — that is a missing rig, not a broken app. How to check, and what to do
when the list comes back empty, is in
[Connect the App to the Rig](./ros/connect-to-rig.md).

---

## What Changed in Version 2.0

The calibration procedure, the hardware, and the results you get are the same as
version 1.1. Five changes are visible while you work.

### 1. There is a Windows installer

`MoilCali-2.0.0-setup.exe` is one file, ~293 MB. The operator double-clicks it
and gets a Start Menu entry — no ROS, no Qt, no Docker, no Linux knowledge on
the machine. See [Windows (Installer)](./installation/client-windows.md).

The macOS route from version 1.1 is **not** carried over: ROS 2 Jazzy is
packaged for Ubuntu 24.04, and the client now builds against it.

### 2. The rig's address matters again — but only on Windows

WSL2 mirrored networking forwards multicast out and drops it coming back, so
ROS 2's normal discovery never completes on the packaged Windows build. The
installer asks for the rig's IP to make discovery unicast instead. Leaving it
blank installs a working application that will never find the rig.

### 3. New tool: PCT Recommend

Paste the measured (PCT, ICT) rows from a round and the window fits a gain curve,
then proposes a fresh PCT series whose ICT steps are as even as the pattern's
integer PCT units allow. See [PCT Recommend](./calibration/cali-result/pct-recommend.md).

### 4. Load to System actually downloads now

Version 1.1 listed this as a known limitation: the Database window could browse
records but never fill the round tables, because the measured values live in
cloud storage rather than in the database. Version 2.0 implements the download
over the **Microsoft Graph API**, so a shared calibration folder can be pulled
straight into the system. See [Load to System](./database/load-to-system.md).

Credentials are **not** compiled into the binary any more — the application asks
for them and keeps them in memory for the session unless you tick *Remember on
this computer*.

### 5. The Calibration Data window was rewritten

The database browser was rebuilt to match the original Python window: a
*Search Information* panel on the left with one field per column, and a table on
the right that switches between the camera list and, on double-click, that
camera's rounds and files.

---

## Known Limitations in Version 2.0

**The rig's address must be entered on Windows.** Covered above — it is required,
not optional.

**No single-instance guard.** Launching the Windows shortcut twice runs two
copies, both writing to the same capture folder.

**The client's ROS distro has to match the rig's.** Cross-distro DDS is not
supported, so a mismatched client never discovers the rig however the network is
configured. The rig runs **Lyrical** — all four launchers in `Server/v2.0.0/` go
through `C:\dev\lyrical` — and so does the
[native Windows/MSVC build](./installation/client-windows-native.md). The Ubuntu,
Docker and packaged-installer routes are built on **Jazzy**.

Note that the repository's own `README.md` and `cpp/README.md` describe the rig as
Jazzy, which its launchers contradict. **Confirm your rig with `ros2 node list`
from the client you intend to use** rather than trusting either document.

**Excel import and export shell out to `zip` and `unzip`.** `setup.sh` does not
install them. Without `unzip`, *Load All Excel* reports *"No round subfolders
(1..10) with .xlsx found"* even when the files are clearly there. Install both
with `sudo apt install -y unzip zip`. The packaged Windows installer already
contains them.

**The rig and the client must be on the same subnet.** ROS 2 discovery uses UDP
multicast, and multicast does not cross a router. Two different physical LANs
need a VPN or overlay network putting both machines on one subnet first.

---

## Where to Start

Begin with [Installation](./installation/server.md). Bring the rig's three ROS
nodes up first, then install the client — the packaged Windows installer if you
just want to use the application, or the Ubuntu build if you are going to change
it.

Before expecting anything to work, confirm the client can see the rig:
[Connect the App to the Rig](./ros/connect-to-rig.md). An empty node list is a
network problem, not an application problem, and diagnosing it as an application
problem is the most common way to lose an afternoon here.

Next read the [Main Window Reference](./system-overview/main-window.md), which
describes the window every calibration is run from.

After that the sidebar follows the order of a real calibration, from pattern
setup through to the database. One thing worth knowing before you begin: the
**PCT Pattern Generator** and **Monitor Viewer** are step 1, not side utilities.
The pattern has to be generated and pushed to the correct monitor before any
capture means anything.
