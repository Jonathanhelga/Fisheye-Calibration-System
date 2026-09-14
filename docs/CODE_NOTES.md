# Code notes

Why the code in `src/` and `qml/` is shaped the way it is.

This file exists because the source comments were cut down to short labels on 2026-09-10.
Everything below was a long block comment in a source file until then.
It is kept because almost all of it records a real failure that cost someone time, and most of those failures are silent: the build passes, `qmllint` passes, and the wrong number reaches a calibration.

Sections are named after the file the note came from.

## The rule these notes keep restating

A fit that is not trustworthy is reported as **no centre**, never as a plausible-looking coordinate.
The rig moves five axes off these numbers, so a wrong centre is worse than no centre.
`doc/auto_center_design.md` is the specification for the cascade that decides this.

The same rule applies one level up: a partial answer is reported as partial, an absent measurement is reported as absent, and a control that cannot do its job says so rather than doing something adjacent.
Most of the notes below are one instance of this.

## src/ImageStore.h, src/ImageStore.cpp

### Why one store instead of a copy per controller

A capture is wanted by three unrelated pieces of code at three different times, and two of them need a different representation of it.

- `CameraController` receives it and wants a `QImage`, to hand the QML image provider something to paint.
- `ComputeController` sends it back out to `/compute/detect` and wants the **original compressed bytes**.
- The file-open path puts bytes in from disk with no capture involved at all.

One store of `(bytes, format, image)` per slot is what lets Capture, Open Img and Snapshot all feed the same downstream analysis without any of them knowing about the others.

Slot names in use: `single`, `positive`, `negative`, `live`.

### Why the original bytes are kept and never re-encoded

Re-encoding a `QImage` to PNG would hand the detect ops a picture that is not the one the camera produced.
Every centre fit in this system is a measurement of exact pixel values, so that difference is not cosmetic.

This was learned the hard way on 2026-09-08.
`CameraController.cpp` arrived from `v2.1_2026_New-UI-CPP-ROS`, which has no `ComputeController` and therefore no reason to keep the bytes.
Nothing wrote `ImageStore` afterwards, so every detect op sent an empty image.
Find Pos, Find Neg and Direction Diff all failed, and the build and `qmllint` were both clean.

### Why the revision counter is never reset

The revision is what makes the QML image URL change.
Without it Qt serves the cached picture and a new capture appears to do nothing.

`clear()` therefore bumps the revision rather than resetting it.
A slot that is cleared and refilled must not reuse a URL Qt has already cached, or the old picture comes back.

### Threading

Frames are written from ROS executor threads and read from the GUI thread and from the image provider's own thread, so every accessor takes the mutex.
The `QImage` and `QByteArray` copies handed out are implicitly shared, so the copy under the lock is cheap.

`ImageStore` takes its own lock, so callers must not hold `frameMutex` while calling into it.
Nesting the two is a deadlock waiting for a second writer.

## src/CameraController.h, src/CameraController.cpp

### imageSizes is the decoded size, frameSizes is the rig's report

The capture reply carries both, and the two do differ.
The mismatch is shown to the operator as "(the rig reported %1x%2)".

Anything working in image coordinates must use `imageSizes`.
The detect ops decode the same bytes and measure those pixels, so a centre derived from a reported size that disagrees would be off by exactly the difference, silently, because both numbers look like a frame size.

### The live preview

Restored 2026-09-08.
`v2.1_2026_New-UI-CPP-ROS` never had a stream, so the merge left `LiveCameraPanel`'s Start, Stop and Snapshot emitting into nothing.

The topic is 3040x3040 BEST_EFFORT.
Decoding every frame that arrives costs more than the panel can show, so frames are dropped at the subscription rather than queued: roughly 12/s reaches the GUI thread.

The subscription exists only while the operator has the stream on.
Subscribing up front and discarding frames would put a 3040x3040 topic on the wire for the whole session to serve a panel nobody is looking at.

`receiving` is not the same question as `streaming`.
`streaming` says the operator asked for frames; `receiving` says some have arrived.
A rig that publishes nothing leaves the first true and the second false, which is the distinction the panel's status line is made of.

The fps readout is smoothed so it does not flicker between 11 and 19 on a stream that is perfectly steady, and it is cleared on every start rather than carried over, so a stale rate from the previous session is not shown as current.

### fieldOfView

The lens's field of view in degrees.
Not a capture property, and nothing in this class reads it.
It lives here because it describes the camera, and it is written into `camera_parameters.json` beside the fitted coefficients by the Cali Result window's Parameter tab.

Restored after the 2026-09-08 merge, which took the camera path from `v2.1_2026_New-UI-CPP-ROS` wholesale.
That branch never had this property, so `CaliResultParameterPanel`'s binding resolved to `undefined` and the tab showed "undefined" as the FOV, then wrote it into the saved parameters.
`qmllint` caught it; at run time it would have been a plausible-looking file with one wrong field.

### openImage

Reads a picture off disk into a slot, so a capture taken earlier or on another machine can be analysed without the rig.

This is the only way to get an image into the app without a camera, and it is what makes offline work possible.
The detect ops take the bytes in the request, so a file loaded here reaches `/compute/detect` exactly as a fresh capture would.
A compute node needs no hardware, so the whole measurement path runs against a server on this machine.

Restored 2026-09-09.
It existed before the merge from `v2.1_2026_New-UI-CPP-ROS`, which removed the button and the backend together, consistently, so nothing dangled and nothing reported it.

The bytes are kept exactly as they came off disk and the format is taken from the suffix rather than by re-encoding, for the reason in the `ImageStore` section.
The frame is named after the file rather than stamped with a time: this frame was not taken now, and a clock reading beside it would say it was.

### Snapshot

Keeps the newest preview frame as the `single` capture.
Labelled as a preview on purpose: it came off a BEST_EFFORT topic and may predate the pattern now on the glass.
Fine for aiming, not a measurement.

### changed() is the one catch-all notify signal

Every frame binding in the UI depends on it, so setters guard against a repeated write emitting it again.

## src/ComputeController.h, src/ComputeController.cpp

### Nothing here computes

Every op runs on the rig.
This class marshals the captures out of `ImageStore`, sends the original compressed bytes, and turns the result JSON into something QML can bind to.
That boundary is the one described in `ComputeOps.h` and it is not negotiable here: there is no OpenCV in this target and no second implementation of a centre fit to drift from the engine's.

### A refused centre is passed straight through

`auto_center` answers with `ok=false` or `(-1,-1)` when its cascade cannot validate a fit.
That answer reaches `centerRefused()` unchanged and is never rounded up into a plausible-looking coordinate.
A refused fit is the correct answer on a badly aimed shot.

### The degrade to pattern_center

Added 2026-09-11, after the rig answered `unknown detect op: auto_center` while the Widgets client kept drawing its crosshair against the same server.
That is the whole diagnosis: the rig runs a server binary older than this source tree, and `auto_center` was added after it was built.
`pattern_center` is older, the Widgets client's Auto has always called it, and it is demonstrably answering on that rig today.

`applyResult` therefore treats one specific failure as a version signal rather than a refusal.
When `auto_center` comes back with a message containing "unknown detect op", the request is reissued as `pattern_center` and the original failure is never reported.
Every other failure, including a genuine `ok=false`, is still passed straight through as a refusal.

