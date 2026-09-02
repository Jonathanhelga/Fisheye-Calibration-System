# MoilCali server 2.1.0

The rig server: **one process, eight ROS nodes**, sharing one context. Replaces the
three separate launchers of [v2.0.0](../v2.0.0/README.md).

```
/moil_axis      /moil_camera   /moil_monitor    /moil_compute
/moil_session   /moil_measure3d  /moil_jobs     /moil_supervisor
```

Its design principle is that **the client computes nothing**: the calibration
maths, the 3D pipeline and the pattern rendering belong here, and the client is a
window onto it. That is why this tree carries `common/engine` — the same
`CaliCompute`, `CaliMath`, `Regression` and `Moildev` sources the client has.

**That is the target, not the finished state.** The migration is under way on
`v2.0_2026_main-cpp-ros_server-calculation-migration`. ICT 8-direction detection,
PCT pattern rendering and the pos/neg shots run here already; whatever has not
moved yet still runs client-side. v2.1.0 is done when nothing does.

## Running it after a clone

```bat
build_server.bat     :: once, and after every pull that touches the code
run_server.bat       :: leave this window open -- it IS the server
check_server.bat     :: PASS/FAIL per layer; run from a client machine too
```

**No editing step, and no paths to set.** `scripts\moil_env.bat` is the one place
that knows where anything on this machine is, and it *searches* rather than
assumes: Qt, OpenCV, Eigen, the ROS distro and MSVC are each looked for in the
environment, then in `deps.local.bat`, then in `third_party/`, then in the usual
places on a Windows dev box. Everything inside the tree is derived from the
script's own location, so the folder can be moved or renamed and the scripts keep
working. To see what it resolved:

```bat
scripts\moil_env.bat --check
```

Override a path only if that reports it missing or wrong: copy
`deps.local.bat.example` to `deps.local.bat` and uncomment the one line you need.
`deps.local.bat` is gitignored, which is the point — the rig pulls new server code
regularly, and paths edited into a tracked file would conflict on every pull.

> Replaced `server_env.bat`, which had to be created and edited before the first
> build could run and carried the rig PC's own paths as its defaults. Anything
> still referring to `server_env.bat` is out of date.

**The build output does not land in this folder.** It goes to `C:\moil_ws` by
default, off the drive root, because Windows caps a path at 260 characters and
`rosidl` nests about 120 characters deep on its own — a clone somewhere deep
overflows and fails with "Cannot open source file" naming a header that exists,
which reads as a missing dependency and is not. `build_server.bat` stamps the
build root with the tree that owns it and wipes it if a different checkout claims
it, so two clones cannot silently compile each other's sources. Set `MOIL_WS_BASE`
in `deps.local.bat` to move it.

Stop the server before building: a running server holds the `moil_interfaces` DLL
open and the build fails with "Permission denied".

`build_server.bat` builds in two stages, sourcing `install\setup.bat` between them,
because a single `colcon build` over both packages does not reliably put
`moil_interfaces` on `CMAKE_PREFIX_PATH` for `moil_server` — which fails in a way
that reads like the interfaces did not build when they built fine.

Everything runs plain LAN multicast on `ROS_DOMAIN_ID=42`, with
`ROS_DISCOVERY_SERVER` and the Fast DDS profile variables deliberately blanked. A
leftover discovery-server setting makes discovery find nothing at all, which looks
like a broken network and is not.

## What is here

```
common/      the calibration engine -- the ONLY copy in this branch
  CMakeLists.txt  builds it as the moil_common static library
  device/      axis, camera, monitor hardware
  engine/      algorithm, cali, compute, pattern, session
  io/  measure3d/  moildev/
packages/
  moil_server/       the eight nodes
                     (the contract is ../../ros/moil_interfaces -- see above)
config/      cali_system, camera_parameters.json, devices.json
scripts/
  moil_env.bat       finds Qt, OpenCV, the ROS distro and MSVC; no absolute
                     path may appear anywhere else in this tree
```

