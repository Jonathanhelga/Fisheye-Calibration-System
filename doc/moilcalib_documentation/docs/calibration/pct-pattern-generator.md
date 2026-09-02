---
id: pct-pattern-generator
slug: /calibration/pct-pattern-generator
title: PCT Pattern Generator
---

# PCT Pattern Generator

Draws the calibration patterns and pushes them to the monitors around the camera.

**This is step 1 of a calibration, not a side utility.** Nothing the camera
captures means anything until the right pattern is on the right screen, at the
right brightness, with the physical displays correctly mapped to compass
directions.

Opened from **Monitor / Pattern → PCT (Pattern Generator)** on the main window,
or by the `pattern_generator` HELP key.

<Shot
  id="fig-1"
  number="1"
  file="pattern-generator-overview.png"
  caption="The Pattern Generator window."
  what="Open the window with a concentric pattern rendered, so the preview on the left is not blank. Capture the whole window including the File menu and the Setup toolbar."
  height={560}
/>

---

## The three pattern types

| Type | Goes on | Default resolution | Layers |
|---|---|---:|---:|
| **Concentric** | `TOP` — the screen above the camera | 1920 × 1920 | 25 |
| **Stripline** | `SIDE` — all four side screens at once | 3840 × 2160 | 50 |
| **Chessboard** | any single direction | you set it | — |

Concentric and stripline are what a calibration run uses. Chessboard feeds
[3D Verification](../verification/3d-verification.md).

<div className="custom-note">
  <div className="custom-note-title">📌 SELECTING "SIDE" FANS OUT TO FOUR SCREENS</div>
  <div>
    Choosing <code>SIDE</code> in the monitor combo box and clicking <strong>Update</strong> sends the pattern to <strong>N, W, S and E</strong> in one click, and writes four files — <code>pattern_circle_n.png</code>, <code>_w</code>, <code>_s</code>, <code>_e</code>. Any other selection is just itself.
  </div>
</div>

---

## 1. Before anything else — Setup Monitor Direction

The application has no way to know which physical screen is North. Until you tell
it, every pattern goes to the wrong place.

