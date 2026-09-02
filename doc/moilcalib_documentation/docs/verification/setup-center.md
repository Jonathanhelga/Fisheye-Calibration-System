---
id: setup-center
slug: /verification/setup-center
title: Setup Center
---

# Setup Center

Verifies and corrects the camera centre point — `iCx`, `iCy` — in a
camera-parameter JSON.

**If the centre is wrong, every result computed from those parameters is wrong.**
That is why this check has its own tool, and why the corrected centre can be
written straight back into the parameter file.

Opened from **Validation → Setup Center** on the main window, or by the
`center_setup` HELP key.

<Shot
  id="fig-1"
  number="1"
  file="setup-center-overview.png"
  caption="Setup Center: original fisheye, panorama preview and anypoint view, with the control column on the right."
  what="Open Setup Center with a fisheye image and a parameter JSON loaded, and both Panorama and Anypoint toggled on, so all three views have content. Capture the whole window."
  height={560}
/>

---

## How it opens

Clicking **Setup Center** shows the window immediately, then loads the **raw
positive capture** — with no ROI or edge overlays — a moment later. That split is
deliberate: the anypoint and panorama maps are expensive to build, and doing it
after the window paints keeps the window from appearing frozen.

The image path reads **`(from host app)`** when it arrived this way.

---

## 1. Media Source

| Control | Object name | What it does |
|---|---|---|
| Path field | `imagePathEdit` | Shows the current image, or `(from host app)` |
| **Open Image…** | `btnOpenImage` | Load a different fisheye image from disk |

---

## 2. Camera Parameter

| Control | Object name | What it does |
|---|---|---|
| Path field | `paramPathEdit` | The JSON currently loaded |
| **Open Parameter JSON…** | `btnOpenParam` | Pick a `.json` |
| Resolution | `resLabel` | `Resolution: 1920 × 1920 · fov 220°` once valid |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ NOTHING RENDERS WITHOUT A PARAMETER FILE</div>
  <div>
    The three views are built from the camera model, not from the image alone. With no JSON loaded, opening an image does nothing visible. Load the parameter file first.
  </div>
</div>

### Two accepted file shapes

| Shape | How it is detected | What happens |
|---|---|---|
| **Single camera** | The object has a `cameraName` key | Parameters are read from the top level |
| **Multi-camera** | No `cameraName` key | **The first entry whose value is an object is used** |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ A MULTI-CAMERA FILE ALWAYS USES THE FIRST ENTRY</div>
  <div>
    There is no camera picker in this window. If your JSON holds several cameras, only the first is loaded — and <strong>Save Center writes back to that same first entry</strong>. To work on a different camera, split it into its own file.
  </div>
</div>

Failure messages: *"Could not open: …"*, *"Not a valid JSON object."*, or
*"No camera parameters found in this file."*

### Working resolution

The file is loaded twice. First at native resolution to learn the calibration
size, then — if the longer side exceeds **1400 px** — again at a reduced scale so
that every centre nudge re-renders quickly.

**All three views, and the `Icx` / `Icy` spin boxes, are in that working
resolution.** The conversion back to native happens only on save. You never see
the ratio, and you do not need to: the numbers in the file stay native.

---

## 3. Center Point