`common/` is a CMake library rather than three lists of `../../common/...` paths
written out in the package that consumes them: a consumer links `moil_common` and
inherits the include paths, the Qt and OpenCV targets and the MSVC flags this code
cannot compile without. It carries a `COLCON_IGNORE` because it is deliberately
**not** a ROS package — nothing in it knows about rclcpp.

Several units are split across files, each with a private header saying what is in
which: `axis_backend.h`, `monitor_device_p.h`, `CaliCompute_p.h`,
`measure3d_node_p.h`.

Not included: `build/`, `install/`, `log/` — colcon output, which now lands in
`C:\moil_ws` rather than here.

## The contract lives at the repository root

`moil_interfaces` is **not** in this folder. It is
[`ros/moil_interfaces`](../../ros/moil_interfaces) at the repository root — 37
services, 6 actions, 4 messages — and both the server and the client build from
it. `build_server.bat` stage 1 reaches out with `--paths ..\..\ros\moil_interfaces`.

It was deliberately consolidated there on 2026-08-19, having briefly existed as two
copies. ROS matches services by **type hash**: a definition differing by one
character makes every call of that service fail to connect, while topics keep
flowing — so the rig looks alive, no button works, and nothing is logged beyond the
client not finding a server. With one copy that cannot happen; with two it was a
thing someone had to remember.

## Verified

Against the live rig from a client machine on 2026-08-19:

- All eight nodes visible on domain 42.
- All 12 services the current client calls resolve with the expected types:
  `/axis/{move,command,sensor,position}`, `/camera/capture`,
  `/monitor/{show_pattern,close_pattern,set_brightness,get_brightness,set_display_direction,get_display_direction,command}`.
- Read-only round trips succeed: axis position, sensor state, panel brightness.

~~**Not verified here:** this tree has not been built on this machine.~~
**Built and run from this tree since 2026-08-26**, with `build_server.bat`
producing `C:\moil_ws\install\moil_server\moil_server.exe` and `run_server.bat`
serving a live rig from it — most recently on 2026-09-01, carrying the
`/axis/watch` fix below. The clone-and-pull flow this tree was vendored for is
therefore proven end to end, build included.


## Fixed 2026-09-01 — `/axis/watch` polled 50× faster than it was asked to

`axis_node.cpp` / `axis_node.h`. No interface change: `AxisWatch.srv` is
untouched, so nothing needs rebuilding on the client side.

**How it looked.** The server console said everything was fine —

```
axis       OK        yuanman: arduino=COM3 crux=COM4
```

— while the client showed `?` in every axis sensor field and logged
`[axis] timeout waiting response: /axis/sensor`. The rig's own log carried the
two halves of the story without connecting them:

```
[INFO] [moil_axis]: /axis/watch: 5 axis/axes at 0.1 Hz
[WARN] [moil_axis.rclcpp]: failed to send response to /axis/sensor (timeout):
       client will not receive response
```

Measured from a client machine on domain 42 while this was happening:
`/rig/info` and `/axis/position` answered instantly, `/axis/sensor` answered
**correctly, after 18.7 seconds**, and `/axis/state` arrived at 1.2 Hz against a
5 Hz target. Discovery, the type hash and the network were never involved.

**What was wrong — and it is not "we polled faster than the Arduino can take".**
Read this way it sounds like somebody picked a bad number. Nobody did, and that
difference *is* the bug.

The client asked for a **slow** rate: 0.1 Hz, one sweep every 10 seconds. That
is well within what the Arduino and the CRUX can handle. The server threw the
request away and kept polling at its own default of 5 Hz. So the fault was never
"we chose a bad number" — it was "**the number had no effect**". `stateTimer_`
was built once, in the constructor, from the `axis_watch_hz` parameter
(default 5.0 = every 200 ms), and `onWatch` saved `req.hz` into `watchHz_`,
returned it in `res.hz`, printed it in the log — and never rebuilt the timer that
actually does the polling.

