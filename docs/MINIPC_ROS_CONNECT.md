# miniPC → Rig ROS 2 connectivity: session findings

Session date: 2026-08-26.
Follow-up to `BACKEND_INTEGRATION.md` (2026-08-25).

The previous session tried the integration from the MacBook and hit a hard blocker: Mac Wi-Fi drops inbound multicast, which is exactly the traffic ROS 2 LAN discovery relies on. The task then shifted to this miniPC to see if it can do what the Mac could not.

**Bottom line: this miniPC can talk to the rig over ROS 2, right now, over Wi-Fi. No new software was installed to get here. Everything needed was already on disk.**

---

## The one-line result

```
$ ros2 service call /axis/sensor moil_interfaces/srv/AxisSensor "{name: 'x_org'}"
response:
moil_interfaces.srv.AxisSensor_Response(success=True, value=False, message='')
```

A real service call, from this box to the rig, over Wi-Fi, with a valid response. This is stronger than the "wait_for_service" bar the plan set for success — it's a full round trip.

---

## What is already installed on this machine

Nothing needed to be downloaded, apt-installed, pip-installed, built, or configured. All of this was here at the start of the session:

| Thing | Where | State |
|---|---|---|
| ROS 2 Jazzy | `/opt/ros/jazzy` | Installed, sourced in the shell environment |
| ROS 2 Rolling | `/opt/ros/rolling` | Installed alongside Jazzy, unused this session |
| `moil_interfaces` (Jazzy build) | `/home/minipc-103-1/moil_ros_ws/install/moil_interfaces` | Built, sourced via `moil_ros_ws/install/setup.bash` |
| `moil_interfaces` sources | `/home/minipc-103-1/moil_ros_ws/src/moil_interfaces` | Present, matches installed build |
| Reference client repo (`moil-fisheye-calibration-system`) | `/home/minipc-103-1/moil-fisheye-calisys/` | Present. C++ ROS client code at `cpp/src/models/device/{axis,camera,monitor}_ros_client.{h,cpp}`. Note the directory name differs from `CLAUDE.md`'s `../moil-fisheye-calibration-system` — same repo, different local folder name. |
| Qt 6.11.1 | `~/Qt/6.11.1/gcc_64` | Already used by the existing build |
| CMake, Ninja, build-essential | apt | Already installed |
| turtlebot3_ws (unrelated) | `~/turtlebot3_ws/install` | Sourced automatically in `.bashrc`; harmless overlay |
| ~30 other `moil_interfaces` copies | `~/Downloads/...`, `~/20082026/...`, `~/ws_20082026_2/...`, etc. | Ignored. `moil_ros_ws` is the canonical one. |

**Libraries installed by me this session: none.**
**apt / pip / pixi / manual builds performed this session: none.**

The only reason we haven't touched anything is because the plan's Phase A ("test the network before writing any code") passed on the very first try. Phase B onwards (which does require a small amount of new C++ and a `CMakeLists.txt` change) has not been executed yet — the session was paused for this note.

---

## Network reality on this box

- Interface used: `wlo1` (Wi-Fi, SSID **"Lab-103"**), IP `192.168.103.191/24`.
- Ethernet ports `enp2s0`, `enp3s0` exist but are down (no cable). This means we are on the *same class of link* the doc warned about — Wi-Fi. It just happens that this AP does not do the multicast-drop behaviour that the Mac's AP did.
- Rig: `192.168.103.56`. Same subnet.
- Rig MAC per ARP: `04:d4:c4:47:ff:e3` — identical to the one recorded in `BACKEND_INTEGRATION.md` Part 3, so we are definitely talking about the same physical box.

### The one thing that "failed" but is expected

```
$ ping -c 1 -W 1 192.168.103.56
--- 192.168.103.56 ping statistics ---
1 packets transmitted, 0 received, 100% packet loss
```

Ping does not work. This is not a real failure. `BACKEND_INTEGRATION.md` covers this: Windows Firewall on the rig drops ICMP by default. The rig is alive and responds to unicast on other ports; ARP alone proves it's on the network.

---

## Phase A test outcomes (the actual proof)

### A3 quick smoke test — passed on first try

