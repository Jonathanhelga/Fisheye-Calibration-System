---
id: camera-calibration
slug: /calibration/camera-calibration
title: Camera Calibration
---

# Camera Calibration

The run itself. Confirm the rig, home the axes, show the pattern, take the
positive and negative shots, check the centre, read the histograms — then repeat
for each round.

Everything here happens on the [Main Window](../system-overview/main-window.md);
this page is the procedure, that page is the reference. The `captured_image`
HELP key points here.

<Shot
  id="fig-1"
  number="1"
  file="camera-calibration-ready.png"
  caption="The main window ready to shoot: axes homed, pattern up, one positive shot taken."
  what="Capture the whole main window at the point where step 4 is complete — Pattern Mode reads Positive, CPX/CPY are filled, both histograms have curves."
  height={560}
/>

---

## Where the captured images go

Every capture lands in `image_cali/`, relative to where the application runs
from.

| File | Written by |
|---|---|
| `capture_single_image.png` | **Capture** |
| `capture_positive_shot.png` | **Pos Shot** |
| `capture_negative_shot.png` | **Neg Shot** |

| Deployment | Full path |
|---|---|
| Ubuntu build from source | `image_cali/` under the folder you launched from |
| **Windows installer** | `\\wsl.localhost\MoilCali\root\MoilCali\image_cali\` — reachable from Explorer, and linked in the Start Menu group |
| Docker | inside the container unless you mounted a volume |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ EACH SHOT OVERWRITES THE LAST</div>
  <div>
    There are only three filenames. The positive shot from round 1 is gone the moment you take round 2's. Anything you want to keep must be <strong>saved into a round folder</strong> before you move on — see <a href="#7-repeat-for-each-round">§7</a>. <strong>Reset</strong> deletes all three outright.
  </div>
</div>

---

## 1. Confirm the rig's nodes are up

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THERE ARE NO SERVER URLS TO FILL IN ANY MORE</div>
  <div>
    Version 1.1 started with three addresses typed into the main window. Version 2.0 finds the rig by ROS 2 discovery on domain 42, so there is nothing to configure — and nothing to look at that tells you it worked. Check explicitly before you start.
  </div>
</div>

```bash
source /opt/ros/jazzy/setup.bash
ROS_DOMAIN_ID=42 ros2 node list
```

All three of `/moil_camera`, `/moil_axis` and `/moil_monitor` must be listed. An
empty list is a network problem — work through
[Connect the App to the Rig](../ros/connect-to-rig.md) rather than restarting the
application.

Two signs from the application itself:

- **Every sensor LED is white** after the startup read → the axis node is not
  answering.
- **The first window took a minute to appear** → the application spent 5 seconds
  timing out on each service before drawing. That is what a missing rig looks
  like.

**Quick confirmation:** click **Capture** — a frame appears. Type `10` into an
axis distance box and click a move arrow — the axis moves. *A distance of `0` is
silently ignored; that is intended, not a fault.*

---

## 2. Home the axes

Click **All HOME**.

1. A **"Checking Home Status…"** dialog reads each origin sensor and reports
   `HOME` / `NOT HOME` / `N/A`. It closes itself after about a second.
2. The axes home in a fixed order — **yaw → pitch → X → Y → Z** — one at a time.
   Any axis already on home is skipped.
3. Each is polled until its origin sensor reads triggered and its move sensor
   reads stopped, twice running, with a **120-second timeout** per axis.
4. Positions and coordinate fields are zeroed as each finishes.

Everything is locked while this runs. When it ends, all five coordinate fields
should read `0.000` and every home LED should be red.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ AN AXIS THAT TIMES OUT DOES NOT STOP THE SEQUENCE</div>
  <div>
    The 120-second timeout returns failure for that axis and the run <strong>moves on to the next one</strong> — no dialog, no summary. Check all five coordinates read <code>0.000</code> afterwards; a stale value is your only signal that one did not make it.
  </div>
</div>

---

## 3. Show the pattern and check the camera

1. Open the [PCT Pattern Generator](./pct-pattern-generator.md). If this is a new
   rig, run **Setup Monitor Direction** first — nothing below works without it.
2. Load or build your pattern, click **Positive Pattern**, then **Update** for
   `TOP` (concentric) and `SIDE` (stripline).
3. Open [Monitor Viewer](./monitor-viewer.md) and check all five previews.
4. Set brightness — start at **TOP 5 %**, sides **1 %**.
5. Back on the main window, click **Capture** and look at the preview.

You want the pattern clearly visible, in focus, not blown out, and the fisheye
circle roughly centred in frame. Adjust brightness and exposure until it is.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ A BLACK CAPTURE IS USUALLY BRIGHTNESS AT 0 %</div>
  <div>
    <strong>Turn off</strong> in Monitor Viewer sets that screen's brightness field to <code>0%</code> and leaves it there. Showing a pattern again does not restore it — you have to type it back in.
  </div>
</div>

---

## 4. Take the positive and negative shots

Click **Pos Shot**, then **Neg Shot**.

Each one drives the whole rig by itself:

1. The concentric pattern goes to `TOP` and the stripline to all four `SIDE`
   screens, in the right polarity.
2. The application waits **300 ms** for the monitors to redraw.
3. One frame is captured, decoded and written to disk.
4. If Centering is on **Auto**, the pattern centre is found in *that* image and
   written into that mode's CPX/CPY.
5. The edge overlay is drawn and **both histograms are redrawn**.

<div className="custom-note">
  <div className="custom-note-title">📌 POSITIVE AND NEGATIVE ARE CENTRED INDEPENDENTLY</div>
  <div>
    Each shot finds its own centre from its own image. The two centres are not expected to be identical, and forcing them to match is not a fix for anything.
  </div>
</div>

**Do not skip either.** The calibration is built from where the positive and
negative intensity profiles cross. One shot on its own yields nothing.

If a capture fails you get *"No image received from camera server"* (nothing came
back) or *"Received data could not be decoded as an image"* (something did, but
not an image). Both are the camera node, not the window.

---

## 5. Check and correct the centre

Look at the preview. The ROI box, its diagonals and the circle should be centred
on the pattern.

### If Auto got it right

Nothing to do. Move on.

### If it did not

1. Select **Manual**.
2. Click the true centre on the preview. The mode flips itself back to **Auto**
   afterwards.
3. Fine-tune **PosThr** / **NegThr** — default `150` — if Auto keeps landing in
   the same wrong place. The threshold is what the seeded fallback uses.
4. Tick **Edge**, set a **Radius** matching the fisheye circle, and check the
   drawn circle sits on it.

Use **Locked** when you have a centre you trust and do not want a later shot
changing it.

### The `?` button — direction difference

The strongest single check that a capture is worth keeping.

<Shot
  id="fig-2"
  number="2"
  file="camera-calibration-direction-diff.png"
  caption="Direction difference on a good capture — most rows near zero."
  what="With a well-centred Pos and Neg pair loaded, click ? and capture the dialog showing the N−S section and the bold summary at the bottom."
  height={520}
/>

It runs the same 8-direction node detection Update Table uses, then compares
**opposite** directions: `N − S`, `W − E`, `NW − SE`, `SW − NE`. Every node is
listed with both values and `|A − B|`, **red at 5 px or more**, with a bold mean
and max per pair.

> Near zero means well-centred and symmetric. Large values mean the capture is
> off-centre — **fix it now**, because a bad round is not detectable later
> without re-shooting it.

It needs both shots: *"Take a Positive Shot and a Negative Shot first."*

---

## 6. Read the histograms

<Shot
  id="fig-3"
  number="3"
  file="camera-calibration-histogram-compare.png"
  caption="Comparison mode: red positive, green negative, white intersection lines."
  what="Tick BOTH the Pos and Neg box for one direction — N is easiest — on Histogram1, and crop to the plot so the red curve, green curve and white vertical lines are all clear."
  height={400}
/>

Tick **both** the Pos and Neg box for one direction. You get:

- the **positive** curve in red,
- the **negative** curve in green,
- **white vertical lines at every intersection** — these are the nodes the whole
  calibration is built from.

What you are looking for:

| Sign | Meaning |
|---|---|
| Clean crossings, evenly spaced | Good capture |
| Curves barely separated | Brightness too low, or exposure wrong |
| Flat-topped curves | Blown out — lower the brightness |
| Crossings clustered or missing at one end | The centre is off, or the pattern does not reach that far |
| Ragged extra crossings | Monitor-gap noise — turn **Clean Noise** on |

<div className="custom-note">
  <div className="custom-note-title">📌 ONLY THE FIRST Pos+Neg DIRECTION IS DRAWN</div>
  <div>
    Ticking both boxes for N <em>and</em> S shows you N only — the order is <code>n, s, w, e, nw, se, sw, ne</code>. Untick N to see S. To overlay several directions at once, tick them on <strong>one side only</strong> (all Pos, or all Neg): that switches to the multi-colour mode.
  </div>
</div>

**Clean Noise** in the header drops false crossings caused by the physical gaps
between monitors, and outliers too close to a real node. Toggling it redraws both
plots immediately, so you can see exactly what it removed.

---

## 7. Repeat for each round

Move the camera to the next position, then repeat steps 3 to 6.

**Before moving, get this round's data out** — the three image filenames are
about to be overwritten:

1. Open the [Calibration Result](./cali-result/index.md) window.
2. Select the round tab for this position.
3. **Update Table**, then save.
4. Copy `capture_positive_shot.png` and `capture_negative_shot.png` into that
   round's folder.

The expected layout is one numbered subfolder per round:

```
my_calibration/
├── 1/   moil_cali_result.xlsx, capture_positive_shot.png, capture_negative_shot.png
├── 3/   …
├── 5/   …
├── 7/   …
└── 9/   …
```

### Or let Auto Calibrate do it

**Auto Calibrate** in the header runs all five positions unattended: five
positive and five negative shots per position at 10-second intervals, Update
Table and save into the matching round folder, then Z back 20 mm and repeat. The
full sequence is documented in
[Main Window → Auto Calibrate](../system-overview/main-window.md#9-auto-calibrate).

It does **not** set the pattern up or move the camera to the first position. Do
steps 1–3 by hand, then hand it over.

---

## Before moving on

| Check | Where |
|---|---|
| All five coordinates read `0.000` after homing | Axis panel |
| Pattern visible on all five screens | Monitor Viewer previews |
| Both Pos and Neg shots taken this round | Pattern Mode + the two CPX/CPY pairs are filled |
| Direction differences near zero | The `?` dialog |
| Curves cross cleanly | Histogram, comparison mode |
| This round's Excel and PNGs saved | Its numbered folder |

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Every sensor LED white | The axis node is not answering — [Connect the App to the Rig](../ros/connect-to-rig.md) |
| First window took a minute | Service probes timing out. Same cause |
| Move button does nothing | The distance is `0`. This is intended |
| A control will not respond | An axis is moving. Only that axis's STOP is live until it finishes |
| *"No image received from camera server"* | `/moil_camera` is not publishing |
| Capture is black | Brightness at `0%` after a Turn off, or TOP left off by **4Side turn off** |
| Pattern on the wrong screen | Direction mapping wrong or never done. Re-run **Setup Monitor Direction** |
| Pos and Neg look identical | The pattern did not change between shots — check the Pattern Generator is reachable and both screens updated |
| Centre lands somewhere absurd | The gradient fit failed and it fell back to the seeded method from a bad starting point. Set it Manually, then adjust the threshold |
| Direction differences large and red | Off-centre capture. Fix the centre and re-shoot |
| Histogram empty although shots were taken | No direction boxes ticked, or the image files were deleted by **Reset** |
| Extra ragged crossings | Turn **Clean Noise** on |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `camera-calibration-ready.png` | Main window with a completed positive shot |
| 2 | `camera-calibration-direction-diff.png` | The `?` dialog on a good capture |
| 3 | `camera-calibration-histogram-compare.png` | Comparison mode: red, green, white lines |

Worth adding — a **good vs bad capture pair**, side by side:

| File | What to capture |
|---|---|
| `camera-calibration-preview-good.png` | Preview with the ROI centred on the pattern |
| `camera-calibration-preview-bad.png` | The same with the centre clearly off |

---

## Source material

| What | Where |
|---|---|
| Capture and shot flow | `cpp/src/controllers/controller_main.cpp` |
| Captured-image viewer | `cpp/src/controllers/controller_dialog_captured_image.cpp` |
| Node detection, centring, histograms | `cpp/src/core/algorithm/moilcali_algorithm.cpp` |
| Camera ROS client | `cpp/src/models/device/camera_ros_client.{h,cpp}` |
| Axis ROS client | `cpp/src/models/device/axis_ros_client.{h,cpp}` |
| Rig-side camera node | `Server/v2.0.0/packages/moil_camera_ros/` |
