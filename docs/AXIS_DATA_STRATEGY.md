# How the axis panel gets its data

This records a decision, not an implementation detail.
The rule it protects is one the rig's own interface files state directly, and breaking it makes the machine slower to command rather than only slower to display.

## The constraint

The axis controller reaches its sensors and its position counter over a serial link.
Reading one sensor is a blocking round trip on that link, and the reply timeouts are 100 ms on the Arduino and 300 ms on the CRUX.
Reading one axis fully means four sensor reads plus one position read, so five round trips.

Motion commands travel over the same link.
Every sensor read the client asks for is time a move command spends waiting in line behind it.
`ros/moil_interfaces/srv/AxisWatch.srv` and `srv/AxisSensor.srv` in the reference repository both say this in their own words, and `AxisSensor.srv` states that the on demand service is the default path while the streaming topic is opt in.

## What the reference app does

`../moil-fisheye-calibration-system/cpp/src/controllers/controller_main.cpp` is the working implementation, and its strategy is:

- `initSensorStatus()` reads all twenty sensors once, when the client connects.
- While nothing is moving it reads nothing at all. The link carries no client traffic.
- `startAxisMonitor(axis, action)` is called only after a move, home, or drive to limit command has been issued. It polls that one axis, and no other, with `kMonitorIntervalMs = 20` between reads.
- That monitor exits as soon as the axis reads not moving twice in a row, or after sixty seconds.
- One final sensor read paints the resting state through `applyUiAxisStopOnSensor()`.

The shape is: fast on one axis while that axis is under command, nothing otherwise.

## What we do

Our panel differs from the reference in one way that matters.
It shows live sensor lamps and gates the direction pads on `lowBlocked` and `highBlocked`, so it cannot go completely silent while idle the way the reference does.
The minimum deviation is therefore a slow sweep rather than no sweep.

`AxisController` reads one axis per pass over `<namespace>/sensor` and `<namespace>/position`, then waits `kPollGapMs` before starting the next axis.
Five axes make one lap.
This is well under one read per second per axis, comfortably inside the ceiling `AxisWatch.srv` describes.

We do not subscribe to `<namespace>/state`, and we do not call `<namespace>/watch`.
An earlier revision asked for all five axes at 4 Hz on connect, which is one hundred round trips per second on the link the motion commands need, and it never sent the stop request on disconnect, so the rig kept streaming to nobody.
That path was removed rather than repaired, because when motion is wired the correct call is per axis and issued at move time, which is not the shape that code had.

## Why the staleness limit is twelve seconds

`AxisState` marks an axis stale when its last reading passes `kStaleMs`, and a stale axis reports both limit directions as blocked, which is the fail safe.

The limit has to exceed one worst case lap.
Five axes at five round trips each, at the CRUX timeout of 300 ms, is 7.5 seconds, plus the gaps between axes.
Three seconds would trip on a healthy rig and lock the pads for no reason, so the limit is twelve.

The cost is that a limit switch reading may be up to twelve seconds old when a move is allowed.
The reference does not have this exposure because it re-reads the axis around every command rather than relying on a background sweep.

## What the motion work must add

When jog, drive to limit, home, and stop are wired to the rig, two things belong in that change and are not present today:

1. Re-read the axis immediately before issuing the move, and decide on that reading rather than on the sweep. This is what closes the twelve second window above.
2. Start a per axis stream for the axis under command, by calling `<namespace>/watch` with that one axis and `hz = 0.0` so the server picks its own configured rate, and stop it when the move ends. The stop request must be allowed to complete during teardown, not abandoned.

`kIdleNeeded = 2` in `AxisState` is the reference's rule for deciding an axis has stopped, and it is only meaningful at the fast per axis rate.
It has no useful effect during the slow idle sweep.
