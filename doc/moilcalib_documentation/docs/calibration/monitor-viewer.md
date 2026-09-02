---
id: monitor-viewer
slug: /calibration/monitor-viewer
title: Monitor Viewer
---

# Monitor Viewer

Controls the five screens around the camera: what each one shows, how bright it
is, and which physical display is which compass direction.

Opened from **Monitor / Pattern → Monitor Viewer** on the main window, or by the
`monitor_viewer` HELP key.

<Shot
  id="fig-1"
  number="1"
  file="monitor-viewer-overview.png"
  caption="Monitor Viewer, laid out as a compass."
  what="Open the window with patterns loaded on all five directions so each preview thumbnail has content. Capture the whole window including the Control menu and the 4Side turn off toolbar."
  height={560}
/>

---

## 1. The compass layout

Five panels, one per screen, arranged the way the screens appear **in the camera
image** — not as a flat list:

```
                ┌─────────┐
                │    N    │   up (front)
                └─────────┘
   ┌─────────┐  ┌─────────┐  ┌─────────┐
   │    W    │  │   TOP   │  │    E    │
   │  left   │  │ centre  │  │  right  │
   └─────────┘  └─────────┘  └─────────┘
                ┌─────────┐
                │    S    │   down
                └─────────┘
```

That orientation is not cosmetic — it is fixed by how the calibration samples
the image. `n` walks **up** the rows from the centre, `s` walks **down**, `w`
walks **left** along the columns, `e` walks **right**, and **Top** is the screen
at the centre of the frame. Laying the panels out this way means you can read the
rig off a camera shot without any mental compass work.

### Each panel