The two ops are not equivalent and the substitution is not silent.
`auto_center` is the validated cascade in `doc/auto_center_design.md`, with the offset, basin and coverage gates; `pattern_center` is the bare gradient fit those gates are applied to.
So the degrade buys an answer at the cost of the validation, which is why it emits a `notice` naming the op and why `centerFound` carries `pattern_center` as its method.
The Centering panel shows that method, and its confidence is empty because this op reports none.

`lastNoiseCleaning_` exists because the reissue happens long after `autoCenter()` returned.
It is the only parameter the two ops share; `expected_rings` is meaningless to `pattern_center`, which counts nothing.

The degrade runs only from `auto_center`, never from `pattern_center`, so a server missing both refuses once and stops.
`degradeToPatternCenter` returns whatever `sendDetect` returned, and a false there falls back into the ordinary refusal path, which is what keeps the no-ROS build and a dead link behaving as before.

The real fix is rebuilding the server, and this does not replace it.

### The in-flight token set

Several ops can be in flight at once: the two histogram panels and a centre fit are independent.
A single "latest token" would drop the older reply **and** leak its busy count, leaving the panel disabled for ever.
Membership in the set is the receipt.

### The timeout

Detect ops have no progress and no cancel (`session_services.cpp:370`), and the `auto_center` cascade is measured in seconds, so the budget is generous.
Without a timeout a request the node never answers leaves the panel disabled with no message, which is the exact silent failure this app is full of elsewhere.

### `slots` is a Qt keyword macro

The parameter is named `slotNames`, not `slots`.
`slots` expands to nothing, so naming it that way silently strips the parameter name and leaves the range-for with no range.
Same for `signals`, `emit` and `foreach`.

### The slot name does double duty

`slot` is both an `ImageStore` key and the name `auto_center` uses to find the prepared PNG it should count rings from.
That PNG is the source it trusts over the modal count inferred from the capture.
See `ComputeDetectOps.cpp`.

### Ordering

Two-image ops send positive first, then negative, which is the order every two-image op documents.

## src/SessionController.h, src/SessionController.cpp

### Why the session path exists

`/compute/detect` takes `sensor_msgs/CompressedImage[] images`: the client uploads the pictures and the node analyses what it was handed.
That is the path this app used until 2026-09-09, and it means every centre fit, every histogram and every node extraction re-sends two 3040x3040 frames the rig took itself and already has on disk.
One Direction Diff is four such uploads.

The session path does not work that way.
A capture is written into a named slot on the rig, and an op names the slot:

```json
{"slots": ["positive", "negative"], "pos_cx": 1520}
```

Named, not sent.
`/session/run_compute` resolves those names to the session's own files, so no image crosses the wire for analysis at all.
The measurement also outlives this client, because it lives in the session rather than in a `QVariantMap` that dies with the process.

This is not a new idea here.
The Qt Widgets client (`cpp/`, on `v2.0_2026_main-cpp-ros`) has done it this way since the server-calculation migration, and `session_ros_client.cpp` is the reference this class was written from, down to the `slots` key and the command vocabulary.
What is new is only that the QML client finally has it.

### One session at a time

The server keeps a current session (`ctx_.sessions->sessionId()`) and `RunCompute` answers from it, so this class holds a single id rather than a set.

`openOrCreate` is the entry point.
`open_or_create` is not a server command: the old client asked for the list, matched by name, opened the match, and created only when there was none.
That two-step is kept rather than collapsed, because `create` on an existing name would silently start an empty session and lose the pair the operator just shot.
Opening an existing session inherits the shots already in it, so the curve panels and Update Table work without re-shooting.

Reopening inherits whatever the session holds, but this client has not been told what that is yet; the server's answer arrives with the next capture or op.
Claiming slots we have not been told about would let a detect op be issued against a slot that is empty on the rig.
`filledSlots` is therefore only ever a copy of the server's answer, never a tally kept on this side, which would drift the moment a session is reopened or another client shoots into it.

### Timeouts

A capture is a mechanical shutter plus a 3040x3040 encode on the rig, and a detect op is a full ring sweep over two of them.
Neither is quick, and a timeout that fires early looks exactly like a rig that ignored the request.

A rejected goal is reported outright.
Without that the call simply never answers and the panel sits busy for ever, which reads as a slow rig rather than a refused goal.

### On disconnect

The session id is dropped.
It belongs to a link that has gone, and keeping it would let an op be sent against a session this client can no longer prove is open.

## src/MonitorController.h, src/MonitorController.cpp (removed 2026-09-14)

### Why it was removed

Two branches each wrote a client for `/monitor`: `MonitorController` on this branch, `PatternController` on `v2.1_2026_New-UI-CPP-ROS`.
After the 2026-09-08 merge every monitor control in QML called `PatternController`, and `MonitorController` was no longer connected on Update.
Its one unique call, `readBrightness`, moved into `PatternController` as `readMonitorBrightness`, and the files were deleted.
The old class is still in git history before this change.

### What went with it, unused at the time

- `describeScreens`, with `screens`, `mappingComplete` and `screensSummary`: asks the server which screens it can see.
- Transcoding a picked image that is not `png` or `jpeg` into PNG before `show_pattern`.

Neither had a QML caller. If screen reporting is wanted, port `describeScreens` from history into `PatternController`.

### Facts that still hold

Direction strings are the server's: `top`, `n`, `w`, `s`, `e`, or `all`, not the UI's labels.
The QML slots carry them in their `direction` property.

An incomplete screen mapping is the most common silent failure.
`show_pattern` answers success for the panels it did reach, so a pattern can land on four screens and the shot is still taken.

## src/CalibrationController.h, src/CalibrationController.cpp

### The client computes nothing

Not the derived columns, not the aggregation, not the regression, not the six polynomial coefficients.
`CaliSeries.srv` says why at length; the short version is that a boundary with an arithmetic exception in it is not a boundary.

What this class holds is the table, which is the operator's working copy and travels whole with every op, plus a **cache** of answers the server gave it.
There is no code here that could produce those numbers, only code that remembers the ones it was given.

The cache is keyed to `tableVersion()`, bumped on every edit and every op.
A redraw, a tab change or a resize reads the cache; the wire is touched only when the table actually changed.
An answer computed from a table that has since been edited is dropped rather than drawn.

### Table geometry

A round table is 77 rows: two header rows then 75 layer rows.
75, not 40.
A real capture's after-bezel segment runs well past layer 40, and a shorter table drops everything below the cut silently.
`CaliTableData::kRows` on the far side is the same table.

The column indices mirror `_dict_column_index` in `CaliCompute.cpp`.
They are the wire format, not a display choice, and must not be tidied.

- `ict_n` .. `ict_ne` are columns 3..10.
- `alpha_<dir>` is `16 + direction * 2`.
- `zfl_<dir>` is `17 + direction * 2`.

### The wire format is kept verbatim

Rather than mirrored into a second structure:

```json
{"rounds": {"1": {"rows": 77, "cols": 35, "cells": {"2": {"3": "100.5"}}}},
 "fields": {"lineedit_pixel_size_top": "0.2478"}}
```

Cells are strings end to end.
`""` and `"0"` mean different things to every formula on the far side (`avgWithoutZero` counts one and not the other), and a JSON number would also reformat what the operator typed.

Storage is sparse, but the Excel grid is dense: a spreadsheet has no notion of a missing cell in the middle of a row.

### Every round exists from the start

A round the operator has not filled in is an **empty** table, not a missing one.
`hasRound()` false means "this tab does not exist" to the formulas, and the eleven tabs do exist.

### One side marker per round

