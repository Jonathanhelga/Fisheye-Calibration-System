# Pattern Generator

Reference notes on the Pattern Generator feature, based on the reference implementation in `moil-fisheye-calibration-system`.
This is the source material for our own Pattern Generator, before we design the QML UI.

## What it is

The Pattern Generator is where calibration pattern pictures get created, previewed, colored, saved, and sent to the monitor screens, before the camera takes its calibration photos.

It builds three kinds of pattern:

1. **Concentric** — rings/squares in circular layers. Usually shown on the TOP screen.
2. **Stripline** — parallel horizontal/vertical stripes. Usually shown on the side screens (N/W/S/E).
3. **Chessboard** — a black-and-white checkerboard, drawn outward from the screen center.

The concentric radii and stripline intervals are not just drawn on screen.
They are also reused downstream as calibration reference data (see PCT section below).
Get them right here and the calibration math downstream will be correct.

## Window layout

One window, three side-by-side panels, plus a File menu.
Each panel has its own preview, its own controls, and its own table of layer values.

| Area | What it's for |
|---|---|
| File menu | Import / export pattern settings as JSON (concentric and stripline separately), and save the current preview image. |
| Concentric panel | Resolution, preview, colors, crossline, direction, Update, and a 25-layer radius table. |
| Stripline panel | Resolution, preview, colors, direction, Update, and a 50-layer height/interval table. |
| Chessboard panel | Resolution, square size (mm), pixel size, two colors, direction, and Generate / Save / Update. |

## File menu

Imports and exports pattern settings so a configuration can be reused instead of retyped.

| Action | What it does |
|---|---|
| Import JSON Concentric | Loads a concentric `.json` file: resolution, colors, crossline, and all layer radii, then refreshes the preview. |
| Export JSON Concentric | Saves the current concentric settings into a `.json` file. |
| Import JSON Stripline | Loads a stripline `.json` file (resolution, colors, layer intervals) and refreshes the preview. |
| Export JSON Stripline | Saves the current stripline settings into a `.json` file. |
| Save Image / Save Image As | Saves the current preview as a PNG. |

Concentric JSON and stripline JSON are different pattern types.
Import a concentric file only through Import JSON Concentric, and a stripline file only through Import JSON Stripline.

## Concentric pattern

A set of rings (or squares) growing outward from the center. Usually shown on the TOP screen.

### Controls

| Control | What it does |
|---|---|
| Resolution H / W | Image height and width in pixels (commonly 1920 x 1920). Invalid text resets to a default. |
| Preview | Shows the pattern rendered from the current settings. |
| Save Image | Saves the current concentric image as a PNG. |
| CrossLine (On/Off) | Draws a center crosshair over the pattern to help check alignment. |
| Direction combobox | Which monitor screen this pattern belongs to (TOP / N / W / S / E). |
| Update | Re-renders the pattern and sends it to the selected monitor direction. |
| Positive / Negative color | Opens a color picker to choose the two pattern colors. |
| Positive Pattern / Negative Pattern | Fills the layers with alternating colors. Negative Pattern swaps the two colors first (inverted look). |

### Concentric table (25 layers)

Each row is one ring/layer. Up to 25 layers.

| Column | Field | Meaning |
|---|---|---|
| No. | layer index | The layer number (1-25). |
| Shape | Circle / Square | The drawing shape for that layer. Circle is the usual choice. |
| Radius | radius value | The size of the ring. This value is used as concentric PCT data. |
| Color | color button | Sets that layer's color individually. |
| Cx / Cy | center offset | Horizontal / vertical center for the layer. Leave 0 for the default center. |

Use Positive Pattern / Negative Pattern to color all layers at once with alternating black/white.
Use the per-row Color button only when one layer needs to be customized.

## Stripline pattern

A set of parallel stripes. Usually shown on the side screens (N, W, S, E).

### Controls