| Control | Object name | What it does |
|---|---|---|
| Preview | `label_pattern_<dir>` | Thumbnail of the current image, scaled to 300 × 300 (**aspect ratio is not preserved**, so a wide pattern looks squashed here) |
| **Img path:** | `lineedit_<dir>` | Path of the image. **Editable** — type or paste a path and press Enter to preview it |
| **Browse…** | `btn_browse_<dir>` | File picker (`.jpeg`, `.jpg`, `.png`). Sets the path and previews it, but does **not** send anything |
| **Brightness:** | `lineedit_brightness_<dir>` | Percentage. Defaults: **TOP 5 %**, the four sides **1 %** |
| **Update** | `btn_update_<dir>` | Sends the image **and** the brightness to that screen |
| **Turn off** | `btn_turnoff_<dir>` | Blacks that screen out — see [§4](#4-turning-screens-off) |

`<dir>` is `top`, `n`, `w`, `s` or `e`.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ BROWSE DOES NOT SEND ANYTHING</div>
  <div>
    Browse and the path field only change what is <em>staged</em> in this window. Nothing reaches the screen until you click <strong>Update</strong> for that direction. A preview thumbnail showing the right pattern is not evidence that the screen is showing it.
  </div>
</div>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ AN UNREADABLE PATH FAILS SILENTLY</div>
  <div>
    If the file in the path field cannot be opened, <strong>Update does nothing at all</strong> — no message, no change on screen. A typo in a pasted path looks identical to a monitor that is not responding.
  </div>
</div>

### What Update actually sends

Two calls, in this order, synchronously — with **no abort timeout**, so a
non-responding monitor node makes the window sit still until it answers:

1. `show_pattern(direction, image_bytes)`
2. `set_brightness(direction, brightness)` — the `%` sign is stripped before
   sending

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ BOTH CALLS ARE REQUIRED</div>
  <div>
    <code>show_pattern</code> on its own leaves the screen <strong>dark or green</strong>. This is exactly why the Pattern Generator's <strong>Update to Monitor</strong> routes through this window instead of talking to the monitor node itself.
  </div>
</div>

---

## 2. Setup Monitor Direction

The one thing to do before anything else on a new rig. Until the mapping is set,
the monitor node has no idea which physical display is North.

The button is on this window's toolbar, and also on the Pattern Generator's —
both open this same dialog, because this window owns the monitor client.

<Shot
  id="fig-2"
  number="2"
  file="monitor-viewer-setup-direction.png"
  caption="Setup Monitor Direction, with the five spin boxes laid out as a compass."
  what="Open the dialog from the toolbar and capture it after clicking Show Numbers, so the status line at the bottom has a response in it."
  height={430}
/>

### The procedure

1. Click **Show Numbers on Screens**. Each physical screen displays a number.
2. **Take a camera shot** and look at it. Read the number off each screen.
3. Type each number into the matching box. The boxes are laid out as a compass
   cross — `N` up, `W` left, `Top` centre, `E` right, `S` down — **positioned the
   way the screens appear in that camera image**, so there is no compass guessing
   to do.
4. Click **Apply Mapping**.

Each box accepts 1–16. The defaults are Top 1, N 2, W 3, S 4, E 5.

The status line at the bottom reports what came back. **"No response from
monitor node."** means `/moil_monitor` is not running — see
[Connect the App to the Rig](../ros/connect-to-rig.md).

---

## 3. Displaying a pattern

### From the Pattern Generator (the normal route)

Click **Update** in the [PCT Pattern Generator](./pct-pattern-generator.md). It
renders, writes `image_cali/pattern_circle_<dir>.png`, and hands it here. This
window then fills in the path, previews it, and sends image plus brightness.

Choosing `SIDE` there copies `pattern_circle_side.png` onto all four side files
and updates **W, N, S and E** in turn.

### By hand

1. **Browse…** to the image, or paste its path and press Enter.
2. Check the preview.
3. Set the brightness.
4. Click **Update** for that direction.

---

## 4. Turning screens off

**Turn off** does not power the display down. It:

1. Generates a 1920 × 1920 all-black PNG at `image_cali/dark.png`.
2. Puts that path in the direction's path field and previews it.
3. Sets that direction's brightness field to **`0%`**.
4. Sends both.

So a "turned off" screen is showing black at zero brightness.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ TURN OFF OVERWRITES THE BRIGHTNESS FIELD</div>
  <div>
    It is left at <code>0%</code>. When you show a pattern again, <strong>type the brightness back in</strong> — 5 % for TOP, 1 % for the sides — or the pattern goes up invisible and the capture is black.
  </div>
</div>

### 4Side turn off

The toolbar button blacks out **N, W, S and E** in one click, leaving **TOP
untouched**. That asymmetry is deliberate: it is the setup for capturing with
only the top screen lit.

The **Control** menu carries the same action.

---

## 5. Where the images live

| File | What it is |
|---|---|
| `image_cali/pattern_circle_top.png` | Current TOP pattern |
| `image_cali/pattern_circle_{n,w,s,e}.png` | Current side patterns |
| `image_cali/pattern_circle_side.png` | The source copied to all four sides |
| `image_cali/dark.png` | The all-black image Turn off uses |
| `image_cali/_tmp_pattern_circle.png` | Last render from the Pattern Generator |

All are regenerated on demand. **Reset** on the main window deletes them.

---

## 6. Recommended workflow

1. **Setup Monitor Direction** — once per rig, and again whenever the screens are
   physically rearranged.
2. Generate patterns in the [Pattern Generator](./pct-pattern-generator.md) and
   click **Update** there.
3. Come back here and check all five previews.
4. Set brightness per screen. Start at TOP 5 %, sides 1 %, and adjust from the
   histograms on the main window.
5. Take a **Capture** on the main window and confirm the pattern is visible and
   not blown out.

---

## 7. Technical notes

**The node is only a transport layer.** The real display control — win32 and
DDC-CI — lives in `mvc_model/moil_monitor`, **outside the application
repository**. `run_monitor_lan.bat` points at it through `MOIL_MONITOR_PATH`. If
that folder is moved or deleted, the monitor stops working even though the node
starts normally.

**The services behind the buttons:**

| Button | Service |
|---|---|
| Update | `ShowPattern` + `SetBrightness` |
| Turn off | the same two, with a black image at 0 % |
| Show Numbers | `MonitorCommand` |
| Apply Mapping | `SetDisplayDirection` |

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Update does nothing, no message | The path field points at a file that cannot be opened |
| Window freezes on Update | The monitor node is not answering. There is no timeout — it waits |
| "No response from monitor node." | `/moil_monitor` is not running |
| Screen is dark or green after a pattern was sent | Brightness was not applied. Set it and click Update again |
| Pattern on the wrong screen | Direction mapping is wrong or was never done. Re-run **Setup Monitor Direction** |
| Screen stays black after Turn off, then a pattern | Brightness is still `0%` — type it back in |
| Preview looks squashed | Expected. Previews are forced to 300 × 300 and ignore aspect ratio |
| Capture is black although the screens look lit | Brightness too low for the exposure, or TOP was left off by 4Side turn off |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `monitor-viewer-overview.png` | Whole window, all five previews filled |
| 2 | `monitor-viewer-setup-direction.png` | Setup Monitor Direction dialog after Show Numbers |

Worth adding if you can: a photo of the rig with numbers on the screens, next to
the camera shot of the same — it makes step 2 of the mapping obvious in a way no
UI capture can.

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/monitor_viewer.ui` |
| Behaviour | `cpp/src/controllers/controller_monitor_viewer.{h,cpp}` |
| ROS client | `cpp/src/models/device/monitor_ros_client.{h,cpp}` |
| Services | `ros/moil_interfaces/srv/{ShowPattern,ClosePattern,SetBrightness,GetBrightness,SetDisplayDirection,GetDisplayDirection,MonitorCommand}.srv` |
| Rig-side node | `Server/v2.0.0/packages/moil_monitor_ros/` (includes `pattern_render.py`) |
| Direction reference images | `Server/v2.0.0/{n,s,e,w,top}_image.png` |