The column records where the after-bezel segment starts, and there is only one start.
First marked row wins; two starts is not a thing the pipeline can read.

### Noise bands are removed one at a time

Every cali op answers with the whole table, and the client replaces its copy with it.
Sending all the removals at once would build each request from the pre-removal table, so the last reply to arrive would overwrite every removal before it.
The result is exactly one band removed and no sign that the others were lost.

The detection and the removal are two steps on purpose: detect first, then remove each band the detector was confident about.
The removal is destructive, so the bands it acts on are the ones reported rather than a fresh guess per direction.

### The mutated table comes back even on failure

An op that got part way has already written derived columns, and the display must match what the server actually holds rather than silently keeping the pre-call state.

### Update Table recomputes afterwards

As the old client's Update Table did.
The op writes the PCT and the eight ICT columns and nothing else, so without the recompute the round shows raw measurements while every derived column (`ict_avg`, `pct_cal`, `distance`, `alpha`, ZFL) still holds the previous round's numbers or is blank.
One press, one finished round.

### The distance searches (CaliJob) are the only cancellable work

Everything else in this window is a plain service that finishes on the rig whatever the client does.
These three run for minutes: `find_distance_for_target_aggregation` is 282 probes, each recomputing all eleven rounds.
That is the reason they are an action rather than a service.

The table comes back even on cancel, at whatever the last probe wrote.
That is deliberate on the server side, and it is why `cancelled` is reported next to `success` rather than instead of it: a cancelled search produced a partial answer, it did not fail.
A partial answer that looks like a final one is the worst outcome here, because the number goes into a calibration.

A total of 0 means the op cannot report one, so the progress bar shows indeterminate rather than pretending to know how far along it is.

Stop cancels a running search first and falls back to dropping replies only when there is no search to cancel.
When it does drop replies it says so: `/compute/cali` and `/compute/xlsx` are services with no cancel, so they finish on the rig regardless.

The goal handle is written on the executor thread when the goal is accepted and read from the GUI thread by `cancelSearch`, so it takes the same mutex as the clients.

### Per-round rebuilds

Typing in a cell must not rebuild all eleven tables: that is roughly 24k `QVariant` constructions per keystroke, in a table the operator types into all day.
`round < 0` rebuilds everything, for the ops that replace the whole table.
A ten-file batch would otherwise rebuild all eleven tables ten times over.

### Excel

The file dialog stays on the client, because picking a path is the operator's job and the operator is sitting at the client.
Everything between the path and the numbers is parsing, and parsing is computing, so the bytes go to the rig and a grid comes back.

`grid_json` is `[["a1","b1"],["a2","b2"]]`: the sheet as text, in the same row and column geometry as the round table.
Anything past 77x35 is a wider sheet than this pipeline reads and is left behind deliberately.

`round_3.xlsx` maps to round 3.
A round number in the file name wins; anything unnumbered falls into the next free slot, so an oddly named folder still loads in a predictable order instead of silently overwriting round 1 ten times.

Writes go through `QSaveFile` so a failed write leaves the previous file intact rather than a truncated one.
These files are the run's only record of a round.

### Pooled versus per-round scores

`aggregationAt` scores every enabled round together at one distance.
That is the number you judge a whole run on, as opposed to how tight a single round is with itself.

`ict_zfl_points` draws one round on its own and ignores its enabled flag on purpose, which is exactly what you need when deciding whether to disable it.

## src/HttpServerProbe.h

HTTP is not a transport in this app.
This holds the committed HTTP server config (host plus three service ports) and the reachability verdict for each.

It is a singleton because the committed URL has more than one reader: the Axis, Monitor and Camera panels all need it, and a QML `id` is only visible inside the file that declares it.

The statuses are a receipt, not a live monitor.
They report what happened the last time `probeAll()` ran, which is why editing a field voids them to Unknown.

One change signal covers all four values.
Re-evaluating four colour bindings is free, and it saves `hostStatus()` from tracking dependencies on the other three.

The enum values double as indices into `port_`, `status_` and `gen_`, so they must not be reordered.

## src/PatternIo.h, src/PatternIo.cpp

`"file://" + path` is right on Linux and wrong on Windows.
A Windows path starts with a drive letter and needs three slashes, so the two-slash form makes `C:` the host and the image silently fails to load.

Both directions are here because QML needs a local path to hand a controller and a URL to hand an `Image`.
A string that already contains `://` is already a URL of some kind (`image://`, `qrc:`, `file:`) and is left alone.

## src/SubAppWindows.h

The 3D Verification screen is still the old QWidget and `uic` dialog, copied unchanged from `v2.0_2026_main-cpp-ros`, so it opens as its own top-level window rather than inside the QML scene.

## qml/windows/Main.qml

### Why the Direction Diff ops live on the window

Two controls now start them: the Direction Diff button, and a histogram panel's Show Curve when it has nothing to draw yet.

One request each covers all eight directions and both polarities.
The reply is cached in `ComputeController.histogram` and the panels filter it locally, so ticking a direction costs nothing.
The old Widgets client asked for only the ticked directions and therefore had to re-fetch whenever one was added.

It is refused rather than defaulted when a centre is missing.
A centre of `(-1,-1)` is what "no centre" looks like here, and running the detection against it returns curves measured from the image corner: plausible-looking numbers that are not a measurement of anything.

### Why refreshCurves is split from runDirectionDiff

Split out 2026-09-09 because the noise-cleaning toggle felt slow next to the Widgets client.
It was.
Every detect op takes `computeMutex` on the compute node, so `histogram_8dir` and `nodes_8dir` **serialise**.
One toggle was two full sweeps of two 3040x3040 frames, one after the other, when only the first changes anything the operator is looking at.

The old client's toggle ran `showCurve(1)` and `showCurve(2)` and nothing else.
It fetched nodes inside Update Table instead, which is where they are actually consumed.

The toggle only re-measures when there is already a measurement to redo.
Before that the toggle is just a setting, and firing two 8-direction sweeps at an empty pair would fail noisily for no reason.

### Connecting the controllers

Every controller that owns a ROS context is connected in the Update handler, and only there.
The app deliberately does not connect on startup, so pressing Update is what creates them.
A controller missing from that list does not fail; it sits idle for ever with no message, which reads as a dead rig rather than an unwired button.
That is how the last three went missing in the 2026-09-08 merge.

`MonitorController` used to be missing from that list on purpose, since nothing called it.
It was deleted on 2026-09-14; all monitor work is `PatternController`'s.

### The error line on this window

This window had no way to say anything until 2026-09-09, which is why the Direction Diff refusal had to borrow the camera panel's error line.

`ComputeController` runs every detect op in the app (`auto_center`, `roi_exact`, `histogram_8dir`, `nodes_8dir`) and until then **nothing listened to it**.
It set `lastError` correctly, emitted `errorRaised` faithfully, and every word went nowhere: `CalibrationController` is heard in the Cali Result window and `PatternController` in Pattern & Monitor, but this one had no window at all.

Found when Direction Diff appeared to do nothing.
It fires `histogram_8dir` **and** `nodes_8dir`; the nodes arrived and the histogram did not, and the reason the server gave was discarded on the way in.
A control that reaches its controller and a controller that reports its failure still add up to silence if no one is connected to the other end.

### Update Table lives here

This is the join between the two halves of the app.
The crossings belong to `ComputeController`, the PCT to the pattern panels, and the table to the Cali Result window.
Only `Main.qml` can see all three.