| Control | Object name | Notes |
|---|---|---|
| **Icx:** | `spinIcx` | Range `0 … working width − 1`. Seeded from the file |
| **Icy:** | `spinIcy` | Range `0 … working height − 1` |
| **Save Center to Parameter File** | `btnSaveCenter` | Writes back — see [§5](#5-saving) |

### Three ways to move the centre

1. **Type into the spin boxes**, or use their arrows.
2. **Click the Original Fisheye view.** The click is mapped back through the
   display scale and the letterbox offset; clicks outside the image are ignored.
3. **Let it seed from the file** and leave it alone if it is already right.

Every change **re-renders all three views**, through a short debounce timer so
that holding a spin-box arrow does not queue up a hundred rebuilds.

---

## 4. Verification Views

Three views, and each one tells you something different about the same centre.

### 4.1 Original Fisheye

The image itself, with **guide rings** drawn on it when enabled.

| Control | Object name | Notes |
|---|---|---|
| **Guide α (°):** | `guideCheck` | Draws rings at the listed field angles |
| Angle list | `guideAlphasEdit` | Comma-separated. Default `50, 70, 90, 110`. Non-numeric and non-positive entries are dropped silently |

> **This is the primary check.** If the centre is right, the guide rings are
> concentric with the fisheye circle. If it is wrong, they sit off to one side —
> and that is far easier to see than a wrong number is to notice.

### 4.2 Panorama Preview

| Control | Object name | Notes |
|---|---|---|
| **Panorama** | `btnPanorama` | Toggle. Off shows *"Panorama off"* |
| Slider | `panoSlider` | Moves a **green vertical line** across the panorama |

The panorama is unwrapped to `alphaMax = round(fov / 2)` and then **stretched to
twice its width**, matching the original Python tool. The slider position is a
percentage across the image.

> A correct centre gives a panorama whose horizon is flat. A wrong one makes it
> rise and fall like a wave as you sweep the green line across.

### 4.3 Anypoint View

A rectilinear view aimed anywhere in the fisheye.

| Control | Object name | Notes |
|---|---|---|
| **Anypoint** | `btnAnypoint` | Toggle. Off shows *"Anypoint off"* |
| **Mode:** | `modeCombo` | Mode 1 or Mode 2 — see below |
| First angle | `spinP1` | Relabelled by mode |
| Second angle | `spinP2` | Relabelled by mode |
| **Zoom:** | `spinZoom` | |

**The two modes change both the labels and the valid ranges:**

| | Mode 1 | Mode 2 |
|---|---|---|
| First control | **Alpha (°)**, −110 … 110 | **Pitch (°)**, −90 … 90 |
| Second control | **Beta (°)**, 0 … 360 | **Yaw (°)**, −180 … 180 |

Switching modes **clamps** the current values into the new range rather than
resetting them, so a beta of 300° becomes 180° when you switch to Mode 2.

> Aim the anypoint at a straight edge in the scene. With a correct centre it
> stays straight; with a wrong one it bows.

---

## 5. Saving

**Save Center to Parameter File** converts the spin values from working
resolution back to native, then rewrites the JSON in place — the whole file,
re-indented.

| Case | Result |
|---|---|
| Single-camera file | `iCx` and `iCy` set at the top level |
| Multi-camera file | Set inside the **first** camera entry |
| No JSON loaded | *"Load a camera parameter JSON first."* |
| Camera entry missing | *"Camera entry not found in JSON."* |
| File not writable | *"Could not write: …"* |
| Success | *"Saved iCx=…, iCy=… to: …"* |

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ THE FILE IS OVERWRITTEN IN PLACE, WITH NO BACKUP</div>
  <div>
    There is no undo and no <code>.bak</code>. Copy the parameter JSON before you save if you might want the old centre back.
  </div>
</div>

---

## How to verify a centre

1. Open Setup Center from the main window after a positive shot, so it is seeded
   with a real capture.
2. **Open Parameter JSON…** and load the file you want to check.
3. Tick **Guide α**, leave the default angles.
4. Look at the Original Fisheye view. Are the rings concentric with the image
   circle?
5. If not, click the true centre in the image, then fine-tune with the spin
   boxes.
6. Turn on **Panorama** and sweep the slider. The horizon should stay flat.
7. Turn on **Anypoint**, aim it at a straight edge, and check it does not bow.
8. **Save Center to Parameter File.**

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| All three views blank | No parameter JSON loaded, or no image |
| *"Resolution: — (invalid parameters)"* | The JSON loaded but the camera model is unusable |
| Wrong camera's parameters | Multi-camera file — only the first entry is ever used |
| Guide rings ignore some angles | Non-numeric or non-positive entries are dropped without a message |
| Panorama and anypoint show *"off"* | Their toggles are not pressed |
| Clicking the image does nothing | The click landed outside the image, or no image is loaded |
| Saved values look wrong in the file | They are native-resolution; the spin boxes are working-resolution. That is expected |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `setup-center-overview.png` | Whole window, all three views live |

Three more that would carry this page:

| File | What to capture |
|---|---|
| `setup-center-guides-good.png` | Original Fisheye with guide rings **concentric** — a correct centre |
| `setup-center-guides-bad.png` | The same with the centre deliberately off by 40–50 px, rings clearly off-axis |
| `setup-center-panorama.png` | Panorama with the green slider line, horizon flat |

The good/bad pair is the most valuable capture on this page — it turns "check the
rings are concentric" into something a reader can recognise instantly.

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/center_setup.ui` |
| Behaviour | `cpp/src/controllers/controller_center_setup.cpp` |
| Anypoint / panorama maps | `cpp/src/core/moildev/` |
| Parameter file | `cpp/config/camera_parameters.json` |
