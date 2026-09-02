---
id: 3d-verification
slug: /verification/3d-verification
title: 3D Verification
---

# 3D Verification

The **Auto 3D Measurement** window checks whether a calibrated camera measures the real world correctly.
You feed it a pair of fisheye checkerboard photos, one from each camera.
It finds the checkerboard corners in both images, works out where those corners must sit in 3D space, and projects that 3D point back onto each image.
The distance between the original corner and its projected-back position, in pixels, is the reprojection error.

A low reprojection error means the calibration describes the real lens well.

Open this window from the **Calibration Result / 3D Validation** panel of the [Main Window](/moilcalib_documentation/docs/v1.1/system-overview/main-window#6-calibration-result--3d-validation-panel).

---

## Two Ways to Find the Corners

Before it can triangulate anything, the window needs to know where each checkerboard corner sits in each image.
There are two ways to get that: **Anypoint** and **ORI_DET**.
They only differ in how a corner's position is found — everything after that, including triangulation and the error calculation, is identical code, so the two methods' results can be compared directly.

| | **Anypoint** | **ORI_DET** |
|---|---|---|
| **Corner source** | A flattened, rectilinear remap of the fisheye | Directly on the original curved fisheye image |
| **Your input** | Angles and zoom per direction, or an auto-framing sweep | 4 manual corner clicks per plane, per camera |
| **Corner detection** | Automatic chessboard detection on the undistorted view | Homography-guided corner recovery on the distorted view |
| **Cost** | One remap per camera per direction — 10 remaps for the default 5 directions | No remap, but 10 sets of manual clicks |
| **Main risk** | Remap interpolation error; the board must be framed carefully | A 4-point homography only approximates the curved fisheye |
| **Where it lives** | **Camera Left** / **Camera Right** tabs | **ORI_DET** / **3D_ORI_DET** tabs |

Which method works better is still being tested — see [Why Two Methods Exist](#why-two-methods-exist).
A future version is expected to keep only one.

---

## The Window

| Tab | Purpose |
|---|---|
| **Camera Left** | Anypoint detection for the left camera. |
| **Camera Right** | Anypoint detection for the right camera. |
| **Result 2 Camera** | Triangulated results from the Anypoint pipeline. |
| **3D View** | A 3D visualisation of the reconstructed points. |
| **ORI_DET** | Manual 4-corner picking on the original images. |
| **3D_ORI_DET** | Results of the ORI_DET pipeline. |

Both methods share the same left-hand column of controls:

| Control | Purpose |
|---|---|
| **Left Image / Left parameter** (and the right equivalents) | The fisheye image and its camera-parameter JSON, per camera. |
| **Fisheye preview** | The loaded image with its detected regions. |
| **Camera Position X / Y / Z** | The physical position of that camera. |
| **Preprocesing image — Standart / enhanced** | Whether the image is pre-processed before detection. |
| **Resolution** | Read from the loaded image, for example `3040 × 3040 px`. |
| **3D Projection — 2 Camera / 3 Camera** | How many cameras take part in the reconstruction. |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ Camera Position Sets the Scale</div>
  <div>
    Triangulation works out where two rays cross, so the distance between the cameras decides the scale of every result.
    If <strong>Camera Position X / Y / Z</strong> does not match the physical rig, the reconstructed points come out wrong, even if the reprojection error still looks fine.
  </div>
</div>

---

## Method 1 — Anypoint

The fisheye image is remapped into a flat, rectilinear view first, so the checkerboard detector never has to deal with fisheye distortion directly.

<Figure id="fig-1" number="1" caption={<>Anypoint detection — the five direction views (WEST, EAST, CENTER, NORTH, SOUTH), each with its own Alpha, Beta, and Zoom, a <strong>Detect</strong> button, and a detected-point readout.</>}>

![Anypoint detection on the Camera Right tab](../assets/images/img_99.png)

</Figure>

### How It Works

```text
For each camera × each direction (5 directions × 2 cameras = 10 views):

Remap the fisheye to a rectilinear "anypoint" view
   for the given (pitch/alpha, yaw/beta, zoom)          <- the expensive step
   |
Detect the checkerboard on that undistorted view
   |
Map the detected corners back to the source fisheye
   |
Read (alpha, beta) for each corner from the fisheye pixel
   |
(both cameras done) Triangulate matching (direction, point) pairs
```

Remap tables are cached per camera and direction, so you only pay the cost once, unless you change that direction's angles or zoom.

### Steps

1. On **Camera Left**, load the **Left Image** and **Left parameter** JSON.
   Repeat on **Camera Right**.
2. Enter the **Camera Position X / Y / Z** for each camera.
3. For each direction — **CENTER, NORTH, SOUTH, EAST, WEST** — set **Alpha**, **Beta**, and **Zoom** so the checkerboard sits fully inside that view, then press **Detect**.
4. Check the **Detected Point** readout for each direction.
   A direction that detects nothing contributes no 3D points.
5. Press **Start Calculation**, then read the **RMS** values for left and right.

Instead of tuning every direction by hand, you can use the auto-framing sweep: it tries small zoom, pitch, and yaw changes on a low-resolution preview until it finds the board, then runs full-resolution detection at that framing.
It costs extra compute but saves most of the manual trial and error.

---

## Method 2 — ORI_DET

Nothing is remapped here.
You mark each plane by hand on the original fisheye image, and the checkerboard grid is recovered from those four points.

<Figure id="fig-2" number="2" caption="ORI_DET — the instruction bar names the plane and the exact corner to click next, with the LEFT and RIGHT original fisheye images side by side.">

![ORI_DET manual corner picking on the original images](../assets/images/img_100.png)

</Figure>

### How It Works

```text
For each plane (center/east/west/south/north) x each camera:

Click the 4 outermost-inner corners
   in the order top-left -> top-right -> bottom-right -> bottom-left
   |
Rectify that quad to a homography
   |
Detect the board to discover the grid size and its corners
   |
Assign canonical (row, column) indices
   |
Refine: coarse pass -> RANSAC re-fit of the homography -> fine pass
   |
Pair left and right corners by (row, column)
   |
Read (alpha, beta) straight from the original pixel — no remap round-trip
   |
Triangulate the paired corners
```

The refinement passes exist because a flat 4-point homography only approximates the curved fisheye surface, so a single lookup would not be accurate enough on its own.

### Steps

1. Open the **ORI_DET** tab.
   The instruction bar names the plane and the corner it wants next, for example *"Plane 'east' — click 4 corners on LEFT (top-left, top-right, bottom-right, bottom-left). Next: top-left (1/4)"*.
2. Click the four corners on the **LEFT** image in that exact order, then the same four physical corners on the **RIGHT** image.
3. Repeat for all five planes.
   The status line under each image reports what was recovered, for example `center 7x7 — 49 corners`.
4. Use **Clear plane** to redo a single plane, or **Reset points** to start over.
   If a plane recovers the wrong grid size, enter the expected size in the **Size hint** fields (for example `6x5`) and click that plane's corners again.
   **Auto Detect** attempts a plane without manual clicks.
5. Press **Start Calculation** and read the results on **3D_ORI_DET**, under **Mean reprojection error / RMS**.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ Click the Same Physical Corners on Both Images</div>
  <div>
    Corners are paired between the two cameras by their <strong>(row, column)</strong> position in the recovered grid.
    If the left and right quads do not cover the same physical part of the board, the pairing is wrong and the triangulated points are meaningless, even though the calculation still completes and still prints an RMS.
  </div>
</div>

---

## Reading the Results

Both pipelines report the same two numbers per camera:

| Value | Meaning |
|---|---|
| **Mean** | Average reprojection error over all points, in pixels. |
| **RMS** | Root-mean-square of the same errors — punishes large individual errors more than the mean does. |

The window also shows the **shortest inter-ray distance** (how far apart the two camera rays pass at their closest point), per-direction distances such as **CENTER Mean Dist**, and inter-plane angles such as **CENTER vs NORTH**.

How to read these numbers:

- **Lower RMS is better.** It shows directly how well the calibration predicts where a known 3D point lands in the image.
- **If RMS is much larger than the mean**, a few corners are badly wrong rather than everything being slightly off.
  Check for a mis-detected direction or a mis-clicked plane before blaming the calibration itself.
- **A large inter-ray gap** points at the camera positions or the corner pairing, not at corner detection.

---

## Why Two Methods Exist

This window's real job is to analyse a calibration result: triangulate a checkerboard into 3D space and report the reprojection error for that calibration.
Comparing methods is a side effect, not the point.

There happen to be two ways to find the corners that feed that analysis — through the Anypoint remap, or directly on the original fisheye image with ORI_DET.
Because both paths feed the same triangulation and RMS code, running one calibration analysis with both tabs produces a calibration result from each method, which makes them easy to compare.

To compare them yourself: run **Anypoint** on a stereo scene and note the left and right RMS, then click the same physical corners under **ORI_DET** and note the RMS on **3D_ORI_DET**.
The two numbers tell you which detection method currently performs better for that setup.

Which method wins overall is still an open question — neither pipeline logs its own timing, so speed has to be judged in practice too.
Both stay in the application so this comparison keeps happening naturally as part of normal calibration work.
Once results consistently favor one method, a future version is expected to drop the other and standardise on a single 3D method.

---

## Troubleshooting

| Problem | Cause | Solution |
|---|---|---|
| *"Load an image + parameter JSON first."* | A camera has no image or no parameter file. | Load both, for both cameras, before detecting. |
| *"Triangulation produced no points."* | No corner was matched between the two cameras. | Check that both cameras detected the same directions or planes, and that the pairing is consistent. |
| A direction reports no detected points | The board is outside that anypoint view, or the framing is unusable. | Adjust Alpha / Beta / Zoom until the board is fully visible, or use the auto-framing sweep. |
| *"Invalid board size. Example: 6x5"* | The size hint is not in `rows x cols` form. | Enter it as two numbers separated by `x`. |
| A plane recovers far fewer corners than expected | The four clicked corners do not enclose the whole board, or the auto size guess is wrong. | Reset that plane, click the outermost-inner corners, and set the size hint. |
| RMS is high with **both** methods | Suspect the calibration itself rather than the detection. | Re-check the centre with [Setup Center](/moilcalib_documentation/docs/v1.1/verification/setup-center), then review the calibration result. |
| Distances are wrong but the RMS looks fine | Reprojection error does not validate scale. | Verify **Camera Position X / Y / Z** against the physical rig. |

---

## Summary

3D Verification reconstructs checkerboard corners in 3D from a stereo fisheye pair and reports how far they land from the original pixels once projected back.
A low **RMS in pixels**, on a rig with correct camera positions, is the evidence that a calibration is good.

Getting the corners can happen two ways: **Anypoint**, from a rectified remap and largely automatic, or **ORI_DET**, from four manual clicks per plane on the untouched fisheye.
They share the same triangulation and error calculation, so either one gives a valid result, and the two can be compared directly.
Which detection method is better is still being tested — every scene analysed with both methods adds a data point, and once a winner is clear, a future version will keep only one.