Direction Diff had always fetched these nodes and thrown them away: `ComputeController.nodes` had no reader at all until 2026-09-09, so the rig was already paying for the 8-direction detection and discarding the answer.

The nodes are re-measured rather than reused from the last Direction Diff, because noise cleaning may have been toggled since.
The toggle deliberately refreshes only the curves, so cached nodes can be from the other setting, and a table filled from those is a measurement of a configuration nobody chose.
The Widgets client did the same: its Update Table called `nodes8dir` itself rather than trusting whatever the curve panels last fetched.

It refuses on a missing PCT.
A pattern window that has never been opened, or whose layers are all zero, would hand over 75 zeros and the table would fill with a plausible, wrong PCT column.
The old client degraded exactly that way, silently.
`pct_cal` is a running sum, so a wrong PCT is not a wrong cell, it is a wrong curve.

The round and the PCT are captured when the button is pressed rather than read when the reply arrives, so they are the ones the operator saw.

The pending-fill token is consumed once and null the rest of the time, so an ordinary Direction Diff does not fill a table nobody asked to fill.
A failed measurement clears it, because otherwise the next Direction Diff would write a table the operator asked for minutes ago against whatever is on the glass now.

### A shot centres itself

Restored 2026-09-09 from the Widgets client, which has no Find buttons at all:

```cpp
if (mode == "Positive" || mode == "Negative") {
    if (radiobutton_auto->isChecked()) ctr = patternCenter(currentSlot_);
    showCurve(1); showCurve(2);
}
```

Everything an operator did there was shoot the pair and read the curves.
This app had turned that into three separate presses (Find Pos, Find Neg, Direction Diff), none of which existed before, and the first two of which an operator had no reason to expect.

It applies only in Auto.
Manual means the operator is placing the centre by hand and a fit that overwrote it a second later would be worse than useless.
Locked means leave it alone.

Only a centre a **capture** asked for triggers a curve refresh.
A manual click in the image sets a centre too, and re-measuring both 8-direction sweeps on every click would make placing a centre by hand unusable.
`centerChanged` was declared on the Centering panel and handled nowhere until the 2026-09-08 audit listed it as a signal emitted into an empty room.

The refresh waits for both halves and for nothing still being centred.
`histogram_8dir` measures the pair against the two centres, so running it half-centred measures the second image from wherever the last pair left it.

### Auto measures the centre on the server

Changed 2026-09-11, reversing the 2026-09-09 decision recorded here before it.

From 2026-09-09 an Auto capture filled CPX and CPY with `Math.round(size.width / 2), Math.round(size.height / 2)`, labelled "frame centre".
The argument was that a frame centre cannot refuse, so the operator is never left with no centre and no next move.
That argument traded the only property this number has to have.
CPX and CPY are what the rig drives five axes from, and half the image width is not a measurement of anything.
A wrong number that always answers is worse here than no number, which is the same reason a centre fit returns `(-1,-1)` rather than its best guess.

The Widgets client measured, and this branch's fill was a migration stopgap rather than a design.
Every Positive or Negative shot there called the server's `pattern_center` op and wrote the answer into the fields, and the crosshair was drawn at that answer, so the marker moved with the measurement.
Grepping `cpp/src/controllers/controller_main.cpp` on `v2.0_2026_main-cpp-ros` for `cols/2`, `rows/2`, `width()/2` and `height()/2` returns nothing: the old app never used the frame middle anywhere.

`onCaptured` now calls `centering.findCenter()`, which reaches `auto_center` through `ComputeController`.
The op is not the one v2.0 used: v2.0 called `pattern_center` and fell back to `roi_exact` by hand, while `auto_center` is that cascade with its gates built in, specified in `doc/auto_center_design.md`.

A refusal is now a normal outcome on this path, and all three consequences are handled.
The centre is cleared to `-1` rather than left stale, the reason is shown in the Centering panel, and the pending checklist below is released.

The size guard that read `imageSizes` went with the fill.
Nothing client-side needs the picture's dimensions any more, because the server measures from the slot's own bytes.

### The pending checklist has three release paths

`autoCentrePending` marks a slot as awaiting a centre, so that only a centre a **capture** asked for triggers a curve refresh.
`onCenterChanged` crosses a slot off, and runs the Direction Diff once the checklist is empty and both halves have a centre.

A slot left on the checklist is never crossed off by anything later, so the Direction Diff stops running for the rest of the session and nothing is logged.
Every path that ends a request therefore has to release it:

- `centerChanged`, the answer arrived;
- `centerRefused`, the server declined the fit, which `applyResult` also emits when the op itself returns `!ok`;
- `statusChanged` away from `Ok`, the link dropped.

The third is the one that is easy to miss.
`applyLink` and `connectTo` both call `liveTokens_.clear()`, so a reply that arrives after either of them returns early at the top of `applyResult` and emits no signal at all.
Without the status handler a rig that disconnects mid-capture strands the flag permanently, and only restarting the app clears it.

### The offline path

A saved capture read into a slot, so the detect ops can run against it with no camera.
The bytes go to `ImageStore` and are uploaded with each `/compute/detect` call exactly as a fresh capture would be.
An imported file has no slot on the rig to name, so uploading is not a fallback here, it is the only correct transport.

The view follows the slot that was just filled, so the operator sees what they loaded rather than whatever the view happened to be on.

## qml/windows/PatternAndMonitor.qml

### The PCT column

In the exact order `update_table_from_capture` expects: 25 concentric radii, **then** 50 stripeline intervals.
Not one or the other.
Both go in every time, whichever pattern is currently on screen.

`CaliCompute` splits them at the `*` row.
Rows above it are the TOP panel and read entries 0..24; rows from the `*` down are the SIDE panel and read `25 + (layer - sideStartLayer)`.
Sending only the concentric radii would pair every side row's ICT with a ring radius, and `pct_cal` is a running sum, so the error compounds down the column instead of staying local.
It would also be invisible: a pattern whose 50 side intervals are all equal produces a table that looks perfectly reasonable.

Strings, not numbers, because `""` and `"0"` are different answers to `updateTableFromCapture` and a blank must stay blank.

### Setup Monitor Direction

The dialog only collects the five display numbers; sending them is the window's job, because the dialog has no controller of its own and should not grow one.

It is wired to `PatternController`, not `MonitorController`.
Both can do this, and that duplication is the open question this branch keeps colliding on, but this window drives every other monitor operation through `PatternController`.
A dialog that reached a different controller than the panel beside it would connect to the rig twice and answer to neither.

Re-wired 2026-09-08.
This window came from `v2.1_2026_New-UI-CPP-ROS`, which opens the dialog but handles neither signal, so both buttons emitted into nothing.
Build and `qmllint` were clean: an unhandled signal is not an error.

## qml/windows/MoilCalibrationResult.qml

### State belongs to the controller

The window is the form.
The table, the load state and the option flags live where the ops that use them live.
Every value pushed by a rig profile goes through `CalibrationController.setField`, so it travels with the table on the next op rather than living in a QML property the server never sees.

The Calibration System combo applies a rig's pixel sizes and screen gaps on `activated` only, which fires on an operator choice and not on the model loading or a programmatic index change.
Opening the window therefore never silently overwrites fields that came out of a loaded Excel file.

### Update Table was pointing at the wrong op

Between the 2026-09-08 merge and 2026-09-09 this button ran `compute_all`, which is the old client's "Update All Cali Result": a different operation that recomputes from data already in the table.
The Parameter tab already carries that one under its correct name, so this was a duplicate wearing the missing function's name.
Pressing it on an empty round recomputed nothing and looked dead.

