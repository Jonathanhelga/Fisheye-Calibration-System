---
id: ros-architecture
slug: /ros/architecture
title: ROS 2 Architecture
---

# ROS 2 Architecture

How the client and the rig are wired together in version 2.0, and what replaced
the three HTTP servers of version 1.1.

Read this once. Every "why can it not see the rig" answer follows from it.

To *bring the nodes up*, see [Server Installation](../installation/server.md). To
*diagnose a connection*, see
[Connect the App to the Rig](./connect-to-rig.md).

---

## 1. Client and rig

```
   ┌──────────────────────────┐   ROS 2 / DDS    ┌───────────────────────────┐
   │  moilcali (client)       │  domain ID 42    │  Rig machine (servers)    │
   │  ── cpp/ ──              │ <──────────────> │  ── Server/v2.0.0/ ──     │
   │  Windows .exe            │   same subnet    │  /moil_camera             │
   │  or Ubuntu build         │                  │  /moil_axis               │
   │  or Docker               │                  │  /moil_monitor            │
   └──────────────────────────┘                  └───────────────────────────┘
```

The application is the **client**. Without the rig's nodes running it opens
normally and every device stays idle — **that is a missing rig, not a broken
app.**

---

## 2. What replaced HTTP

| | Version 1.1 | Version 2.0 |
|---|---|---|
| **Axis** | `POST` to port `8000` | services `/axis/move`, `/axis/command`, `/axis/sensor` |
| **Monitor** | `POST` to port `8001` | services `/monitor/show_pattern_spec`, `/monitor/set_brightness`, … |
| **Camera** | `POST` to port `8002` | topic `/camera/image_raw/compressed`, service `/camera/capture` |
| **Addressing** | You type three URLs into the client | Automatic discovery on `ROS_DOMAIN_ID=42` |
| **Server language** | Python 3.8.10 + FastAPI | ROS 2 nodes |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE MAIN WINDOW'S HTTP SERVER URL PANEL IS GONE</div>
  <div>
    Section 1 of the v1.1 <a href="/moilcalib_documentation/docs/v1.1/system-overview/main-window">Main Window Reference</a> documented three address fields and their Update buttons. They are <strong>not in the form at all</strong> in v2.0. There is nothing to configure, and nothing on screen that tells you whether discovery succeeded.
  </div>
</div>

---

## 3. The interface contract

`ros/moil_interfaces/` holds the `.msg` and `.srv` definitions that the client
and the rig's servers **both build against**. This package is the contract; if
the two sides disagree about it, nothing works.

| Kind | Files |
|---|---|
| Messages | `AxisSensors.msg` |
| Axis services | `AxisMove.srv`, `AxisCommand.srv`, `AxisSensor.srv` |
| Monitor services | `ShowPattern.srv`, `ClosePattern.srv`, `SetBrightness.srv`, `GetBrightness.srv`, `SetDisplayDirection.srv`, `GetDisplayDirection.srv`, `MonitorCommand.srv` |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE PACKAGE MUST BE BUILT BEFORE THE APP — AND THE APP USES THE *INSTALLED* COPY</div>
  <div>
    The application compiles against the interfaces in <code>~/moil_ros_ws</code>, <strong>not</strong> the ones in your source tree. Editing a <code>.srv</code> and rebuilding only the app changes nothing. Rebuild the package first:
    <br /><br />
    <code>cd ~/moil_ros_ws && colcon build --packages-select moil_interfaces</code>
  </div>
</div>

---

## 4. Where this lives in the code

| Device | Client-side file |
|---|---|
| Camera | `cpp/src/models/device/camera_ros_client.{h,cpp}` |
| Axis | `cpp/src/models/device/axis_ros_client.{h,cpp}` |
| Monitor | `cpp/src/models/device/monitor_ros_client.{h,cpp}` |
| Domain ID | `cpp/src/models/device/ros_domain_env.h` |

**The application forces `ROS_DOMAIN_ID=42` on itself**, in the ROS client
constructors. You never export it for the application — only for CLI checks.

The rig-side nodes are in `Server/v2.0.0/packages/`, and they are built and run
**on the rig's machine**, not here.

### The nodes are only a transport layer

The real hardware logic — the axis serial protocol, the win32/DDC-CI display
control — lives in `mvc_model/moil_axis` and `mvc_model/moil_monitor`, **outside
this repository**. The rig's launchers point at them through `MOIL_AXIS_PATH` and
`MOIL_MONITOR_PATH`.

> **If that folder is moved or deleted, the axis and monitor stop working** even
> though the nodes start normally. The camera is self-contained and unaffected.

---

## 5. Why discovery constrains the network

One fact explains every networking rule in this documentation:

> **DDS peers advertise the address they expect replies on, and discovery is
> bidirectional.**

From that follow all of:

| Rule | Because |
|---|---|
| Both machines must be on the **same subnet** | Discovery uses UDP multicast, and multicast does not cross a router |
| **NAT breaks it** — Docker bridge networks, Docker Desktop's VM, WSL2's default `172.x` | The advertised address is unreachable from the other side |
| A **one-directional firewall rule** produces "the rig can ping me but I hear nothing" | Only your inbound half is blocked, and discovery needs both |
| `ping` proves nothing | An open UDP port and a blocked one look identical from outside |

Two physically separate LANs need a VPN or overlay network putting both machines
on one subnet before any of this can work.

---

## 6. Why the first window takes a minute

Before drawing anything, the application probes the rig's ROS services with a
**5-second timeout each**. On a machine that cannot see the rig, this looks like
nothing is happening.

It is not hung. Give it the full minute before deciding something is wrong.

---

## 7. What Windows adds

The packaged Windows build runs the identical Linux binary inside a private WSL2
distribution, drawn on screen by WSLg. That adds one constraint the Linux build
does not have:

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ ON WINDOWS THE RIG'S ADDRESS IS REQUIRED, NOT OPTIONAL</div>
  <div>
    WSL2 mirrored networking <strong>forwards multicast out and drops it coming back</strong>, so ROS 2's normal discovery never completes: the rig hears the client, and the client never hears the rig. Naming the rig makes discovery unicast, which does get through.
    <br /><br />
    Leaving the address blank at install time produces a working application that will never find the rig.
  </div>
</div>

---

## Diagrams this page needs

No application screenshots — this page is architecture. Two diagrams would carry
it, and both are worth drawing rather than capturing:

| File | What it should show |
|---|---|
| `ros-architecture-topology.png` | Client, rig and the three nodes, with the topic and service names on the arrows |
| `ros-discovery-bidirectional.png` | Why a one-directional firewall rule fails — the rig's announcement arriving, the client's reply blocked |

The second is the one that saves people an afternoon. The ASCII diagram in §1
covers the first adequately if drawing time is short.

---

## Source material

| What | Where |
|---|---|
| Architecture, node table, domain ID | `README.md` (repo root) |
| The contract | `ros/moil_interfaces/msg/`, `ros/moil_interfaces/srv/` |
| Client-side ROS clients | `cpp/src/models/device/` |
| Rig-side node packages | `Server/v2.0.0/packages/` |
| Why the rig's address is required on Windows | `Windows/v2.0.0/RELEASE-v2.0.0.md` |