| Control | What it does |
|---|---|
| Resolution H / W | Stripe image height and width in pixels (e.g. a wide side display). |
| Preview | Shows the rendered stripe pattern. |
| Save Image | Saves the current stripline image as a PNG. |
| CrossLine (On/Off) | Draws a center crosshair over the pattern. |
| Direction combobox | Which monitor screen this pattern belongs to. |
| Update | Re-renders and sends the pattern to the selected direction. |
| Positive / Negative color + Positive / Negative Pattern | Same idea as the concentric panel: pick two colors, then apply them in alternating order (Negative swaps them). |

### Stripline table (50 layers)

Each row is one stripe. Up to 50 layers.

| Column | Field | Meaning |
|---|---|---|
| No. | layer index | The stripe number (1-50). |
| Height | height / interval | The size/spacing of the stripe. This value is used as stripline PCT data. |
| Color | color button | Sets that stripe's color. |

The column is labeled "Height" in the UI, but internally it is the stripe interval.
Treat "Height" and "interval" as the same stripline value.

## Chessboard pattern

A black-and-white checkerboard tiled outward from the center of the screen (a square boundary passes through the center).
This is an extra pattern, separate from the 75 PCT values.

| Control | What it does |
|---|---|
| Resolution H / W | Board image size in pixels (e.g. 1080 x 1920). |
| Square (mm) | The physical size of one square, in millimeters (default 45). |
| Pixel size (mm) | How many millimeters one screen pixel is, chosen from a dropdown: 0.2478, 0.155, or 0.293. This is the monitor's pixel pitch. |
| Square color / Background | The two checker colors (default black + light grey 180,180,180). |
| Direction combobox | Which monitor screen the board belongs to. |
| Generate | Renders the board into the preview. |
| Save Image / Update | Saves the board as PNG / sends it to the selected monitor direction. |

How the square size becomes pixels:

```
square_px = round( Square(mm) / Pixel size(mm) )
```

Example: 45 mm / 0.2478 mm/px is about 182 px per square. The board is then tiled from the screen center so the pattern is symmetric.

## Typical workflow, pattern to calibration photo

1. In the Pattern Generator, set the resolutions and fill the layer values.
2. Choose the two colors and click Positive Pattern (or Negative Pattern) to color the layers.
3. Pick the Direction for each pattern (e.g. concentric to TOP, stripline to the side screens).
4. Click Update so the pattern is sent to the correct screen and saved.
5. Open the Monitor Viewer to confirm each screen shows its pattern.
6. Go to the main window and capture the positive and negative photos.

The positive photo (pattern on) and the negative photo (colors inverted) are used together by the calibration process to find the intersection points (ICT) that produce the calibration data.

## How the values feed calibration (PCT)

The Pattern Generator is not only a drawing tool. Its layer values are reused by the Cali Result window as the PCT list.

| Pattern source | Data used |
|---|---|
| Concentric table | Radius values from layers 1-25. |
| Stripline table | Interval values from layers 1-50. |

Together they form the 75 PCT values:

```
25 concentric radius values
+ 50 stripline interval values
-----------------------------
= 75 PCT values
```

These 75 values become the PCT column in the calibration table and drive PCT calibration, alpha, ZFL, overlap, and aggregation.

If a radius or a stripline height is wrong, the Cali Result table receives wrong PCT data, which corrupts alpha, ZFL, overlap, and aggregation. Values should be checked before capturing.

## Where images are saved

When Update (or Save Image) is clicked, the rendered pattern is written to the calibration image folder, named by the selected direction, for example:

```
image_cali/pattern_circle_top.png
image_cali/pattern_circle_side.png
```

The Monitor Viewer and the capture workflow read these files, so the Direction and pattern type should always be confirmed before updating.

## Quick reference

