---
id: camera-calibration
slug: /calibration/camera-calibration
title: Camera Calibration
---

# Camera Calibration

This is step 2 of the calibration workflow: the capture run itself.
You use the Main Window to home the axes, put the calibration pattern on the monitors, and take the two images everything else is computed from — the positive shot and the negative shot.

This page walks through that procedure in order.
For what every field, LED, and button in the Main Window does, see the [Main Window Reference](/moilcalib_documentation/docs/v1.1/system-overview/main-window).

Before you start, make sure of three things.
The [servers](/moilcalib_documentation/docs/v1.1/installation/server) are running and reachable, and the [client](/moilcalib_documentation/docs/v1.1/installation/client) is built.
The calibration patterns are prepared in the [PCT Pattern Generator](/moilcalib_documentation/docs/v1.1/calibration/pct-pattern-generator) and assigned to the correct directions in the [Monitor Viewer](/moilcalib_documentation/docs/v1.1/calibration/monitor-viewer) (step 1).
The axis stage, monitors, and camera are connected and powered.

<Figure id="fig-1" number="1" caption="The Main Window — every step on this page happens here.">

![Main Window overview](../assets/images/main-window-overview.png)

</Figure>

---

## Where the Captured Images Are Saved

Every capture is written as a PNG into an `image_cali/` folder.
The two files you need are these:

| Button | File written | What it is |
|---|---|---|
| **Pos Shot** | `image_cali/capture_positive_shot.png` | The positive image — the pattern as generated |
| **Neg Shot** | `image_cali/capture_negative_shot.png` | The negative image — the same pattern with inverted colours |
| **Capture** | `image_cali/capture_single_image.png` | A plain preview frame, not used by the calibration |

The same folder also holds the patterns that were pushed to the monitors:

```text
image_cali/pattern_circle_<direction>.png    e.g. pattern_circle_top.png, pattern_circle_n.png
image_cali/_tmp_pattern_circle.png           temporary render from the Pattern Generator
```

The `image_cali/` folder is created relative to the directory the client was launched from, not the folder holding the binary.
If you are ever unsure where it is, read the **Img Path** field in the Camera Panel — after every capture it shows the full path of the file that was just written.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ CAPTURES OVERWRITE, AND RESET DELETES</div>
  <div>
    File names are fixed, so the next <strong>Pos Shot</strong> replaces <code>capture_positive_shot.png</code> and the next <strong>Neg Shot</strong> replaces <code>capture_negative_shot.png</code>. If you need to keep a round's images, copy them out of <code>image_cali/</code> before capturing again.
    <br/><br/>
    <strong>Reset</strong> in the menu bar goes further: it deletes every capture and pattern file in <code>image_cali/</code> (after asking for confirmation). Reference sample images and pattern JSON configurations are left untouched.
  </div>
</div>

---

## 1. Connect to the Servers

In the **HTTP Server URL** panel, enter the three server addresses and press **Update** for each one.

| Field | Local server | Remote rig |
|---|---|---|
| **Axis URL** | `http://127.0.0.1:8000/` | `http://<server-ip>:8000/` |
| **Monitor URL** | `http://127.0.0.1:8001/` | `http://<server-ip>:8001/` |
| **Camera URL** | `http://127.0.0.1:8002/` | `http://<server-ip>:8002/` |

Pressing **Update** next to the **Axis URL** also re-checks the origin state of all five axes.
Use it if the axis LEDs look wrong or the stage was power-cycled.

---

## 2. Home the Axes

<Figure id="fig-2" number="2" caption="Axis Control Panel.">

![Axis Control Panel](../assets/images/axis-control-panel.png)

</Figure>

Press **All HOME** before the first capture of a session.
A short dialog opens first and checks the origin sensor of all five axes, then closes by itself.
Homing then runs in a fixed order — Yaw, Pitch, X, Y, Z — skipping any axis already at its origin, and waiting for one axis to fully stop before starting the next.
Each axis reads `0.000` when it finishes.

While an axis is homing, its **M** lamp blinks yellow, and if it is X or Y, the coordinate field blinks the word `Homing`.
Controls for other axes grey out during this time — that is the safety lock, not a fault, and it clears once the stage confirms it has stopped.

Once homing is done, move the stage to the position for this round.
Pick a **Speed**, type the distance into the **Relative Move** box of the axis you want — millimetres for X / Y / Z, degrees for Yaw / Pitch — and press the direction button on either side of it.
The resulting **α / β** values are shown in the panel header and update live.

