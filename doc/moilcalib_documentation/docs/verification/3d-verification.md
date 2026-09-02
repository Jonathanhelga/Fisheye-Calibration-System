---
id: 3d-verification
slug: /verification/3d-verification
title: 3D Verification
---

# 3D Verification

The final check: reconstruct a known 3D target from two calibrated cameras and
see whether the numbers agree with reality.

Opened from **Validation → 3D Verification** on the main window, or by the
`measure3d` HELP key.

<Shot
  id="fig-1"
  number="1"
  file="3d-verification-overview.png"
  caption="The 3D measurement window with both cameras loaded and detections drawn."
  what="Load a left and right fisheye image plus their parameter JSONs, run detection on at least the center direction for both, then capture the whole window on the Camera left tab."
  height={560}
/>

<div className="custom-note custom-warning">
  <div className="custom-note-title">🚧 THIS WINDOW IS STILL CHANGING</div>
  <div>
    <code>PlaneFit3dViz</code>, <code>Point3dGlView</code> and the measurement controller were all being modified when this page was written, and <code>CbDetect</code> and <code>PanelCornerRecovery</code> are new modules. Expect small differences from the build in front of you, and re-check this page before v2.0 is frozen.
  </div>
</div>

---

## The six tabs

| Tab | What it holds |
|---|---|
| **Camera left** | The left fisheye, its five anypoint views, and per-direction detection |
| **Camera Right** | The same for the right camera |
| **Result 2 Camera** | Triangulated angles, distances and reprojection errors |
| **3D View** | An OpenGL point cloud with both camera positions and their FOV rings |
| **ORI_DET** | The manual click-the-corners method |
| **3D_ORI_DET** | The 3D result from ORI_DET corners |

---

## The five directions

Every camera is unwrapped into **five anypoint views**, one per panel of the
target:

| Direction | Pitch | Yaw | Zoom | Where it looks |
|---|---:|---:|---:|---|
| `center` | 0 | 0 | 4 | Straight ahead |
| `north` | −90 | 0 | 4 | Up |
| `south` | 90 | 0 | 4 | Down |
| `west` | 0 | −90 | 4 | Left |
| `east` | 0 | 90 | 4 | Right |

Those are the defaults loaded into the spin boxes. **Alpha is yaw and beta is
pitch**, which is the reverse of what the labels suggest — worth knowing before
you adjust them by hand.

Changing any alpha, beta or zoom spin box **re-renders that one anypoint view
immediately**, provided an image and parameters are loaded.

---

## Method 1 — Anypoint

The automatic route. Detect a chessboard in each unwrapped view, then triangulate.

### 1. Load the images and parameters

| Button | Object name | What it does |
|---|---|---|
| **Left image** | `btn_left_image` | Opens a fisheye image. **Asks for the parameter JSON too if none is loaded yet** |
| **Right image** | `btn_right_image` | Same for the right camera |
| **Left parameter** | `btn_left_parameter` | Swaps the left camera's JSON |
| **Right parameter** | `btn_right_parameter` | Swaps the right camera's JSON |

<div className="custom-note">
  <div className="custom-note-title">📌 SWAPPING AN IMAGE OR A JSON RE-RUNS EVERYTHING AUTOMATICALLY</div>
  <div>
    Old detections were computed against the previous image and parameters, so they no longer apply. The window <strong>remembers which directions you had detected</strong>, wipes the stale state, rebuilds all five anypoint views, re-detects those same directions, and recalculates. You do not press anything.
    <br /><br />
    This is what makes swapping a parameter JSON actually change the result — a fix worth knowing about, because the old behaviour looked like the swap had been ignored.
  </div>
</div>

Neither button responds while detection is running.

### 2. The reorder combo boxes

Each camera-and-direction pair has a combo box controlling how the detected
chessboard grid is oriented:

`default` · `flip_horizontal` · `flip_vertical` · `rotate_90` · `rotate_180` ·
`rotate_270` · `adaptive`

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ THE REORDER SETTING IS THE USUAL CAUSE OF A NONSENSE 3D RESULT</div>
  <div>
    The detector finds the corners but not which one is "first". If left and right disagree, triangulation pairs the wrong corners and produces a plausible-looking but wrong cloud. When the geometry comes out mirrored or twisted, <strong>check these before anything else.</strong>
  </div>
</div>

### 3. Detect

| Button | What it does |
|---|---|
| `pushButton_Detect_<cam>_<dir>` | Runs chessboard detection on that one view |
| `pushButton_Detect_<cam>_result` | Overlays every detection for that camera back onto its fisheye |

The overlay is the check that the five views really do line up on the physical
target.

### 4. Camera positions

Six spin boxes — `spinBox_x_left`, `spinBox_y_left`, `spinBox_z_left` and the
same for `right` — carrying each camera's position in the rig.

> **These are the baseline.** Triangulation is only as good as the distance
> between the two cameras, so an error here scales straight through into every
> measured distance.

### 5. Start Calculation

`pushButton_start_calculated`.

It requires:

- the **2-camera** radio button selected,
- detections with valid alpha/beta on **both** cameras,
- triangulation to produce at least one point.

**Failing any of those, it returns silently.** No message. An unresponsive Start
Calculation means one of the three above, not a crash.

What it then does:

1. Triangulates every detected point pair into 3D.
2. **Fills the 3D View tab** — but does not switch to it, so the interface does
   not jump. Open the tab when you want it.
3. Sets the FOV rings from the loaded parameter JSONs, not from a typed guess.
4. Fits planes and computes the angles between them, mean distances, per-plane
   distances and depth from origin.
5. Reprojects the 3D points back through both cameras and reports mean and RMS
   error.

---

## Method 2 — ORI_DET

