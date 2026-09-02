---
id: main-window
slug: /system-overview/main-window
title: Main Window Reference
---

# Main Window Reference

This is the window every calibration is run from. It drives the 5-axis stage,
takes the captures, finds the pattern centre, draws the two histograms, and
launches every other window in the system.

It is what the **HELP** button opens with the key `main_window` — and also what
the Curve Color sub-window points at.

<Shot
  id="fig-1"
  number="1"
  file="main-window-overview.png"
  caption="The Main Window at startup, with a positive shot loaded."
  what="Full window, maximised, after one Pos Shot so the preview and both histograms have content. Include the header strip at the top."
  height={560}
/>

---

## Panel map

| # | Area | What it is for |
|---:|---|---|
| 1 | **Header strip** (top right) | View, Reset, Auto Calibrate, Clean Noise, theme toggle, HELP |
| 2 | **Axis Control Panel** | Move, home and stop the five axes; read their sensors and coordinates |
| 3 | **Camera Panel** | Capture, Pos Shot, Neg Shot, Open Img, and the image preview |
| 4 | **Centering** | Find and correct the pattern centre on the positive and negative captures |
| 5 | **Histogram1 / Histogram2** | Intensity profiles along the eight compass directions |
| 6 | **Monitor / Pattern** | Launches PCT Pattern Generator and Monitor Viewer |
| 7 | **Calibration Result** | Launches Moil Cali Result and Recommend PCT |
| 8 | **Validation** | Launches Setup Center and 3D Verification |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THERE IS NO SERVER URL PANEL ANY MORE</div>
  <div>
    Version 1.1 had an <strong>HTTP Server URL</strong> panel at the top of this window with three address fields and Update buttons. It is gone from the form entirely. The axis, camera and monitor are ROS 2 nodes now, found by discovery on domain 42 — there is nothing to type. If a device does not respond, the problem is on the network, not in this window: see <a href="../ros/connect-to-rig.md">Connect the App to the Rig</a>.
  </div>
</div>

---

## 1. Header strip

Six controls, laid out in the form as `widget_menu_corner` so they can be moved
in Qt Designer.