**Setup Monitor Direction** sits on the window's toolbar. It opens the Monitor
Viewer's mapping dialog — the two windows share one monitor client, which is why
the button lives here but the dialog belongs there. The procedure is documented
in [Monitor Viewer → Setup Monitor Direction](./monitor-viewer.md#2-setup-monitor-direction).

Do it once per rig. It is the first thing to re-check when patterns appear on
the wrong screen.

---

## 2. Concentric pattern

<Shot
  id="fig-2"
  number="2"
  file="pattern-generator-concentric.png"
  caption="Concentric panel: resolution, preview, monitor target and the positive/negative controls."
  what="Crop to the concentric section — resolution fields, the preview label, monitor combo, Update, Save Image, CrossLine, and the Positive/Negative Pattern buttons with their colour swatches."
  height={430}
/>

### 2.1 Controls

| Control | Object name | What it does |
|---|---|---|
| **W:** / **H:** | `lineedit_width_concentric` / `lineedit_height_concentric` | Output resolution. Default **1920 × 1920**. A non-integer entry is silently reset to the default, then the pattern re-renders |
| Preview | `label_img_concentric` | Live render, scaled so the longer side is at most 300 px. Never upscaled |
| **CrossLine: On/Off** | `btn_crossline_concentric` | Toggles a crosshair through the pattern centre. Redraws immediately |
| Monitor | `combobox_select_monitor_concentric` | Which screen **Update** targets. Defaults to `TOP` |
| **Update** | `btn_update_to_monitor_concentric` | Renders, saves the PNG, and pushes it to the monitor |
| **Save Image** | `btn_save_image_concentric` | Renders and saves to a file you pick. Default name `pattern_circle.png` |
| **Positive Pattern** | `btn_positive_pattern_concentric` | Paints alternating base colours across all 25 layers — see below |
| **Negative Pattern** | `btn_negative_pattern_concentric` | The same with the two colours swapped |
| Positive / Negative colour | `btn_color_positive_concentric` / `btn_color_negative_concentric` | Colour swatches. Clicking one opens a colour picker |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE BASE COLOUR SWATCHES DO NOT REDRAW ANYTHING</div>
  <div>
    Picking a positive or negative base colour only stores it on the button. Nothing changes on screen until you click <strong>Positive Pattern</strong> or <strong>Negative Pattern</strong>, which is what actually applies it across the layers.
  </div>
</div>

### 2.2 The layer table

25 rows, each: **No.** · **Shape** · **Cx** · **Cy** · **Radius** · **Color**.

<Shot
  id="fig-3"
  number="3"
  file="pattern-generator-concentric-table.png"
  caption="Concentric layer table, first rows filled."
  what="Crop to the 25-row concentric table with a real pattern loaded, so Radius values are non-zero and the colour swatches alternate."
  height={430}
/>

| Column | Widget | Notes |
|---|---|---|
| Shape | `combobox_shape_<n>` | Defaults to `○`. Falls back to `circle` when empty |
| Cx / Cy | `lineedit_cx_<n>` / `lineedit_cy_<n>` | Centre offset for that layer. Default `0` |
| Radius | `lineedit_radius_<n>` | **A radius of `0` or less means the layer is skipped entirely.** This is how you use fewer than 25 rings |
| Color | `btn_color_<n>` | Click to pick. Defaults to black when unreadable |

Any edit — shape, Cx, Cy, radius, colour — re-renders the pattern immediately.

### 2.3 Positive and negative

Clicking **Positive Pattern** paints the positive base colour on **odd** layers
and the negative base colour on **even** layers. **Negative Pattern** swaps
them. That inversion is the whole point: the positive and negative captures
differ, and the calibration is built from where their intensity profiles cross.

---

## 3. Stripline pattern

<Shot
  id="fig-4"
  number="4"
  file="pattern-generator-stripline.png"
  caption="Stripline panel and its 50-row interval table."
  what="Crop to the stripline section with a pattern loaded — resolution, preview, monitor combo showing SIDE, and enough of the 50-row table to show the No./interval/Color columns."
  height={470}
/>

Same controls as concentric, with three differences:

| | Concentric | Stripline |
|---|---|---|
| Default resolution | 1920 × 1920 | **3840 × 2160** (`lineedit_width_stripeline` / `_height_stripeline`) |
| Default monitor | `TOP` | **`SIDE`** |
| Per-layer data | shape, Cx, Cy, radius, colour | **interval** + colour only |

The table is 50 rows of `lineedit_interval_stripeline_<n>` and
`btn_color_stripeline_<n>`. As with radius, **an interval of `0` or less skips
that layer.** Default layer colour is white.

---

## 4. Chessboard pattern

Not part of a calibration run — this is the target for
[3D Verification](../verification/3d-verification.md).

<Shot
  id="fig-5"
  number="5"
  file="pattern-generator-chessboard.png"
  caption="Chessboard panel."
  what="Crop to the chessboard section with a board rendered in the preview."
  height={400}
/>

| Control | Object name | What it does |
|---|---|---|
| Resolution W / H | `lineedit_chess_res_w` / `_res_h` | Output size in pixels |
| Square size (mm) | `lineedit_chess_square_mm` | Physical square size you want on the screen |
| Pixel size | `combobox_chess_pixel_size` | **mm per screen pixel** for the monitor you are driving |
| Foreground | `btn_color_chess_fg` | Square colour. Defaults to black |
| Background | `btn_color_chess_bg` | Defaults to grey (180, 180, 180) |
| **Generate** | `btn_generate_chessboard` | Re-renders |
| **Save Image** | `btn_save_image_chessboard` | Default name `pattern_chessboard.png` |
| **Update** | `btn_update_to_monitor_chessboard` | Pushes to `combobox_select_monitor_chessboard` |

The square size in pixels is `round(square_mm / pixel_size_mm)`. If the
resolution is below 1 in either direction, or that division yields fewer than 1
pixel, **the preview goes blank and nothing is generated** — no error message.
That empty preview is the symptom of a wrong pixel size.

Editing any field, or picking either colour, re-renders straight away.

---

## 5. File menu — JSON import and export

<Shot
  id="fig-6"
  number="6"
  file="pattern-generator-file-menu.png"
  caption="The File menu, open."
  what="Click File and capture the open menu showing all four import/export entries."
  height={220}
/>

Four actions: import and export, for concentric and for stripline separately.

Export writes to `pattern_json/` by default. The schema is compatible with the
original Python tool:

```json
{
  "type": "concentric",
  "width": 1920,
  "height": 1920,
  "crossline": false,
  "pos_neg_color": false,
  "positive color": [255, 255, 255],
  "negative color": [0, 0, 0],
  "1": { "shape": "circle", "radius": 120, "cx": 0, "cy": 0, "color": [0, 0, 0] },
  "2": { "...": "..." }
}
```

Stripline layers carry `{ "interval": …, "color": [r,g,b] }` instead.

Rig profiles that ship with the system — for example
`Server/v2.0.0/yuanman_TOP_concentric_…json` and
`yuanman_SIDE_stripeline_…json` — are exactly this format, so importing one
reproduces a known-good pattern.

---

## 6. What Update writes to disk

Every render **persists files immediately**, before you press anything:

| File | Written when |
|---|---|
| `image_cali/_tmp_pattern_circle.png` | every render |
| `image_cali/pattern_circle_<dir>.png` | every render, for the currently selected direction |
| four `pattern_circle_{n,w,s,e}.png` | when the direction is `SIDE` |

Those files are what the capture workflow reads. It also means the on-disk
pattern tracks whatever is in the form, not whatever you last pushed to a screen.

---

## 7. What the Main Window does with this

### Pos Shot / Neg Shot

Clicking **Pos Shot** or **Neg Shot** on the main window drives this window
automatically, before the camera fires:

1. The concentric monitor combo is forced to `TOP`, the positive or negative
   colours are applied, and it is pushed.
2. The stripline monitor combo is forced to `SIDE`, the same polarity is applied,
   and it is pushed — fanning out to all four side screens.
3. The main window waits 300 ms, then captures.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ A SHOT OVERWRITES YOUR MONITOR SELECTION</div>
  <div>
    Whatever you had chosen in the two monitor combo boxes is replaced by <code>TOP</code> and <code>SIDE</code>. That is intended — it guarantees a shot always drives the full rig — but it means a manual single-screen test does not survive the next Pos Shot.
  </div>
</div>

### The PCT list

The Calibration Result window's **Update Table** reads **75 PCT values** from
this window: the 25 concentric radii followed by the 50 stripline intervals, in
order, exactly as typed. Empty rows come through as empty strings.

This is also where [PCT Recommend](./cali-result/pct-recommend.md) sends you
back to — you paste its proposed series into these radius and interval fields.

---

## 8. Update to Monitor, and why it goes through Monitor Viewer

**Update** does not talk to the monitor node directly. It emits the rendered PNG,
the main window catches it, and hands it to the **Monitor Viewer**, which sends
`show_pattern` **and** `set_brightness` together.

> Sending `show_pattern` on its own leaves the screen dark or green. The
> brightness call is not optional, and routing through Monitor Viewer is what
> guarantees both are sent.

---

## Recommended workflow

1. **Setup Monitor Direction** — once per rig.
2. Import a known-good JSON, or fill the layer table by hand.
3. Set the resolution to match the physical screens.
4. Click **Positive Pattern**, check the preview.
5. Select `TOP` (concentric) and `SIDE` (stripline), click **Update** on each.
6. Check the real screens, and set brightness in
   [Monitor Viewer](./monitor-viewer.md).
7. Export the JSON so the run is reproducible.
8. Go to the main window and take your shots.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Preview is blank | Every radius (or interval) is `0` or less — no layers to draw. On chessboard, the pixel size makes the square smaller than 1 px |
| Resolution field snaps back to 1920 | What you typed is not an integer |
| Pattern lands on the wrong screen | Direction mapping not done, or done before the screens were rearranged. Re-run **Setup Monitor Direction** |
| Screen goes dark or green after Update | Brightness was not applied — see [§8](#8-update-to-monitor-and-why-it-goes-through-monitor-viewer). Set it in Monitor Viewer |
| Colour picked but nothing changed | You picked a **base** colour. Click Positive Pattern or Negative Pattern to apply it |
| Only one side screen updated | Direction was set to a single compass point, not `SIDE` |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `pattern-generator-overview.png` | Whole window with a rendered concentric pattern |
| 2 | `pattern-generator-concentric.png` | Concentric controls and preview |
| 3 | `pattern-generator-concentric-table.png` | The 25-row layer table, filled |
| 4 | `pattern-generator-stripline.png` | Stripline controls and interval table |
| 5 | `pattern-generator-chessboard.png` | Chessboard panel with a board rendered |
| 6 | `pattern-generator-file-menu.png` | File menu open, four JSON entries |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/pattern_generator.ui` |
| Behaviour | `cpp/src/controllers/controller_pattern_generator.{h,cpp}` |
| Rendering | `cpp/src/core/pattern/moil_pattern_gennerator.cpp` |
| Example configs | `Server/v2.0.0/yuanman_*.json`, `cpp/config/` |