The window does not do the work.
It has no access to the pattern panels, and the nodes belong to `ComputeController`.
`Main.qml` owns both, so the request goes there through `updateTableRequested`, which had been declared and never used.

### Min Aggregation by Interval is one minimum per round

Restored 2026-09-09, after the merge with `origin/v2.1_2026_New-UI-CPP-ROS` dropped it.
That merge rebuilt this tab as the 20-range workspace and pointed the button at `findMinInWindow`, which is a different op with a different meaning, and the substitution is invisible in the UI:

| QML call | Server op | Meaning |
| --- | --- | --- |
| `findMinInWindow` | `find_min_aggregation_in_window` | one minimum, over every enabled round pooled |
| `findMinForRound` | `find_min_aggr_single_round` | each round's own minimum, one search per round |

The pooled number cannot answer the question this button is for.
A single bad round, shot at the wrong distance or against a stale pattern, barely moves the pooled minimum.
It shows up only as its own minimum landing somewhere the others did not.
That is the whole reason the old client looped the rounds instead of asking once, and it is why `docs/MISSING_CONTROLS.md` lists this exact swap as a semantic trap.

The distance range is 1..500, matching the old client's `find_min_aggr_single_round(i, 1, 500)`.
Deliberately not the panel's interval fields: those are an ICT window in **pixels**, converted from IH percent, and feeding an ICT window in as a distance range would be the same class of unit error, accepted without complaint and wrong by a factor nobody can see in the output.

A round with no ICT anywhere is skipped.
The rig would otherwise spend a full ternary descent to report a minimum of nothing.
The old client skipped these too.

`searchChanged` also fires for progress, so "finished" cannot be read from one edge of `searchRunning`.
A round is treated as done only once it has been seen actually running and then stopping.

A refused or failed op ends the sequence rather than marching the rig through nine more rounds that will fail the same way.
Partial is reported as partial: a cancelled search produced a real answer for the rounds it got through, and saying otherwise would throw away work the operator paid rig time for.

### Stop drops the queue as well as the running probe

Calling `cancelSearch()` alone would land the rig back on the next round a moment later, which reads as a Stop button that does not stop.
It always unwinds the rig, whether or not a sequence is running, because this is the tab's only Stop and the other two searches go through it as well.

### Redraw versus search

Update All recomputes the pipeline and then re-reads the series.
The two plot buttons only re-read, because the numbers behind them have not changed unless the table did.
The Overlap tab's "Update Dist vs. Aggr" is a redraw of the samples the Aggregation tab's searches produced: `updatePlotDistVsAggr()` in the old controller never started a search, and a button saying "update" must not spend minutes on the rig.

### File dialogs

They stay on the client: picking a path is the operator's, and the operator is sitting at the client.
Everything between the path and the numbers is parsing, and that happens on the rig.
Browse just records the folder; Load All and Load Database go on to read it.
One dialog, because "which folder" is the same question.

## qml/panels/CaliResultDataPanel.qml

The layer count is 75, not 74, for the reason in the `CalibrationController` section.

The table lives in the controller.
This panel displays it and sends edits back; it does not hold a second copy, because the copy that travels to the rig must be the copy on screen.
Edits bump the controller's table version so every cached series knows it is stale.

The centre read-outs are a read-out of a measurement, not an input.
Typing over them would be claiming a centre nothing measured.

### singleDistance is readonly on purpose

A writable property here would be assigned by the toggle, and that assignment **destroys** the binding to the controller.
After the first click the switch would stop following anything that changed the flag elsewhere, while still looking correct.
The toggle writes the controller and the value comes back through the binding.

The toggle was moved here from the window on the New-UI branch, so it sits next to the Distance field it qualifies rather than in a toolbar three panels away.
The window keeps an alias so `root.singleDistance` still reads.

### round_enabled

Ships with every request.
The server uses it for the aggregations, the regression fit and the pooled Overlap, so without it the request silently means "all of them", including a round you know is bad.

### Auto Detect Noise Bands is not Clean Noise

Renamed 2026-09-09.

In the Widgets client "Clean Noise" is a **toggle** of the global `noise_cleaning` flag.
It changes how the rig extracts nodes, and its effect shows up in the histogram the moment you flip it.
That control exists here too, on the Centering panel, spelled "Noise cleaning".

This button is a different operation that shares none of that.
`auto_detect_noise_bands` finds bands in the round's ICT columns and **removes** them from the table.
It is destructive, it touches no histogram, and the Widgets client has no equivalent at all: it is a newer server capability.

Two unrelated things under one name, and an operator who knew the old client pressed this expecting the curves to change.
Same trap as Update Table, found the same afternoon.

### hasRawIct

The engine's own "is there anything worth computing yet".
Running the pipeline over an empty table produced a table of blanks and no explanation.

## qml/panels/CaliResultAggregationPanel.qml

This is the range workspace: Global plus Range_1..20 as columns, the pattern gaps, the sensor and round fields, and the per-IH-range result table.
Its five buttons are the old client's, name for name:

| Old client | Here |
| --- | --- |
| `btn_range_window` | Range Window |
| `btn_min_aggregation_by_interval` | Min Aggregation by Interval |
| `btn_aggr_by_range_and_distance` | Aggr by Range and Distance |
| `btn_keep_round_data` | Keep Round Data |
| `btn_save_history_distance` | Save Distance History |

### The searches live here, not in Overlap

In the old controller `minAggregationByInterval()` and `aggrByRangeAndDistance()` are what fill `distAggrSamples_`; Overlap's "Update Dist vs. Aggr" only redraws them.
That is why this panel carries the Stop button and the progress strip: these are the only cancellable jobs in the window, and the target search is 282 probes each recomputing all eleven rounds.
Minutes, not seconds.

The old client ran these behind a modal progress dialog with a Cancel button.
This is the same affordance without the modality, which is what made the old one feel hung.
The bar is indeterminate when the op cannot report a total, rather than showing a 0% bar that looks stuck.

### IH fields are percentages, the ops take pixels

The old client converts with `xLo = pct / 100 * maxIctAllRounds()` before every call.
Passing the percentages straight through is the silent failure to watch for here: 0..100 is a valid pixel window, so the search runs, converges, and returns a plausible wrong distance with no error anywhere.
`windowXLo` and `windowXHi` do the conversion once so no caller can forget it.

`maxIct` is `maxIctAllRounds()` in the old client.
Zero means no IH data has been loaded, which is exactly when a percent-to-pixel conversion must not run.

Anything outside 0..100, or with min not below max, is rejected before converting, as the old client did.
An inverted window silently scores nothing.

### Two ops behind one button

A Distance in the box means "score that distance"; an empty Distance with an Aggregation means "search for the distance that hits it".
That is the branch the old `aggrByRangeAndDistance()` takes on an empty line-edit, and the window reads `hasRequestedDistance` to pick.

### Range Window

Ports `rangeWindow()`: step a window of `windowInterval` across `[ihMinInterval, ihMaxInterval]` by `stepInterval` and write the resulting IH Min/Max into Range_1..Range_20.
Range 0 is Global and is deliberately not touched.

The old client read these four numbers out of a JSON file chosen from a dialog (`range_min`, `range_max`, `step`, `window`); the New-UI layout puts them on the panel instead, which is the same arithmetic without the file.

Whole numbers stay whole, as the old client's `fmt()` did, so the filled cells read like the ones an operator types by hand.