For the full description of the sensor LEDs, speed selector, and interlocks, see [Main Window Reference, Section 2](/moilcalib_documentation/docs/v1.1/system-overview/main-window#2-axis-control-panel).

---

## 3. Show the Pattern and Check the Camera

<Figure id="fig-3" number="3" caption="Camera Panel.">

![Camera Panel](../assets/images/camera-control-panel.png)

</Figure>

Press **Capture** to take a plain preview frame.
Use it to confirm three things before the real shots:

1. The camera server answers and an image appears in the preview.
2. The fisheye circle is inside the frame and roughly centred.
3. The monitors are showing the pattern at a brightness that is neither washed out nor too dark (adjust in the [Monitor Viewer](/moilcalib_documentation/docs/v1.1/calibration/monitor-viewer)).

Everything above the preview is a readout, not a setting: **Pattern Mode** shows which shot the current image came from (blank after a plain Capture), **Img Path** shows the file just written, **Org Res** is the resolution the camera sent, and **Cali Res** is the resolution of the scaled preview.
There is nothing to configure here.
Field-by-field description of this panel: [Main Window Reference, Section 4](/moilcalib_documentation/docs/v1.1/system-overview/main-window#4-camera-panel).

Note that **Open Img** is not active in version 1.1.
To work on an existing image, capture again, or point a tool that accepts a file — such as [Setup Center](/moilcalib_documentation/docs/v1.1/verification/setup-center) — at the PNG in `image_cali/`.

---

## 4. Take the Positive and Negative Shots

This is the actual measurement.
**Pos Shot** and **Neg Shot** are not plain captures — clicking either one runs a short sequence automatically: it pushes the matching pattern (positive or negative) to every monitor, waits about 300 ms for the screens to update, then fetches and saves a frame from the camera.
From that saved image, the pattern centre is auto-detected (when the Centering panel is in Auto mode), the ROI marker and edge circle are drawn, and both histograms refresh.

Both shots are needed because the positive and negative images show the same pattern with inverted colours.
The calibration uses the pair together to find the intersection points (ICT) reliably — a single image cannot separate the pattern edges from the background as cleanly.
The two shots detect their centres independently, into separate **Positive CPX/CPY** and **Negative CPX/CPY** fields, and are not expected to be identical — you will check and correct each one in the next step.

The captured pair looks like this — the concentric pattern in the middle and the stripline patterns on the four sides:

<Figure id="fig-4" number="4" caption={<>Positive shot — <code>image_cali/capture_positive_shot.png</code>.</>}>

![Captured positive shot](../assets/images/img_97.png)

</Figure>

<Figure id="fig-5" number="5" caption={<>Negative shot — <code>image_cali/capture_negative_shot.png</code>, the same layout with inverted colours.</>}>

![Captured negative shot](../assets/images/img_98.png)

</Figure>

---

## 5. Check and Correct the Centre

<Figure id="fig-6" number="6" caption="Centering Panel.">

![Centering Panel](../assets/images/centering-panel.png)

</Figure>

The centre point is the most important value in this step — everything downstream inherits an error in it.
Each shot has already filled in its own CPX/CPY; your job here is to confirm those values, and fix them if they are wrong.

The panel is a grid: the **CPX** and **CPY** columns hold the horizontal and vertical centre in original-image pixels, and the **Positive** and **Negative** rows hold the values from each shot.
**Center ROI** is the radius of the ROI box drawn around the centre on the preview — raise it if the marker box is too small to judge.

To confirm the centre, double-click the preview to open the zoomable viewer and check that the marked centre really sits on the middle of the concentric pattern.
Tick **Edge** and set a **Radius** to draw a circle overlay and compare the centre against the outer edge of the fisheye circle.
The positive and negative circles can be given their own **Color** and **Thickness**.

If the centre is wrong, choose one of these fixes:

| Situation | Do this |
|---|---|
| Auto-detection landed close but not exactly right | Set **PosThr** (positive image) or **NegThr** (negative image), then click once on the preview while the panel is in **Auto**. The centre is refined from its current value with that threshold, repeated until it stops moving. |
| Auto-detection is far off — wrong lobe of the pattern entirely | Switch to **Manual** and click the correct centre on the preview. The clicked point becomes CPX/CPY exactly as clicked, and the panel switches itself back to **Auto**. Click again to refine from there. |
| The centre is correct and you do not want captures to move it | Switch to **Locked**. Clicks are ignored and later shots leave the values alone. |

Do this for both images — the positive and negative centres are corrected separately.
Note that clicking only works once a Pos Shot or Neg Shot exists (a plain Capture leaves Pattern Mode empty, so clicks are ignored), and that **PosThr**/**NegThr** only affect the click-refine above, not the automatic centre found right after a shot.

### The `?` Button: Direction Difference

Once both shots exist, the **?** button in the Camera Panel gives a numeric check on the centre.
It finds the intersection nodes (ICT) along all eight directions of the latest positive and negative shots, then compares the opposite pairs — N-S, W-E, NW-SE, SW-NE — node by node.

A dialog lists each pair's nodes with their values and the difference between them, plus the mean and maximum difference per pair.
A well-centred capture is symmetric, so the differences sit near zero; the dialog paints any difference of 5 px or more in red.
Large differences point back at a wrong centre, so fix the centre and take the shots again.

If the button reports that it needs a positive and a negative shot first, take both, then try again.

Full description of every field and mode in this panel: [Main Window Reference, Section 5](/moilcalib_documentation/docs/v1.1/system-overview/main-window#5-centering-panel).

---

## 6. Read the Histograms

<Figure id="fig-7" number="7" caption="Histogram panel.">

![Histogram Panel](../assets/images/histogram-panel.png)

</Figure>

Both histograms refresh automatically after a positive or negative shot.
This is the quality gate for the capture — read them before you move on.

There are two identical panels, **Histogram1** and **Histogram2**, so you can keep two different comparisons on screen at once.
In either one, tick directions in the **Pos** and **Neg** columns (eight each: N, S, W, E, NW, SE, SW, NE) and press **Show Curve**.
The plot draws grey level against distance from the centre along that direction.

What you tick changes what you see:

| What you tick | What you get |
|---|---|
| The same direction in both the Pos and Neg columns | **Comparison mode.** Only that direction is drawn: positive in red, negative in green, with a white vertical line at every intersection node (ICT) between them. |
| Directions in one column only, or different directions in each | One curve per ticked direction, each in its own colour. No intersection lines. |

To compare the positive and negative of one direction, tick that direction in both columns.
To compare several directions against each other, keep to one column.
(The **Pop Up** button does nothing in version 1.1 — read the curves in place, or enlarge the main window.)

Use the shape of the curves to decide whether to move on or re-capture:

- **Clear swings between light and dark** — good, move on to the next round.
- **Flat tops** — the monitor is too bright and the stripe edges are lost. Lower the brightness in the [Monitor Viewer](/moilcalib_documentation/docs/v1.1/calibration/monitor-viewer) and capture again.
- **Barely any swing** — too dark. Raise the brightness and capture again.
- **In comparison mode, the red and green curves differ a lot, or the white intersection lines are few and irregular** — suspect a wrong centre or the wrong pattern on that monitor. Fix the cause, then capture again.

Do not try to compensate for a bad capture later in the analysis — re-capture instead.

Description of the controls and how the curves are built: [Main Window Reference, Section 7](/moilcalib_documentation/docs/v1.1/system-overview/main-window#7-histogram-panel).

---

## 7. Repeat for Each Round

A calibration set is built from several rounds, moving the stage between them.
For each round:

1. Move the axes to the next position.
2. Take the **Pos Shot**.
3. Take the **Neg Shot**.
4. Confirm the centre, run the **?** direction-difference check, and read the histograms.
5. Copy the two PNGs out of `image_cali/` if you need to keep them — the next round overwrites them.

---

## Before Moving On

| Check | |
|---|---|
| All five axes homed successfully | ☐ |
| Patterns visible on every monitor direction | ☐ |
| `capture_positive_shot.png` written and looks correct | ☐ |
| `capture_negative_shot.png` written and looks correct | ☐ |
| Positive CPX/CPY sit on the centre of the concentric pattern | ☐ |
| Negative CPX/CPY sit on the centre of the concentric pattern | ☐ |
| Direction differences from the **?** check are small, with nothing in red | ☐ |
| Histogram curves swing clearly, no flat tops | ☐ |
| Images copied out of `image_cali/` if this round must be kept | ☐ |

Next: open **Moil Cali Result** to compute and inspect the values — see [3. Calibration Result](/moilcalib_documentation/docs/v1.1/calibration/cali-result).

---

## Troubleshooting

| Problem | Cause | Solution |
|---|---|---|
| "No image received from camera server" | The camera server is not running or the URL is wrong. | Check the Camera URL, confirm the camera server is up (`http://<server-ip>:8002/docs`), then press **Update**. |
| "Received data could not be decoded as an image" | The server replied, but not with a usable image. | Check the camera driver selection on the server and that the camera is connected. |
| The capture is black or shows no pattern | The pattern was not pushed to the monitors, or brightness is at zero. | Re-send the pattern from the [Monitor Viewer](/moilcalib_documentation/docs/v1.1/calibration/monitor-viewer) and raise the brightness. |
| Auto-detected centre is clearly wrong | The threshold does not suit this image. | Adjust **PosThr**/**NegThr**, or switch to **Manual**, click the correct centre, and let Auto refine it. |
| The centre keeps changing between captures | Auto mode re-detects on every shot — this is expected. | Use **Locked** once you are satisfied with the centre. |
| Clicking the image does nothing | Pattern Mode is empty (a plain Capture). | Take a **Pos Shot** or **Neg Shot** first. |
| No `image_cali/` folder anywhere | It is created relative to the launch directory. | Read the full path from the **Img Path** field, or relaunch the client from the project root. |
| The images disappeared | **Reset** was used. | Reset deletes the cached captures and patterns. Capture again — and copy files out before resetting next time. |
| Controls are greyed out | An axis is still moving; the safety lock is active. | Wait for the axis to stop, or press its **STOP** button. |

---

## Summary

Camera calibration is the capture run: connect to the servers, home the axes, put the pattern on the monitors, then take a positive and a negative shot.
Each shot pushes its own pattern, saves a PNG into `image_cali/`, detects the pattern centre from that image, and refreshes the histograms.
The two files —

```text
image_cali/capture_positive_shot.png
image_cali/capture_negative_shot.png
```

— together with the PCT values from the Pattern Generator, are the input to the calibration result calculation in step 3.

---

_The panel screenshots on this page are version 1.1 captures. The two example shots ([Figure 4](#fig-4) and [Figure 5](#fig-5)) are still reused from version 1.0 and will be replaced._