With `ROS_DOMAIN_ID=42` set (rig's channel), everything the doc said we should see is there:

```
$ ros2 node list
/moil_axis
/moil_camera
/moil_compute
/moil_jobs
/moil_measure3d
/moil_monitor
/moil_session
/moil_supervisor
```

All three critical nodes (`/moil_axis`, `/moil_camera`, `/moil_monitor`) are discovered, plus five helper nodes we didn't know about. Services line up too — `/axis/sensor`, `/axis/move`, `/axis/command`, every `/monitor/*` we expected, and `/camera/capture`. The topic `/camera/image_raw/compressed` is present.

### The service round-trip — also passed

```
$ ros2 service call /axis/sensor moil_interfaces/srv/AxisSensor "{name: 'x_org'}"
response:
moil_interfaces.srv.AxisSensor_Response(success=True, value=False, message='')
```

This is the important one. It proves three separate things at once:

1. **Multicast discovery works on this Wi-Fi.** The Mac-era blocker (`BACKEND_INTEGRATION.md` Part 3) does not apply to this SSID.
2. **The rig is actually running.** All three launcher windows must be up on the Windows box for this to succeed. This is external state we did not control — someone had it running.
3. **Cross-distro ROS 2 works for `moil_interfaces`.** This box is on **Jazzy**, the rig is on **Lyrical**, and the `moil_interfaces/srv/AxisSensor` request/response deserialised correctly on both ends. This was the open question in `BACKEND_INTEGRATION.md` Part 5 — "Will a Kilted client talk to a Lyrical server?" We now know the Jazzy answer, which is one distro further out than Kilted, and it works.

### A1 / A2 — skipped, and correctly

The plan had a fallback for cases where A3 failed: run the Appendix B mDNS QU probe (A1) and Appendix A DDS listener (A2). Both would only prove *weaker* claims than the service call already proved. There is no reason to run them.

---

## What was NOT done this session

Everything from Phase B onwards was blocked while writing this note. Concretely, none of the following has happened yet:

- `CMakeLists.txt` has not been changed. It is still Qt-only, no `find_package(rclcpp)`.
- No new C++ files. `RosServerProbe.h` / `RosServerProbe.cpp` do not exist.
- `qml/panels/ServerConfigPanel.qml:214-215` still emits `rosUpdateRequested` with no listener.
- `qml/windows/Main.qml:51` still instantiates `ServerConfigPanel` bare with no handler.
- The app has not been rebuilt.
- The three ROS status dots on the panel still do nothing when the Update button is pressed in ROS mode.

The plan file at `/home/minipc-103-1/.claude/plans/hi-claude-so-my-declarative-taco.md` describes exactly what those steps look like. It is unaffected by this pause.

---

## Open questions the session did not answer

1. **Will this SSID keep behaving?** Wi-Fi APs sometimes rate-limit or throttle multicast under load. A single quiet moment of discovery is not the same as steady-state reliability. If the dots ever start flickering, this is the first thing to check.
2. **Would wired Ethernet be better?** Almost certainly yes. `enp2s0`/`enp3s0` are just down, not broken. A cable eliminates any AP-related multicast risk.
3. **Actions across Jazzy↔Lyrical?** Only the *service* AxisSensor was tested. The `.action` path (`AxisLimitMove`, etc.) uses more moving pieces and could still surface a type-hash mismatch. Not needed for the Server Panel goal, but a landmine for later panels.
4. **Is one of the eight discovered nodes doing something we should worry about?** `/moil_session`, `/moil_supervisor`, `/moil_jobs`, `/moil_compute`, `/moil_measure3d` are all present. The doc only names axis / monitor / camera; these five weren't on the map.

---

## Recommended next steps

In order, all covered by the plan file:

1. Add the ROS 2 block to `CMakeLists.txt`, guarded by a `FISHEYE_ENABLE_ROS` cache option so the Mac build is unaffected.
2. Write `RosServerProbe.h` / `.cpp` mirroring the shape of `HttpServerProbe.{h,cpp}`.
3. In `ServerConfigPanel.qml`, bind the three ROS status dots to `RosServerProbe.{axis,monitor,camera}Status`.
4. In `Main.qml:51`, add the `onRosUpdateRequested` handler that calls `RosServerProbe.probeAll(...)`.
5. Build in a shell that has `/opt/ros/jazzy/setup.bash` **and** `~/moil_ros_ws/install/setup.bash` sourced. Neither is optional — CMake needs both to find `rclcpp` and `moil_interfaces`.
6. Verify by clicking Update in ROS mode with Domain ID 42 (green dots), then Domain ID 0 (red dots).

Update `RUNNING.md` at the same time so the two `source` lines are documented for the next person or the next session.