| Action | Where |
|---|---|
| Import / export concentric JSON | File menu |
| Import / export stripline JSON | File menu |
| Change concentric size | Concentric Resolution H / W |
| Change a ring size | Concentric table, Radius |
| Change stripe size | Stripline table, Height |
| Alternating colors | Positive Pattern / Negative Pattern |
| Chessboard square size | Chessboard Square (mm) + Pixel size (mm) |
| Send a pattern to a screen | Pick Direction, then Update |
| Save a pattern picture | Save Image |

## Troubleshooting (reference app)

| Problem | Likely cause | What to do |
|---|---|---|
| Preview does not change | The edited field was not confirmed. | Press Enter or click outside the field. |
| Imported JSON does not load | Wrong JSON type or invalid file. | Use the matching import action for that pattern type. |
| Colors look swapped | Positive/negative were reversed. | Click the other pattern button, or set colors manually. |
| Chessboard squares wrong size | Wrong Pixel size (mm) for the monitor. | Pick the pixel pitch that matches the calibration monitor. |
| Monitor does not update | Direction not sent, or monitor not connected. | Pick the direction and click Update again. |
| Calibration PCT values wrong | Radius / stripline values incorrect. | Re-check every concentric radius and stripline height before updating. |

## Reference implementation architecture (moil-fisheye-calibration-system)

For context when designing our own version:

- `ControllerPatternGenerator` (`cpp/src/controllers/controller_pattern_generator.h/.cpp`) is the Qt window controller. It wires up the three panels, handles per-layer editing, positive/negative color application, dimension edits, crossline toggle, live preview, JSON import/export, image save, and "update to monitor."
- `MoilCaliPatternGenerator` (`cpp/src/core/pattern/moil_pattern_gennerator.h/.cpp`) is the pattern data/config model, ported from a Python original. It keeps the pattern as a JSON object and exposes setters for every field (dimensions, crossline, colors, per-layer shape/radius/color/cx/cy for concentric, per-layer interval/color for stripline, chessboard pixel size and grid mm).
- `PatternGen` namespace is the actual rendering engine: `renderConcentric`, `renderStripeline`, `renderChessboard`, operating on `ConcentricLayer` / `StripelineLayer` structs, producing `cv::Mat` images.
- "Update to Monitor" writes `pattern_circle_<direction>.png` into the calibration image folder and emits a signal the main window forwards to the monitor client.
- Each pattern type keeps its own positive/negative color state independently.

## Functionality improvement ideas (calibrator workflow)

These are proposed changes to how the window *works*, not how it looks.
The goal is to reduce manual entry and catch mistakes before they reach the camera capture step, since the concentric radii and stripline intervals feed the 75 PCT values used by calibration.

### What the row values actually mean

This came out of tracing `CaliCompute::updatePctCal` and the rendering code (`renderConcentric` / `renderStripeline` in `moil_pattern_gennerator.cpp`), plus direct confirmation from the calibrator who built the reference app.

Each row is not an absolute position from the center.
It is a **step**, the pixel distance from the previous ring or stripe to this one.

For concentric, row 1 is the innermost ring's radius.
Row 2 is how many more pixels the second ring sits beyond the first, and so on outward.
The renderer confirms this directly: it sums all 25 row values, draws the outermost circle first using that full sum, then subtracts each row's value moving inward, ending with row 1 drawn last as the innermost ring.

For stripline, row 1 is the pixel thickness of the first stripe from the reference edge, row 2 is the thickness of the next stripe after it, and so on.

The software turns these steps into the real physical distance used in the alpha and ZFL trigonometry by running a cumulative sum and multiplying by the monitor's pixel pitch, set in the Cali Result window (0.2478 mm per pixel for the top screen, 0.155 mm per pixel for the side screens, by default):

```
pct_cal(row N) = (row 1 + row 2 + ... + row N) * pixel_pitch_mm_per_px
```

Because every step is a positive pixel value, the cumulative radius is always increasing by construction.
There is no way to type a sequence that goes backward, so a "rings must increase" validation is unnecessary, the math already guarantees it.

### Why the calibrator uses semi-random values, not a fixed sequence

