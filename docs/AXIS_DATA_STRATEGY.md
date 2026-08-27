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

`AxisController` opts in to the server side stream by calling `<namespace>/watch`, then subscribes to `<namespace>/state`.
It runs that watch in two shapes.

**Idle: all five axes.**
Every axis needs a fresh limit reading or its direction pads fail safe to disabled, so the resting shape is the full set.
Measured on the rig this yields 1.15 messages per second in total, which is one update per axis every 4.3 seconds.
The link is saturated at this shape, which is acceptable only because nothing is competing for it.

**Under command: one axis.**
The moment a jog is dispatched the watch narrows to the axis being moved, at `kFocusWatchHz`.
That axis then updates roughly once a second instead of once every 4.3 seconds, which is what releases the direction pads promptly after the move rather than eight seconds later.
The other four go stale meanwhile, which costs nothing, because `guardCommand` already refuses to move a second axis while one is under command.

The watch widens back to all five as soon as nothing is busy, including when the move fails.

`kFocusWatchHz` is 1.0 rather than the saturating value on purpose.
One axis at 1 Hz is five round trips a second against a measured ceiling of 5.7, so the link keeps a margin.
That margin is what a STOP command travels through, and it is the reason not to ask for the highest rate the focused axis could technically sustain.

If the rig does not serve `<namespace>/watch`, the controller falls back to polling `<namespace>/sensor` and `<namespace>/position` one axis per pass, waiting `kPollGapMs` between axes.
An earlier revision asked for all five axes at 4 Hz and never sent the stop request on disconnect, so the rig kept streaming to nobody.
The stop request is now sent when the session ends.

## Why the staleness limit is twelve seconds

`AxisState` marks an axis stale when its last reading passes `kStaleMs`, and a stale axis reports both limit directions as blocked, which is the fail safe.

The limit has to exceed one worst case lap.
Five axes at five round trips each, at the CRUX timeout of 300 ms, is 7.5 seconds, plus the gaps between axes.
Three seconds would trip on a healthy rig and lock the pads for no reason, so the limit is twelve.

The cost is that a limit switch reading may be up to twelve seconds old when a move is allowed.
The reference does not have this exposure because it re-reads the axis around every command rather than relying on a background sweep.

## What is still missing

Jog and stop are wired, over `<namespace>/move` and `<namespace>/command`.
Drive to limit and homing are not, because the rig serves those as ROS actions rather than services and they need a different client.

One item from the original plan is still open.
The controller does not re-read the axis immediately before issuing a move, so the decision to allow that move rests on a reading that may be up to twelve seconds old.
Narrowing the watch under command shortens the window after the move starts, but it does not close the window before it.

`kIdleNeeded = 2` in `AxisState` is the reference's rule for deciding an axis has stopped.
It is meaningful at the focused rate, where two readings are about two seconds, and close to useless at the idle rate, where the same two readings span nearly nine.