## qml/panels/CaliResultOverlapPanel.qml, qml/panels/CaliResultParameterPanel.qml

Overlap is the old client's `tab_overlap`, ported.
Two plots side by side, exactly as `cali_result.ui` had them: `label_overlap` driven by `btn_update_overlap` on the left, `label_dist_vs_aggr` driven by `btn_update_dist_vs_aggr` on the right.

Both panels are pure views: every series arrives as a property and every button leaves as a signal, and `MoilCalibrationResult.qml` binds them to `CalibrationController`.
That is how the New-UI branch wrote it, and it keeps the plots testable without a rig.

Per-round colours come from the pipeline's own `curve_color` table, so a round is the same colour here as in the Data tab's rows and in every other tool that reads these files.
Empty falls back to the palette.

The single-round overlay is fetched with `ict_zfl_points`, which ignores the round's enabled flag on purpose: you need to see a round you have switched off in order to decide whether switching it off was right.
It is drawn last and in the marker colour so it reads on top rather than becoming one more indistinguishable scatter.

On the Parameter tab, the ICT axis is sized from `max_ict_all_rounds`, a question only the pipeline can answer.
The coefficient boxes come straight from `alpha_polynomial` and are re-assigned explicitly rather than left to the binding: an edit above breaks the binding, and without the explicit assignment a freshly fitted polynomial would be silently ignored in favour of a hand-typed one.
Table fields read back as text and are blank rather than `undefined` when never set, so an empty box means "not set" and not "broken".

The FOV is a property of the rig's camera, so it is stored there and shared with the Camera panel's spin box rather than kept twice.
The image centre is what the detect cascade found, not a number to type; blank means no centre was established.

The rig-profile fields are shown because the operator is told to confirm them before computing, and a value you cannot see is one you cannot confirm.
They are editable because a rig can differ from its profile.

The bands are scaled to the measured ICT range rather than fixed pixel spans, which only lined up with the stand-in data they were drawn for.

`PlotBlock` was extracted from this file to `qml/controls/PlotBlock.qml` on the New-UI branch because Overlap and Graphs need the same block.
The extracted one is a superset (it adds `emptyText` and `readoutFontSize`), so every use is unchanged.

## qml/panels/ConcentricPanel.qml, qml/panels/StripelinePanel.qml

### layerRevision

`specJson()` reads the layers through `layerModel.get()`, and a `ListModel` emits no signal a QML binding can depend on.
A fingerprint built from `specJson()` alone re-evaluates for the scalar fields (resolution, colours, crossline) and **never** for a layer's radius, shape or centre, which is the edit an operator actually makes.
The counter is what makes those visible.

Every mutation of `layerModel` goes through `setLayer()` or bumps `layerRevision` itself.
A `setProperty` that skips it is a change Auto Update will never see.

Bulk operations bump it once for the whole change rather than per row: a repaint is one change, and bumping per layer would fire 25 auto-renders.

### positive / negative / custom

The mode records whether the layer table still holds a generated alternation of the two colours above, or was edited row by row.
Only a generated table is recoloured automatically.

Recolouring a hand-edited table would silently throw away per-layer colours the operator set deliberately, and nothing would say so.
A hand-picked colour therefore switches the table to `custom`, and the colour pickers stop repainting over it.

A file that carried `pos_neg_color` is a generated alternation and stays repaintable; one with per-layer colours is custom.

### The import guard

The mode is held at `custom` while the envelope's colours land, so the `positiveColor`/`negativeColor` changes it makes do not fire `refreshPattern()` and repaint rows the loop is about to rewrite.
Without it an import repaints the whole table once for nothing and bumps `layerRevision` an extra time, firing a spurious auto-render.

`ConcentricPanel` has carried this guard since the `335ee75` port.
`StripelinePanel` did not, and the 2026-09-08 merge did not restore it because `StripelinePanel.qml` auto-merged without a conflict.
Neither the build, `qmllint`, nor an API diff can see a missing guard inside a function body.
It was found by comparing the two panels against each other.

### Colours on the wire

The two colours go to `PreparePatterns` as `[r, g, b]`.
The server applies the odd/even inversion itself, so these are just the pair, not a polarity.

### Column widths

Passed in from the panel, so the header row and every layer-row delegate bind to the same values and can never drift apart.

### Stripeline geometry

Each row's Height is a **step**: the pixel thickness of that stripe from the previous one, not an absolute position.
It is named `interval` internally to match the reference app, because `Item` already has a builtin `height` property.

## qml/panels/ChessboardPanel.qml

A checkerboard tiled outward from the screen centre.
An extra pattern, separate from the 75 PCT values, so it has no layer table and no Import/Export: there is no PCT JSON for it.

With no layer table every input is a bindable property, so `specJson()` alone is a sufficient fingerprint and no revision counter is needed.

`autoUpdate` is declared here because the Auto Update switch binds to it.
Without the declaration the binding assigned `[undefined]` to a bool and the engine logged "Unable to assign [undefined] to bool" on every startup.
The sibling Concentric and Stripeline panels both declare it and this one did not.

## qml/panels/CenteringPanel.qml

### findCenter is what an Auto capture calls

Find Pos and Find Neg were removed on 2026-09-09, and the Widgets client never had them.
That left `findCenter()` with no caller for two days, and the `auto_center` cascade unreachable from this UI, because Auto filled the frame's middle instead.

Since 2026-09-11 `Main.qml`'s `onCaptured` calls it on every Auto capture.
There is still no button, and there should not be one: an operator shoots the pair and reads the curves, and a separate press to measure the centre is a step the old app never asked for.
`onCenterFound` and `onCenterRefused` are the other half of this and are live with it.

Manual is the other caller and does not come through here.
A click sets the centre by hand and seeds `roi_exact` through `setCenter`.

### A refused fit clears the centre

It is not left showing the previous run's coordinates.
The rig drives five axes off this number, and a stale one looks exactly like a fresh one to whoever reads it next.

The refusal reason is usually longer than the strip it is elided into, and it is the one thing worth reading in full.

### The dot answers one question, not two

It used to fall through to the link state whenever there was no confidence to report, so a connected rig with no fit yet drew the same green dot as a trusted `auto_center` result, and a hand-picked centre drew it too.
Green then meant "the compute node answers" in one moment and "this centre can be driven on" in the next.

`detectState` now reports the centre and only the centre.
`ProbeStatus.Ok` is reserved for `confidence == "good"`, which is the one case `doc/auto_center_design.md` allows a move on; `marginal` is amber, a refusal is red, and everything else -- no fit yet, picked by hand, compute node absent -- is grey.
Grey is deliberate for the hand-picked case: the operator supplied that point and nothing measured it, so there is no verdict to show.

A missing compute node is grey rather than red for the same reason `CLAUDE.md` gives: an idle rig is a missing rig, not a fault, and it outranks a stale refusal from the session before it.

The label is coloured only in the two states that need acting on, and carries the long form in its tooltip.
The strip is about 40 characters wide and the refusal sentences run past 60, so the short text names the state and the tooltip quotes the stage reason verbatim.

### The click means different things per mode

In Manual the click is a **seed**, not the answer: `roi_exact` recurses `detect_roi` from it until the point stops moving.
In Locked nothing is sent at all.
In Auto the click is an override the operator made deliberately, so it is left exactly where they put it.

The recursed answer is assigned directly rather than through `setCenter`, which would send it straight back out as a new seed and loop.

### method