That is why it stayed hidden: **the rig log said 0.1 Hz while the rig was doing
5 Hz.**

**And too fast, on its own, would not have produced 19-second waits.** If sweeps
had run one at a time, polling flat out would only mean "the bus is always busy"
— a sensor request slots in between round trips and still comes back quickly.
What made the wait unbounded is that the timer sat on `jobGroup_`, which is
**Reentrant**, under a `MultiThreadedExecutor`. One sweep is five blocking serial
round trips **per axis** — four sensors and a position — so five axes is 25 round
trips against reply timeouts of 100 ms (Arduino) and 300 ms (CRUX), and it cannot
finish inside 200 ms. Every time a sweep missed that deadline the next tick
**started another sweep on another thread** instead of waiting. Sweeps stacked
up, each queuing 25 more round trips onto the one thread that owns the ports, and
the wait grew and grew. That is where 18.7 seconds came from. The
`failed to send response` warnings are the tail of it: the answer was eventually
computed, the client had long given up, and the reply had nowhere to go.

So, three faults, in order of importance:

1. **The requested rate never reached the timer** — the real bug.
2. **Missed ticks spawned overlapping sweeps instead of being skipped** — what
   turned a slowdown into a pile-up.
3. **The log and the service reply reported the requested rate, not the actual
   one** — what hid both.

The fix addresses all three, which is also why the rate is now clamped *and*
reported back honestly: if some future client asks for 50 Hz it gets 5 and is
told so, rather than being told 50 and quietly getting something else.

Opening a client made it worse rather than better — its startup sweep is twenty
more sensor reads onto a bus already at 100%, which is why this read as "the
connection drops when the user opens the client".

**What changed.**

| | |
|---|---|
| `applyWatchTimer(double hz)` | new, and the **only** place `watchHz_` is assigned — it clamps, computes the period and rebuilds `stateTimer_` when the period actually changes. A rate that is not in a timer is not a rate. |
| `watchGroup_` | new **MutuallyExclusive** group for the poll timer, replacing `jobGroup_`. A tick arriving mid-sweep is now dropped, which is the correct answer to "the rig cannot go this fast". |
| `res.hz` and the log line | now report the rate **in force** after clamping, and the timer's real period: `/axis/watch: 5 axis/axes at 0.10 Hz (every 10000 ms)`. A client asking for more than the link can carry is told what it is getting. |

Rates are clamped to **0.01–5.0 Hz**. The low bound has to be genuinely low: the
old `std::max(0.5, watchHz_)` floor would have raised a 0.1 Hz request to 0.5 Hz
even with the timer rebuilt. The high bound is roughly what two serial ports can
carry. `axis_watch_hz` still sets the starting rate; `/axis/watch` overrides it
per request, and `{axes: [], hz: 0.0}` is still the resting state — no polling at
all.

**Checking it on a rig.** The log line now states the period, so it can be read
directly. To confirm from a client machine on domain 42:

```powershell
ros2 service call /axis/watch moil_interfaces/srv/AxisWatch "{axes: [], hz: 0.0}"
Measure-Command { ros2 service call /axis/sensor moil_interfaces/srv/AxisSensor "{name: 'x_org'}" }
# sub-second on an idle rig; it was 18.7 s

ros2 service call /axis/watch moil_interfaces/srv/AxisWatch "{axes: ['x','y','z','yaw','pitch'], hz: 0.1}"
ros2 topic hz /axis/state    # ~0.5 msg/s -- five axes every ten seconds, not flat out
```

Confirmed working on the rig on 2026-09-01.

**Worth knowing while diagnosing this class of fault:** `/rig/info` reports port
state, not throughput, so a fully saturated serial bus still shows `axis OK` on
the console — as it did here for hours. `check_server.bat` rung 4 has the same
blind spot. The signal that separates "the rig is busy" from "the rig is broken"
is the *latency* of a single `/axis/sensor` call, which is why the command above
is worth keeping to hand. The watch is server-wide, so the client holding it may
well be on another machine on the subnet.