Confirmed directly by the calibrator who built the reference app: each row's value is picked by hand, bounded between 10 and 100 (in raw pixel-step units, regardless of resolution), then hand-tuned further after checking a real capture.
It looks random from outside, but it is a deliberate compensation for how a fisheye lens works, not noise.

The step value controls spacing *on the screen*.
What actually matters for calibration is spacing *in the photograph*, after the fisheye lens has bent the light.
A fisheye lens does not compress space evenly, some regions of the field of view get squeezed together in the photo far more than others.
So identical on-screen steps do not produce identical spacing in the photo, rings that are evenly spaced on the display can land bunched together in one part of the photo and spread too far apart in another.

That is why "just use the same number for every row" does not work: it optimizes spacing on the screen, but the screen is not what gets measured, the photo is.
The calibrator instead varies each step, guided by experience with a given lens, so that the rings land reasonably well spread out across the actual captured image, giving the calibration curve fit good coverage everywhere instead of dense samples in one region and gaps in another.
The 10 to 100 floor and ceiling exist so that, whatever compensation she is doing, no ring ends up too close to detect reliably in the photo, or so far out that it burns through the available rows without covering the field of view.

This process is for finding a good pattern for a lens or rig that does not already have one.
Once a set of values works well for a given lens family, it should be saved and reused rather than re-picked from scratch on every calibration run, see the saved presets idea below.

### Ideas

1. **Randomize-within-range instead of typing 75 numbers by hand.**
Every one of the 25 steps and 50 intervals is currently typed in individually, starting from 0.
A button that fills all rows with random values inside an adjustable range (default 10 to 100) would match how the calibrator already works, with every cell still editable afterward for the hand-tuning pass.
This is different from a plain arithmetic sequence, an arithmetic fill would produce even spacing on the screen, which is not what is wanted, see above.

2. **Bounds check instead of a monotonic check.**
Since the cumulative radius always increases by construction, checking for "must keep increasing" is redundant.
The check that actually matters is the range the calibrator already uses by hand: flag any row outside roughly 10 to 100, and warn if the cumulative total for a pattern would run past the edge of the resolution.

3. **Live preview instead of "press Enter to see it."**
The reference app's own troubleshooting notes flag this as a real complaint (see Troubleshooting above).
Re-rendering as the calibrator types, debounced, instead of requiring focus-out removes a step they currently have to remember.

4. **One button to push all three patterns to their screens.**
Today the calibrator sets Direction and clicks Update three separate times, once per panel.
The reference controller already has a `pushCalibrationPatterns(bool positive)` hook meant for exactly this.
It is currently only reachable from the main window's capture flow; surfacing it directly on the Pattern Generator window would save the repeated manual step.

5. **Saved presets, not just raw JSON import/export.**
This matters more than it first looked like.
The random-plus-hand-tune process is how a good pattern gets *found* for a lens or rig that has never been calibrated before, it is not meant to be repeated on every routine recalibration of the same lens and rig.
A dropdown of named, known-good configs (for example "Rig A", "Lens Model X") beats hunting for the right file on disk, and is the normal path for repeat calibration work, not raw JSON import/export.

6. **(Possible follow-up, bigger scope) Feed measured photo spacing back into the Pattern Generator.**
The Cali Result window already computes ICT, the actual spacing between rings in the captured photo, per direction.
Surfacing "these two rings landed only 6px apart in the last capture" back on the Pattern Generator side would turn the calibrator's visual eyeballing into something the software points out directly.
This needs a capture to exist first, so it cannot happen at pattern-design time, only as a post-capture feedback loop. Flagged here, not yet scoped.

Priority: items 1 and 2 together address the failure mode the reference docs call out as most damaging, bad PCT values silently corrupting alpha, ZFL, overlap, and aggregation downstream, now updated to match how the values are actually meant to be chosen.

## Status

This document describes the reference app's Pattern Generator only, for use as a functional basis.
Our own QML implementation has not been started yet.