Optional, and names where the centre came from.
The read-out beside it is the only thing telling the operator whether they are looking at a measurement or a default.
Omitted, it stays "picked by hand", which is what every existing caller means.

### Ring count 0

Lets `auto_center` take the count from the prepared PNG, which is measured off the picture that was actually on the glass and beats the modal count inferred from the capture itself.
See `ComputeDetectOps.cpp`.

## qml/panels/HistogramPanel.qml

### The curve is what the rig measured, or nothing

This used to be a closed-form formula that produced a plausible-looking greyscale trace for any direction, which meant the panel drew a full set of curves whether or not a shot had ever been taken.
Reading a node position off one of those was reading a sine wave.
An empty list is the honest answer until Direction Diff has run.

### The crossings come from the server

`get_list_intersecting_nodes` is a calibration measurement: it is what the ICT columns are built from.
A second implementation of it in QML would be a second answer to the same question.
The server returns the raw crossings of exactly this pair of curves, with no diagonal scaling and no cross-direction reconciliation, so they line up with what is drawn once this panel applies its own scale.

The `sqrt(2)` on the diagonals is this panel's own x-axis scaling.
A diagonal ray crosses `sqrt(2)` pixels per step, and the server deliberately does **not** apply it (see `histogram_8dir` in `ComputeDetectOps.cpp`) so that the plotted crossings land where the two drawn curves actually meet.

### Show Curve collects once, then toggles

The old Widgets client's Show Curve **fetched**: `showCurve(n)` called `histogram_8dir` on demand and cached the answer.
Here it was a pure view toggle, so an operator who pressed it on an empty panel got nothing at all and no explanation, which is exactly how it was reported on 2026-09-09.

It does not re-fetch once there is data.
One request already carries all eight directions for both polarities, and the checkboxes filter that locally.
Pressing it again would put an identical request on the wire and re-measure two 3040x3040 frames for the same answer.
Direction Diff is the deliberate re-measure.

### The two derived properties

`sampleCurve` is the expensive part, keyed by `"side:direction"`, and only resamples when the selection or channel changes.
`curveShapes` is selection plus resolved colour, cheap to rebuild on every `colorOverrides` edit, and never touches `sampleCurve`.
That split is why a colour edit does not resample.

### The legend lives in the Pop Up window, not in the panel

The direction box used to carry a `Flow` of legend chips under the two rosettes, shown whenever a direction was selected.
It overflowed the panel.
At `unit = 16` the panel needs about 190 px with no direction selected against a `Theme.minHistogramHeight` of 208, so there were 18 px of slack; the chip row cost 30 px (8 px of `SectionFrame` spacing, another 8 px of its own `Layout.topMargin`, and 14 px of chips) and put the content 12 px past the border.
Nothing reported it, because the panel is a plain `Rectangle` and a `Rectangle` does not clip: the direction box simply drew over the panel below it.
The two histogram panels had been absorbing the difference out of leftover height, and the `HelpButton` added to `Main.qml`'s header row took `unit * 1.25` out of `rootColumn.free`, which pushed them both down onto their minimum and exposed it.

The chips were also redundant here.
A rosette cell that is switched on is filled with that curve's own colour (`DirectionRosette`'s `tint`, fed by `posColors` / `negColors`, which resolve `colorOverrides`), so a red "Pos N" chip under a red N cell repeated what the cell already showed.

The Pop Up window keeps its legend.
It has the width for it, its header row was already laid out that way, and its rosettes are not on screen.

## qml/controls/HistogramPlotView.qml

### axisTitles, and why the margins move with it

The two axis titles are drawn outside `area`, in margin space reserved for them: `plotMarginLeft` carried a `+ unit` for the rotated y title and `plotMarginBottom` was `unit * 2.8` where the tick numbers alone need about `unit * 1.6`.
Hiding the labels without touching the margins wins nothing -- the canvas keeps the same size and the reserved strip is simply blank.

So `Theme` now names the two parts separately (`plotTickLabelWidth`, `plotTickLabelHeight`, `plotAxisTitleSpace`) and the view picks its own `marginLeft` / `marginBottom` from `axisTitles`.
At `unit = 16` that is 16 px of width and 16 px of height handed back to the plot, on a canvas about 150 px tall in the docked panel.

It is a property rather than a deletion because the two users disagree.
`HistogramPanel` puts a `RangeControl` labelled "Gray Scale" and another labelled "IH" directly beside the plot, so the in-plot titles repeat what is already on screen.
`PlotBlock` -- the Cali Result graphs -- has a title above the plot and unlabelled readouts below it, so in `CaliResultGraphsPanel` the in-plot titles are the **only** place the axes are named.
Deleting them there would leave "Shift of Entrance Pupil" with two anonymous axes.

## qml/controls/DirectionDiffDialog.qml

ICT Direction Difference: opposite directions, compared node by node.

This is the Widgets client's `btn_direction_diff`, which is a "?" beside the shot buttons.
An **info popup**, not a measurement control.
Restored 2026-09-09, when it turned out this app had put the label on something else entirely: pressing Direction Diff here re-ran the curves, which is what a Pos/Neg shot does by itself in the old client.

What it is for: a well-aimed capture is symmetric about the centre, so the crossing at ring k going north should sit at the same radius as the one going south.
It does not, quite, and the size of that disagreement is the most direct read on whether the rig is aimed properly, far more direct than looking at the curves.
N-S large means the centre is off vertically; W-E large means horizontally; the diagonals catch a tilt the straights miss.

Red at `|difference| >= 5 px`, which is the old client's threshold, kept exactly.
5 px is where an aiming error stops being noise.

The nodes are the same ones Update Table writes: same op, same slots, same parameters.
That is the whole value of it, because it shows what Update Table is about to record.
A second way of getting these numbers would make it show something else, and the difference would look like a measurement.

The dialog re-measures before opening, exactly as the old one did, so the popup shows what Update Table would write rather than whatever the last shot left behind.
Noise cleaning may have been toggled since, and that now refreshes only the curves.

Directions are read with find-style access, never `nodes[d]` with a default.
A direction the server did not return is **absent**, and inventing an empty list for it turns "this direction was not detected" into "this direction has no nodes", which reads as a measurement rather than as missing data.
The header row prints both counts so the two stay distinguishable.

## qml/panels/LiveCameraPanel.qml

`streaming` is owned by the controller.
The stream is a ROS subscription, and a local bool that says "streaming" while nothing is subscribed is exactly the lie this panel used to tell.
The button asks; the controller decides.
A Go Live that cannot connect leaves the button reading Go Live rather than pretending to stream.

`receiving` is told by the controller whether frames are actually arriving.
`preview.loaded` only says the last URL decoded, which stays true after the stream stops.

The frames come from a ROS topic, not the HTTP endpoint this panel used to name.
HTTP is a reachability probe in this app and carries no image data at all, so the status dot falls back to the ROS camera link.

URLs already carrying a scheme are passed through untouched.
The live frames arrive as `image://moilcamera/live/<revision>` from the image provider, and prefixing those with `file://` produced a silent blank panel.

The ROI button governs both markers.
They are the same overlay to anyone aiming the rig, and one toggle that leaves half of it on would read as a bug.

## qml/panels/CameraPanel.qml

Open Img loads a picture from disk into the slot the view is currently showing (Single, Positive or Negative).
It deliberately follows the view rather than asking: the operator can see which slot they are about to overwrite, which a dialog with a dropdown cannot promise.