| Control | Object name | What it does |
|---|---|---|
| **View** | `btn_view` | Opens a popup menu. Its only entry is **Theme**, listing every theme the application ships |
| **Reset** | `btn_reset` | Returns the whole application to a fresh state — see below |
| **Auto Calibrate** | `autoCalibrate` | Runs a full unattended five-position calibration — see [§9](#9-auto-calibrate) |
| **Clean Noise** | `cleanNoiseToggle` | Checkable. Filters false crossings out of node detection |
| Theme toggle | `themeToggle` | One-click swap between Light and Dark. Its label shows the theme you would switch **to** — `☾ Dark` while you are in Light |
| **HELP** | `helpButton` | Opens this page in your browser |

<Shot
  id="fig-2"
  number="2"
  file="main-window-header-strip.png"
  caption="The header strip."
  what="Crop tightly to the six header controls in the top-right corner. Take it in Light theme so the ☾ Dark toggle label is visible."
  height={110}
/>

### Clean Noise

A **toggle**, not an action. While it is on, intersection-node detection drops
two kinds of false crossing: the ones caused by the physical gaps between
monitors, and outliers that sit too close to a real node.

Toggling it immediately redraws **both** histograms, so you can see the effect
without pressing Show Curve again. The setting is global — the Calibration
Result window's calculations use the same flag.

### Reset

Asks for confirmation, then:

1. Stops every running axis monitor, so nothing repaints after the reset.
2. Clears the preview, the image path, pattern mode, both resolutions, all four
   centre fields, and alpha/beta.
3. Clears both histogram plots.
4. **Deletes cached images from `image_cali/`** — `_tmp_pattern_circle.png`,
   `capture_single_image.png`, `capture_positive_shot.png`,
   `capture_negative_shot.png`, and every `pattern_circle_*.png`.
5. Re-reads all 20 axis sensors.
6. Destroys and recreates every sub-window, so their tables, pattern layers and
   file paths return to their designed defaults — exactly like restarting the
   program.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ RESET DELETES CAPTURED IMAGES</div>
  <div>
    The positive and negative shots in <code>image_cali/</code> are removed. Anything you still need must already be saved into a round folder. Reference sample images and the <code>pattern_json</code> configs are left alone.
  </div>
</div>

---

## 2. Axis Control Panel

Five axes — **X**, **Y**, **Z**, **Yaw**, **Pitch** — each with an identical row
of controls.

<Shot
  id="fig-3"
  number="3"
  file="main-window-axis-panel.png"
  caption="Axis Control Panel: five axis rows, sensor LEDs, relative-move boxes, coordinates and alpha/beta."
  what="Crop to the Axis Control Panel group box. Capture it connected to the rig so the LEDs are green/red rather than white."
  height={480}
/>

### 2.1 What each row contains

| Element | Widget | Notes |
|---|---|---|
| Direction buttons | `btn_relative_<axis>_<dir>` | X: `◂ Left` / `Right ▸` · Y: `▴ Up` / `Down ▾` · Z: `⊗Forward` / `⊙Back` · Yaw: `◂ Left` / `Right ▸` · Pitch: `▴ Up` / `Down ▾` |
| Distance | `doublespinbox_relative_<axis>` | mm for X/Y/Z, degrees for Yaw/Pitch |
| **Speed** | `combobox_speed_<axis>` | Defaults to `High` |
| Sensor LEDs | `btn_sensor_<axis>_<low\|home\|high>` | Labelled `L`/`H`/`R`, `D`/`H`/`U`, `B`/`H`/`F` |
| Moving LED | `btn_sensor_<axis>_move` | Labelled `M` |
| **STOP** | `btn_stop_<axis>` | |
| Coordinate | `lineedit_coordinate_<axis>` | Reads `？` until the axis is homed or read |

Below the five rows: **All HOME** (`btn_all_home`), and the read-only **α** and
**β** fields derived from the yaw and pitch coordinates.

### 2.2 Making a move

Type a distance, pick a speed, click a direction button. What happens then:

1. **A distance of `0` is silently ignored.** The click does nothing at all — no
   message, no movement. This is deliberate, and it is the single most common
   "the axis is broken" report.
2. That axis is locked immediately, on the UI thread.
3. The move is sent on a worker thread, so the window never freezes.
4. An axis monitor starts. It drives the LEDs and the coordinate field live, and
   unlocks the axis once it has stopped.

### 2.3 The sensor LEDs

Three limit/origin LEDs plus one moving LED per axis, 20 in total, each mapped to
one sensor on the axis node.

| LED kind | Colour | Meaning |
|---|---|---|
| Limit / origin | 🔴 Red | Sensor **triggered** — the axis is sitting on that limit or on origin |
| Limit / origin | 🟢 Green | Sensor clear |
| Moving (`M`) | 🟡 Yellow | Axis is moving. It blinks black⇄yellow on a 500 ms timer |
| Moving (`M`) | 🟢 Green | Axis is stopped |
| Either | ⬜ White | **Unknown** — the sensor could not be read. Usually means the axis node is not running |

The `L`, `H`, `R`-style LEDs are also **buttons**: clicking one homes that axis
or drives it to that limit. Two are pure indicators and ignore clicks entirely —
every `M` LED, and `btn_sensor_z_forward`.

### 2.4 Startup sensor read

On launch — and again after Reset — the application reads all 20 sensors behind
a modal **"Initializing Sensors…"** dialog with one row per sensor and a
progress bar. Each row ends as `TRIGGERED` / `CLEAR` for limits, `MOVING` /
`STOPPED` for the move sensors, or `ERROR` when it could not be read.

Any axis found sitting on its origin sensor has its controller position written
to zero and its coordinate field set from the axis.

If the very first probe returns nothing, the whole read is skipped, every LED
stays white, and you get a connection warning.

<Shot
  id="fig-4"
  number="4"
  file="main-window-init-sensors-dialog.png"
  caption="The Initializing Sensors dialog, mid-read."
  what="Restart the app connected to the rig and capture the modal while it is still counting up — it closes on its own shortly after the twentieth read."
  height={430}
/>

### 2.5 The safety lock

The rule is simple and absolute:

> **While any axis is moving, every control is disabled except the STOP buttons
> of the axes that are actually moving.** Sensor LEDs stop accepting clicks.
> When nothing is moving, everything is enabled again.

Clicking a move button also moves keyboard focus onto that axis's STOP, so the
stop is one keypress away without aiming the mouse.

### 2.6 All HOME

<Shot
  id="fig-5"
  number="5"
  file="main-window-check-home-dialog.png"
  caption="Checking Home Status, run before the homing sequence starts."
  what="Click All HOME and capture the modal that appears first. It shows PITCH / YAW / X / Y / Z and auto-closes after about a second."
  height={300}
/>

1. A modal **"Checking Home Status…"** dialog reads each axis's origin sensor and
   reports `HOME` (green), `NOT HOME` (red) or `N/A` (grey). It closes itself
   after roughly a second.
2. Every motion control is disabled and focus moves to **STOP Z**.
3. The axes are homed **in a fixed order: yaw → pitch → X → Y → Z**, one at a
   time. **An axis already on home is skipped.**
4. Each axis is polled every 100 ms until its origin sensor has read triggered
   *and* its move sensor has read not-moving on **two consecutive polls**, with a
   **120-second timeout** per axis.
5. On success the controller position and coordinate field are zeroed. X and Y
   blink while homing and are zeroed by the blink teardown instead.
6. When the sequence finishes, all controls come back.

---

## 3. Camera Panel

<Shot
  id="fig-6"
  number="6"
  file="main-window-camera-panel.png"
  caption="Camera Panel with a positive shot loaded and the centring overlay drawn."
  what="Crop to the Camera Panel group box after a Pos Shot, so Pattern Mode reads Positive, both resolutions are filled, and the ROI box and diagonals are visible on the preview."
  height={470}
/>

### 3.1 Buttons and fields

| Control | Object name | What it does |
|---|---|---|
| **Capture** | `btn_capture` | Grabs one frame. Saves to `image_cali/capture_single_image.png`. Leaves Pattern Mode empty, so no overlay and no centre detection |
| **Pos Shot** | `btn_positive_shot` | Pushes the **positive** pattern to the monitors, waits 300 ms, captures, saves to `capture_positive_shot.png`, sets Pattern Mode to `Positive` |
| **Neg Shot** | `btn_negative_shot` | Same with the **inverted** pattern → `capture_negative_shot.png`, Pattern Mode `Negative` |
| **Open Img** | `btn_browse` | Load an existing image from disk instead of capturing |
| **?** | `btn_direction_diff` | Direction-difference check — see [§3.4](#34-the--button-direction-difference) |
| Pattern Mode | `lineedit_pattern_mode` | `Positive`, `Negative`, or empty |
| Org Res | `lineedit_org_res` | Resolution of the captured file, e.g. `1920x1920` |
| Cali Res | `lineedit_cali_res` | Resolution it was scaled to for the preview |
| Img Path | `lineedit_image_path` | Full path of the image on screen |
| Preview | `label_capture` | The image itself |

### 3.2 What a shot button actually does

Reading the flow in order matters, because the 300 ms wait is the part people
trip over:

1. Pattern Mode and Img Path are set.
2. The Pattern Generator pushes the matching pattern to the monitors — the
   positive pattern for Pos Shot, the inverted one for Neg Shot. **This is why
   the two captures differ.**
3. On a worker thread: sleep **300 ms** so the monitors have actually redrawn,
   then request one frame from the camera node.
4. Back on the UI thread the frame is decoded and written to disk.
5. If Pattern Mode is `Positive` or `Negative` **and** the Centering mode is
   **Auto**, the pattern centre is found in *this* image and written into that
   mode's CPX/CPY fields. Positive and negative are centred independently.
6. The edge overlay is drawn, and **both histograms are redrawn**.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ "NO IMAGE RECEIVED FROM CAMERA SERVER"</div>
  <div>
    An empty response gives this warning; data that cannot be decoded gives <em>"Received data could not be decoded as an image."</em> Both mean the camera node — not the window. Check <code>/moil_camera</code> is publishing.
  </div>
</div>

### 3.3 Preview interaction

| Action | Result |
|---|---|
| **Single left-click** | Sets the pattern centre at that point — but only when Pattern Mode is set **and** Centering is on **Manual** |
| **Double left-click** | Opens the full captured-image viewer |

The two are told apart by a timer: a press arms a one-shot timer of the system
double-click interval, and a genuine double-click cancels it. So a single click
registers with a short, deliberate delay.

The click position is mapped back to original-image pixels using the Org Res /
Cali Res ratio, and the letterbox offset inside the preview is subtracted first.
Clicks outside the image are ignored. After a manual click the mode flips itself
back to **Auto**.

### 3.4 The `?` button (direction difference)

Runs the same 8-direction node detection that Update Table uses, on the current
positive and negative captures, then reports the difference between **opposite**
directions: `N − S`, `W − E`, `NW − SE`, `SW − NE`.

The dialog lists every node with its two values and `|A − B|`, **red when the
difference is 5 px or more**, and a bold summary of mean and max per pair.

> Near zero means a well-centred, symmetric capture. Large values mean the
> capture is off-centre and the round is not worth computing.

It refuses to run without both a positive and a negative shot:
*"Take a Positive Shot and a Negative Shot first."*

<Shot
  id="fig-7"
  number="7"
  file="main-window-direction-diff.png"
  caption="ICT Direction Difference dialog."
  what="With a good Pos and Neg shot loaded, click ? and capture the dialog scrolled to show at least one section header and several node rows, including a red row if you have one."
  height={520}
/>

---

## 4. Centering

Everything that decides where the pattern centre is.

<Shot
  id="fig-8"
  number="8"
  file="main-window-centering-panel.png"
  caption="Centering panel."
  what="Crop to the Centering group box with a positive shot loaded, so CPX/CPY hold real values."
  height={340}
/>

### 4.1 Fields

| Field | Object name | Default | Meaning |
|---|---|---|---|
| Center ROI | `lineedit_center_roi` | `100` | Radius of the overlay box, diagonals and circle drawn on the preview |
| PosThr | `lineedit_posthr` | `150` | Intensity threshold for detecting the centre on the positive image |
| NegThr | `lineedit_negthr` | `150` | Same for the negative image |
| Positive CPX / CPY | `lineedit_positive_cpx` / `_cpy` | `1003` / `836` | The positive image's centre |
| Negative CPX / CPY | `lineedit_negative_cpx` / `_cpy` | `1003` / `836` | The negative image's centre |

Per side there is also an **Edge** checkbox, a **Radius**, a **Thickness**
(default `10`) and a **Color** swatch button. With Edge ticked, a circle of that
radius, thickness and colour is drawn on the preview — the visual check that the
centre and the fisheye circle agree. Clicking a colour swatch opens a colour
picker and redraws both overlays.

Editing Center ROI and pressing Enter redraws the overlay immediately.

### 4.2 The three modes

| Mode | Radio button | Behaviour |
|---|---|---|
| **Auto** | `radiobutton_auto` | Every Pos/Neg shot finds its own centre automatically. This is the normal setting |
| **Manual** | `radiobutton_manual` | A single click on the preview sets the centre, then the mode returns to Auto by itself |
| **Locked** | `radiobutton_locked` | The centre is left exactly as it is |

### 4.3 How Auto finds the centre

Two different algorithms, and it matters which one is running.

**On a fresh capture** the centre is found from the image alone, with no seed.
On a concentric pattern the intensity gradient at every ring edge points along
the local radius, so the line through each edge pixel passes through the common
centre. The application takes a Sobel gradient, keeps edges above 15 % of the
maximum magnitude, and solves for the point closest to all of those lines in a
least-squares sense. A couple of refinement passes then restrict the fit to the
concentric core, dropping off-centre outer edges.

**If that fit fails**, it falls back to the seeded method: start from whatever
CPX/CPY currently hold, run threshold-based ROI detection, and repeat until the
centre stops moving or 20 iterations pass.

---

## 5. Histogram1 and Histogram2

Two identical plots, each with 16 direction checkboxes — eight **Pos**, eight
**Neg** — and three buttons.

<Shot
  id="fig-9"
  number="9"
  file="main-window-histogram-panel.png"
  caption="Histogram1 with several directions plotted."
  what="Crop to one histogram group box with a Pos and Neg shot loaded and a few direction boxes ticked, so curves are actually drawn."
  height={430}
/>

Both plots are fixed to **X 0–2000** and **Y 0–270**.

### 5.1 Controls

| Control | Object name | What it does |
|---|---|---|
| Direction boxes | `checkbox_pos_<dir>_h<n>`, `checkbox_neg_<dir>_h<n>` | `n`, `s`, `w`, `e`, `nw`, `se`, `sw`, `ne`. **Ticking one redraws immediately** — you do not have to press Show Curve |
| **Show Curve** | `btn_show_curve_histogram<n>` | Redraws that plot |
| **Curve Color** | `btn_curve_color_histogram_<n>` | Opens the Curve Color window |
| **Pop Up** | `btn_pop_up_histogram_<n>` | Opens the plot in its own window |

### 5.2 The two drawing modes

Which one you get depends on the checkboxes, and this catches people out:

**Mode 1 — comparison.** If **any** direction has *both* its Pos and Neg boxes
ticked, that one direction is drawn as a positive curve in **red**, a negative
curve in **green**, and **white vertical lines at every intersection** — the
intersecting nodes the calibration is actually built from.

> Only the **first** such direction is drawn, in the order
> `n, s, w, e, nw, se, sw, ne`. Ticking Pos+Neg for both N and S shows you N
> only. This is intentional, and matches the original Python behaviour.

**Mode 2 — overlay.** Otherwise every ticked Pos direction is drawn, then every
ticked Neg direction, each in a different colour from an eight-colour palette
(red, green, blue, yellow, magenta, cyan, orange, violet).

The four diagonal directions are scaled by `√2` on the X axis, so a diagonal
profile lines up with a straight one.

---

## 6. Monitor / Pattern

| Button | Object name | Opens |
|---|---|---|
| **PCT (Pattern Generator)** | `btn_pattern_generator` | [PCT Pattern Generator](../calibration/pct-pattern-generator.md) |
| **Monitor Viewer** | `btn_monitor_viewer` | [Monitor Viewer](../calibration/monitor-viewer.md) |

The two are wired together behind the scenes. **Update to Monitor** in the
Pattern Generator routes through the Monitor Viewer, so it sends *both*
`show_pattern` and `set_brightness` — sending `show_pattern` alone leaves the
screen dark or green. **Setup Monitor Direction** in the Pattern Generator opens
the Monitor Viewer's mapping dialog, which is what owns the monitor client.

---

## 7. Calibration Result

| Button | Object name | Opens |
|---|---|---|
| **Moil Cali Result** | `btn_cali_result` | [Main Cali Result](../calibration/cali-result/index.md), sized 1600 × 900 |
| **Recommend PCT** | `btn_pct_recommend` | [PCT Recommend](../calibration/cali-result/pct-recommend.md) |

Two values cross the window boundary when you press **Update Table** in the Cali
Result window:

- the four centre values from this window's CPX/CPY fields, and
- the PCT list from the Pattern Generator.

PCT Recommend pulls its reference rows from round 1's PCT and ICT-average
columns in the Cali Result window.

---

## 8. Validation

| Button | Object name | Opens |
|---|---|---|
| **Setup Center** | `btn_setup_center` | [Setup Center](../verification/setup-center.md) |
| **3D Verification** | `btn_3d_distance` | [3D Verification](../verification/3d-verification.md) |

Setup Center is seeded with the **raw positive capture** — no overlays — if one
exists. The window appears first and the image is loaded a moment later, so the
heavy anypoint and panorama build does not delay it opening.

---

## 9. Auto Calibrate

Runs a complete five-position calibration with no further input. Budget roughly
**nine minutes** of shooting per position.

<Shot
  id="fig-10"
  number="10"
  file="main-window-auto-calibrate-running.png"
  caption="Auto Calibrate mid-run, the button showing its progress."
  what="Start an Auto Calibrate run and capture the header strip while the button reads something like [3] Pos 2/5."
  height={110}
/>

**Before you start:** the pattern must be generated, the monitors showing it, and
the camera at the first Z position. Auto Calibrate does not set any of that up.

1. You are asked to choose or create a folder. It creates subfolders
   **`1`, `3`, `5`, `7`, `9`** inside it — one per Z position.
2. For each position, in order:
   - **five Positive Shots**, one every 10 seconds,
   - **five Negative Shots**, one every 10 seconds,
   - Clean Noise is forced **on**, then Update Table and Save run on the round
     tab matching the folder name (folder `3` → `round_3`), writing
     `moil_cali_result.xlsx` into that subfolder,
   - the last positive and negative captures are copied in as
     `capture_positive_shot.png` and `capture_negative_shot.png`.
3. Between positions, **Z moves back 20 mm**. The run polls every 500 ms until
   the move finishes — with a 90-second safety timeout — then waits a further
   2 seconds to settle.
4. When all five are done: *"Done. Saved rounds to: …/{1,3,5,7,9}"*.

The button is disabled for the whole run and reports where it is:
`[3] Pos 2/5`, `[3] Neg 4/5`, `[3] Saving…`, `[5] Z back…`.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE 20 mm Z STEP IS FIXED</div>
  <div>
    So are the five folder names, the five shots per mode, and the 10-second gap. None of them is exposed in the interface — changing any of them means changing <code>controller_main.cpp</code>. If your rig needs a different spacing, drive the rounds by hand.
  </div>
</div>

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `main-window-overview.png` | Whole window, maximised, one Pos Shot loaded |
| 2 | `main-window-header-strip.png` | The six header controls, Light theme |
| 3 | `main-window-axis-panel.png` | Axis Control Panel, connected so LEDs are coloured |
| 4 | `main-window-init-sensors-dialog.png` | "Initializing Sensors…" modal mid-read |
| 5 | `main-window-check-home-dialog.png` | "Checking Home Status…" modal |
| 6 | `main-window-camera-panel.png` | Camera Panel after a Pos Shot |
| 7 | `main-window-direction-diff.png` | The `?` dialog with node rows visible |
| 8 | `main-window-centering-panel.png` | Centering group box, CPX/CPY filled |
| 9 | `main-window-histogram-panel.png` | One histogram with curves drawn |
| 10 | `main-window-auto-calibrate-running.png` | Header strip during an Auto Calibrate run |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/mainwindow_main.ui` |
| Behaviour | `cpp/src/controllers/controller_main.cpp` |
| Curve Color sub-window | `cpp/ui/curve_color.ui`, `cpp/src/controllers/controller_curve_color.cpp` |
| Node detection, centring, histograms | `cpp/src/core/algorithm/moilcali_algorithm.cpp` |
| LED colours | `cpp/src/views/widgets/SensorStyles.h` |
| HELP button wiring | `cpp/app/Help.cpp` |