The manual route: click the four corners of each panel yourself, on the
**original fisheye** rather than an unwrapped view. Use it when automatic
detection cannot find the board.

<Shot
  id="fig-2"
  number="2"
  file="3d-verification-ori-det.png"
  caption="ORI_DET mid-sequence — the active panel highlighted and the prompt naming the next corner."
  what="Open the ORI_DET tab and click a couple of corners. Capture it so the ▶ LEFT IMAGE header, the plane compass and the status line are all readable."
  height={520}
/>

### The click order is fixed

**Five planes, in this order:**

```
center → east → west → south → north
```

**Four corners per plane, in this order:**

```
top-left → top-right → bottom-right → bottom-left
```

**Left camera first, then right**, for each plane.

That is 5 × 4 × 2 = **40 clicks**.

### The window walks you through it

| Cue | What it tells you |
|---|---|
| **`▶ LEFT IMAGE — 2/5 planes`** | Which camera to click, on a red background. The inactive side is greyed |
| The status line | The plane, **where it is in the picture**, which corner is next, and the count: *"Plane "east" (right of the picture) — click 4 corners on LEFT (top-left, top-right, bottom-right, bottom-left). Next: top-right (2/4)"* |
| The compass | Highlights the plane you are on, drawn on the image at the same spot |

When all 40 are in: *"All 5 planes captured on both cameras. Press "Start
Calculation"."*

> The status line names the plane's **position in the frame**, not just its
> compass name, because the panel positions are fixed by the optics. You should
> never have to guess which physical panel `east` refers to.

### Recovering a bad plane

A plane whose clicks went wrong can be deleted and recovered without restarting
the whole 40-click sequence — `PanelCornerRecovery` is what backs that.

---

## Reading the results

### Result 2 Camera

| Quantity | Meaning |
|---|---|
| **Angles** | Between the fitted planes. A physical target with right angles should read close to 90° |
| **Mean distances** | Between reconstructed points |
| **Plane distances** | Mean distance from points to their fitted plane — **the flatness measure** |
| **Depth from origin** | Per direction |
| **Reprojection mean and RMS** | The 3D points projected back through both cameras, compared with where they were actually detected |

<div className="custom-note">
  <div className="custom-note-title">📌 REPROJECTION ERROR IS THE HEADLINE NUMBER</div>
  <div>
    It is the one figure that answers "is this calibration good?". Everything else describes the target; this describes the <em>camera model</em>. A low reprojection error with sensible plane angles means the parameters are trustworthy.
  </div>
</div>

### 3D View

An OpenGL point cloud with both camera positions and their FOV rings. It fills
automatically after Start Calculation.

> **In Docker, add `--device /dev/dri`** for GPU rendering. Without it you get
> software GL, which works but is slow to rotate.

### Image viewing

Every image label is clickable: click one to open it full-screen, with zoom and
fit-to-view.

---

## Which method to use

| | Anypoint | ORI_DET |
|---|---|---|
| Effort | Load, click Detect | 40 clicks |
| Works when the board is hard to see | ✗ | ✓ |
| Sensitive to reorder settings | ✓ | ✗ |
| Distortion at the moment of detection | Removed — detection runs on the unwrapped view | Present — you click the raw fisheye |

Use Anypoint first. Fall back to ORI_DET when detection fails, and to
cross-check a result you do not trust — two independent routes to the same number
is a stronger check than either alone.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| **Start Calculation does nothing** | The 2-camera radio is unselected, one camera has no valid detections, or triangulation produced no points. It returns silently |
| Load buttons unresponsive | Detection is running. Wait for it |
| Result did not change after swapping a JSON | It should — the swap re-detects and recalculates. If not, the previous detections had no valid alpha/beta to re-run |
| 3D cloud mirrored or twisted | A reorder combo disagrees between left and right |
| Plane angles far from 90° | Wrong reorder, wrong camera positions, or a genuinely misaligned target |
| Reprojection error large | The camera parameters are wrong. Go back to [Setup Center](./setup-center.md) and the parameter tab |
| 3D View empty | Start Calculation has not produced points yet — it fills the tab, it does not switch to it |
| 3D View very slow | Software GL. In Docker, add `--device /dev/dri` |
| Anypoint views blank | No image, no parameter JSON, or a zoom that pushes everything out of frame |
| ORI_DET clicks land in the wrong plane | The sequence is fixed: center, east, west, south, north — left camera then right. Follow the ▶ header |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `3d-verification-overview.png` | Whole window, both cameras loaded, detections drawn |
| 2 | `3d-verification-ori-det.png` | ORI_DET mid-sequence with the prompt and compass |

Four more that would carry this page:

| File | What to capture |
|---|---|
| `3d-verification-anypoint-detect.png` | One anypoint view with the detected chessboard corners drawn |
| `3d-verification-overlay-fisheye.png` | All five detections overlaid back on the fisheye |
| `3d-verification-result-2camera.png` | The Result 2 Camera tab with angles, distances and reprojection error |
| `3d-verification-3d-view.png` | The 3D View point cloud with both cameras and their FOV rings |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/UI3d_measurement.ui`, `cpp/src/views/widgets/UI3d_measurement.h` |
| Behaviour | `cpp/src/controllers/controller_auto_3d_measurement.{h,cpp}` |
| Triangulation, plane fitting | `cpp/src/core/measure3d/` — `moil_3d_algorithm`, `Measure3dTypes`, `AnypointChessboard`, `CbDetect`, `PanelCornerRecovery`, `PlaneFit3dViz` |
| OpenGL view | `cpp/src/views/Point3dGlView.{h,cpp}` |
| Sample output | `cpp/image_cali/output_3D/` |
| Tests | the `Measure3d` and `Moil3d` test binaries |