It is not gated on `root.busy` or on the rig.
This is the offline path: with a compute node running anywhere, including this machine, which needs no hardware for it, Open Img plus a centre plus Direction Diff is a complete measurement without a camera.

## qml/controls/AutoRefresh.qml

Debounced "the thing changed, render it again".

A change is one keystroke: typing "120" into a radius field is three changes, and rendering a 1920x1920 pattern on the rig for each of them would queue three renders to throw two away.
The timer restarts on every change, so the render happens once the operator stops typing.

`fingerprint` is any string that differs when the subject differs.
In the pattern panels it is the spec JSON plus a layer revision counter.

Enabling fires immediately rather than waiting for the next unrelated edit, so the switch has a visible effect.

## qml/controls/ImagePreview.qml

The fisheye edge circle is a second ring on the same centre, drawn at a radius the operator sets by hand.
It is separate from the ROI because it means something different: the ROI is the search window a centre fit works in, the edge is where the image circle ends.

It draws just the circle, with no crosshair and no bounding square.
Those belong to the ROI marker and would clutter the one thing this ring is for, which is seeing whether the radius matches where the image circle actually ends.

### The ROI marker is a readout, not a reticle

The X is drawn where the server measured the pattern centre, and nowhere else.
No capture, or a refused fit, means no X.

A fallback to the middle of the frame was added on 2026-09-10 and removed on 2026-09-11.
It existed so that something was on screen to aim at while jogging, before any capture had been taken.
What it did instead was make a refusal indistinguishable from a success.
`onCenterRefused` sets CPX and CPY to `-1`, the fallback then placed the X in the middle of the frame, and nothing on screen separated that mark from a measurement.
The rig drives five axes off this number, and the rule for the whole centring path is that a fit which cannot be trusted reads as "no centre", never as a plausible-looking wrong one.

An aiming point for jogging is still a real need and is currently unmet.
If it returns it has to be drawn differently from the measurement, in another colour or shape, so that the two can never be read as the same mark.

Either switch hides the marker.
`roiRadius: 0` is what `LiveCameraPanel` passes through `showRoi` and what `CameraPanel` passes in fold view, and `centerX: -1` hides it as well.

### The measurement is its own gate

There is no pattern-type check in front of the marker, and adding one back would be a mistake.

A `reticleRadius` property existed for a day, on 2026-09-10.
It read `patternAndMonitor.shownPatternType` and forced `roiRadius` to 0 unless the last pattern shown was concentric, because the marker fell back to the middle of the frame back then and would otherwise have drawn an X over a stripeline or chessboard capture.

It was removed on 2026-09-11 for two reasons.

The first is that its premise went away with the fallback.
The X is now drawn only where a fit landed, and a fit only lands on concentric rings.
A stripeline capture has parallel gradients everywhere, so the least-squares intersection in `patternCenterFit` is degenerate and the op answers `(-1,-1)`, which draws nothing without anyone checking a pattern type.

The second is that the gate did not actually work.
`shownPatternType` was written in exactly one place, `PatternAndMonitor.showOnMonitor`, and a pair shot never goes through it: `Main.qml`'s capture handler calls `PatternController.showPrepared` directly, and the rig keeps its rendered pattern PNGs between sessions.
So a fresh app could take a perfectly good pair, fill CPX and CPY, and draw no crosshair at all, with nothing logged.
That is the failure it was reported as, and it is the ordinary shape of a state variable that has one writer and a path that bypasses it.
Nothing in `PatternController` or `MonitorController` records the type, so without this the app has no idea what is on the screens.

It tracks what is displayed now, not what a capture contained.
Switching the monitor to stripeline therefore removes the X from a concentric capture that is still on screen.
Recording the type per slot at capture time would fix that, and would need a place to put it next to the capture in `CameraController`.

Closing a pattern does not clear it either.
`DpadMonitorViewer` calls `PatternController.closeMonitor` directly, per direction, and the reticle is not per direction.

### Wheel zoom scales one wrapper, not the image

Added 2026-09-11.
The `Image`, the picker `MouseArea`, the grid, the edge circle and the ROI marker all sit inside one `Item` called `stage`, and the wheel changes only that item's `scale` and `x`/`y`.

Nothing else in the file had to learn about zoom.
`padLeft`, `padTop`, `zoom` and `coordScale` are all still derived from `image.paintedWidth`, which does not change when a parent is scaled, so every marker keeps landing on the same image pixel and the click-to-pick maths is untouched.
Qt inverts the parent transform before delivering mouse positions to a `MouseArea`, so `toFrameX` still receives coordinates in unscaled preview space.

The scale pivots on the cursor rather than the centre.
`zoomAt` recomputes `panX` and `panY` so the pixel under the pointer stays where it is, which is what makes a wheel the only control needed: there is no drag-to-pan, you point at what you want and scroll.
Scrolling back down to `viewScale === 1` resets the pan to zero, so fit-to-panel is always exactly reachable.

`smooth` and `mipmap` are switched off above fit.
Both are for downscaling; left on while magnified they interpolate the very pixel values a centre fit is measured from, which is the opposite of what a closer look is for.

`zoom` still reports the fit scale alone, so `CameraPanel`'s Zoom readout multiplies it by `viewScale`.
The loupe deliberately does not: it is a fixed 18x magnifier over the source image and is anchored outside `stage`, so it never scales with the view.

## qml/panels/MonitorSlotPanel.qml

One projector/camera slot in the Monitor Viewer window: preview, pattern file, and brightness for a single direction (TOP, N, W, S, E).

Browse loads a pattern JSON into the panel that owns that pattern type.
Update pushes that panel's spec plus brightness to this screen.
Turn off asks the rig to close the pattern on it, so the panel returns to its desktop.

`PatternIo` on this branch spells the conversion `toLocalPath`, not `localPath`, and also carries `toFileUrl`.
Same function, one name.

### Brightness starts from the rig, not from 5

`appliedBrightness` is what the rig last confirmed; "Press Update" shows while the spin box differs from it.
Both used to start at `5`, so a TOP panel really at 60% showed 5 and no pending change.
When `PatternController.status` becomes `Ok`, a slot with `brightnessSupported` calls `readMonitorBrightness(direction)`.
The reply, `brightnessRead`, sets both values, so the panel opens clean at the real brightness.
The read reports nothing if the call cannot be sent: it runs on its own, not from a button.

## qml/panels/CaliSystems.js

The rig profiles behind the Calibration System combo.

Selecting a system applies that hardware's pixel sizes and screen gaps to the table fields the pipeline reads: `lineedit_pixel_size_top` / `_side`, and `lineedit_h_gap_<dir>` / `lineedit_v_gap_<dir>`.
Nothing else about the system is a computation input; the name itself reaches no service.

### The source of truth is on the rig

`Server/v2.1.0/config/cali_system/*.json`.
The values are mirrored here rather than read from there because the server runs on the rig's machine and this client does not share its filesystem: a client cannot open a path that exists on another host.

They are four numbers and eight gaps per system, and they describe physical hardware, so they change when someone builds a new rig, not on a release cadence.
If those files are edited, edit these too.

### Gap order

The gap arrays are in the order the v2.0 form laid its fields out: N, S, W, E.
That is why Yuanman reads 250, 250, 240, 240: opposite screens share a gap, which is what you would expect of a rig and what makes the order checkable.

### The zeros are real

For one system `pixel_side` is 0 and every gap is 0 in the source file.
That is the profile as shipped, not a placeholder: this system has no side screens to measure, so the side pixel size and the gaps are never read.
