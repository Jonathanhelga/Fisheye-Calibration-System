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

The narrowing happens **before** the move request goes out, not after.
`jog` sets the focus and then queues the request, and the worker loop applies a pending focus change ahead of draining its command queue.
The order is the whole point.
At the idle shape the client is asking for five axes at 4 Hz, which is one hundred serial round trips a second against a link that measures out at 5.7, so the server's serial port is permanently backlogged and a move request lands at the end of that queue.
Narrowing first drops the standing request to five round trips a second, and the move then reaches the rig without waiting behind four axes nobody is looking at.
An earlier revision narrowed after dispatch and paid the full backlog on every press.

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

## Why a reading has to repeat before it is believed

`AxisState` treats an isolated reading and a sustained one differently, in two places, and both exist because a single odd sample used to reach the direction pads and grey them out for seconds.

A `sensor_moving` of triggered sets `moving` immediately while a command of ours is pending, and otherwise needs `kMotionNeeded` consecutive triggered readings.
Without that, one stray triggered sample after a move had already finished put the axis back into moving, which then took `kIdleNeeded` further readings to leave, so every jog ended with the pads flicking blue, grey, and blue again a second or two apart.
`AxisHome.action` in the reference repository names the cause directly: the moving lamp flickers as the stage settles.

An unreadable limit sensor keeps its previous value for one sample, and falls to unreadable on the second.
Unreadable still means blocked, which is the fail safe `AxisState.msg` insists on, and this does not weaken it: a sensor that is genuinely unreadable is unreadable on the next sample too.
What it removes is the single dropped read, which over a saturated link with 100 ms and 300 ms reply timeouts is common, and which used to grey one arrow for a full sample interval.

## What is still missing

Jog and stop are wired, over `<namespace>/move` and `<namespace>/command`.

Drive to limit is not, and it needs `AxisLimitMove.action`.
`wireLimitMoves` in the reference explains why a client side watch a sensor then stop loop was rejected: a single `AxisMove` caps at 487.5 mm on the wire, which Z's roughly 530 mm of travel exceeds outright, and the stop would depend on the network link staying up.

Per axis homing is not wired either, but it does **not** need an action.
`/axis/command` with `command = "home"` is the same `AxisCommand` service the STOP button already uses, and `commandAvailable` is already true whenever STOP works.
It sends the axis to its origin sensor and returns immediately, so the arrival still has to be watched, and `AxisZero.srv` is what records the software zero once the axis is standing on that sensor.
`AxisHome.action` is only required for the All Home flow, which has to block until each axis has arrived before starting the next.
An earlier version of this document said homing needed an action, and that was wrong.

One item from the original plan is still open.
The controller does not re-read the axis immediately before issuing a move, so the decision to allow that move rests on a reading that may be up to twelve seconds old.
Narrowing the watch under command shortens the window after the move starts, but it does not close the window before it.

`kIdleNeeded = 2` in `AxisState` is the reference's rule for deciding an axis has stopped.
It is meaningful at the focused rate, where two readings are about two seconds, and close to useless at the idle rate, where the same two readings span nearly nine.
