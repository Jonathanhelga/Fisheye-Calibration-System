---
id: setup-center
slug: /verification/setup-center
title: Setup Center
---

# Setup Center

**Setup Center** answers one question: is the camera centre (`iCx`, `iCy`) in the parameter file correct for this camera?
A wrong centre point makes every later result wrong.
The panorama bends, the anypoint view drifts, and the calibration numbers inherit the error.
This tool lets you check the centre visually and fix it before you trust the parameters.

Setup Center is new in version 1.1.
You open it from the **Calibration Result / 3D Validation** panel of the main window, next to **Moil Cali Result** and **3D Verification**.

Use it after calibration, before you accept the camera parameters, or any time the panorama or anypoint view looks tilted or off-centre.
It is also worth running on a camera parameter JSON that came from another source and has never been checked.
If instead you want to measure real distances on the pattern, use [3D Verification](/moilcalib_documentation/docs/v1.1/verification/3d-verification) instead.

---

## What It Needs

Setup Center works on still images only.
There is no live camera or video input here.

You need a captured fisheye image (`.png`, `.jpg`, `.jpeg`, or `.bmp`).
When you open the tool from the main window, the current positive capture is loaded automatically.
You also need the Moildev camera parameter JSON, which holds `iCx` / `iCy` and the camera's calibration resolution.

---

## The Window

The left side shows the image views.
The right side is a control panel with four numbered sections.

<Figure id="fig-1" number="1" caption="The Setup Center window — Original Fisheye with the guide rings and crosslines (left), Panorama Preview and Anypoint View (bottom), and the four-section control panel (right).">

![Setup Center window](../assets/images/img_101.png)

</Figure>

### Views

| View | Purpose |
|---|---|
| **Original Fisheye** | The source image. Click anywhere on it to move the centre or to aim the anypoint view. |
| **Panorama Preview** | The unwrapped panorama. A centre error shows up here as a wave or tilt in what should be a straight horizon. |
| **Anypoint View** | A rectified view of the direction you clicked. Use it to confirm that straight lines in the scene really look straight. |

### Control Panel

| Section | Contents |
|---|---|
| **1. Media Source** | `Open Image…` — load a fisheye image. The path of the loaded file is shown above the button. |
| **2. Camera Parameter** | `Open Parameter JSON…` — load the camera file. A readout confirms what was read, for example `Resolution: 3040 × 3040 · fov 200°`. |
| **3. Center Point** | `Icx` and `Icy` spin boxes, and `Save Center to Parameter File`. |
| **4. Verification Views** | `Panorama` / `Anypoint` toggles, the **Guide α (°)** checkbox with its angle list (e.g. `50, 70, 90, 110`), the **Mode** selector, the aiming spin boxes, and **Zoom**. |

---

## How to Verify a Centre

1. **Load the image and the parameter JSON** in sections 1 and 2.
   The resolution line confirms the file was read correctly.
2. **Turn on the guide rings.**
   Tick **Guide α (°)** and enter the alpha angles you want drawn.
   The rings are drawn around the current centre, so if they are not concentric with the fisheye circle, the centre is wrong.
3. **Adjust the centre.**
   Click the correct centre directly on the **Original Fisheye** view, or fine-tune it with the **Icx** / **Icy** spin boxes.
   The previews rebuild as you change it.
4. **Check the previews.**
   A correct centre gives a panorama with a level horizon and an anypoint view without skew.
   Use the panorama slider to sweep around, and click different directions to re-aim the anypoint view.
5. **Save.**
   Press **Save Center to Parameter File**.
   The new `iCx` / `iCy` are written back into the JSON, and a confirmation shows the saved values and the file path.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ Saving overwrites your file</div>
  <div>
    <strong>Save Center to Parameter File</strong> writes directly into the JSON you loaded.
    Keep a copy of the original if you may want to go back to it.
  </div>
</div>

---

## Anypoint Aiming Modes

The **Mode** selector chooses how you aim the anypoint view: **Mode 1** uses **Alpha** and **Beta** (degrees), and **Mode 2** uses **Pitch** and **Yaw** (degrees).
A **Zoom** control is available in both modes, and clicking on the Original Fisheye view sets the aim directly.

---

## Reading Figure 1

The screenshot above shows a correctly centred example, so it's a useful reference for what "good" looks like.
The guide rings on the Original Fisheye are drawn at `50, 70, 90, 110°` and sit concentric with the pattern, with the crosslines meeting at the pattern centre.
The centre values are `Icx 719`, `Icy 705`, for a `3040 × 3040`, 200° fov camera.
In the Panorama Preview, the stripe pattern runs straight and level; a wrong centre would bend or tilt it.
In the Anypoint View, the concentric rings stay circular and centred at `Zoom 4.00`; a wrong centre would push them off to one side.
