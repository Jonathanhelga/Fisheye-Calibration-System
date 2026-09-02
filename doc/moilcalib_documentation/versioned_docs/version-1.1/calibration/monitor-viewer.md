---
id: monitor-viewer
slug: /calibration/monitor-viewer
title: Monitor Viewer
---

# Monitor Viewer

The **Monitor Viewer** window sends calibration pattern images to the monitors around the camera rig.
It has five sections, one for each monitor: **TOP**, **N**, **W**, **S**, and **E**.
Each section lets you pick an image, set its brightness, and turn its monitor on or off.

You normally use this window together with the **PCT Pattern Generator**.
The Pattern Generator creates the pattern images first.
The Monitor Viewer then loads those images and puts each one on the correct monitor.

---

## 1. Overview

<Figure id="fig-1" number="1" caption="Monitor Viewer main window overview.">

![Monitor Viewer overview](../assets/images/monitor-viewer.png)

</Figure>

The window shows five panels side by side, plus a **Control** menu in the menu bar.

| No. | Area | Purpose |
|---:|---|---|
| 1 | **TOP** | Pattern for the top monitor. Usually the concentric pattern. |
| 2 | **N** | Pattern for the north monitor. |
| 3 | **W** | Pattern for the west monitor. |
| 4 | **S** | Pattern for the south monitor. |
| 5 | **E** | Pattern for the east monitor. |
| 6 | **Control menu** | Contains **4Side turn off**, which turns off the four side monitors at once. See [section 6.5](#65-4side-turn-off). |

The panels are ordered **TOP, N, W, S, E** from left to right.
This is not compass order, so always check the direction header printed on each panel rather than counting positions.

When you first open the window, every preview is empty and every **Img path** field is blank.
The TOP panel starts at `5%` brightness, and the four side panels start at `1%`.
Getting every pattern onto its correct monitor, at a workable brightness, is the whole point of this window before you capture a calibration image.

---

## 2. Panel Layout

All five panels share the exact same layout.
Only the direction header and the brightness value change between them.

<Figure id="fig-2" number="2" caption="The five monitor panels side by side, showing the shared panel structure.">

![Monitor Viewer panel row](../assets/images/panel-layout.png)

</Figure>

Each panel has four stacked parts:

| No. | Area | Function |
|---:|---|---|
| 1 | **Direction Header** | Shows which monitor this panel controls: `TOP`, `N`, `W`, `S`, or `E`. |
| 2 | **Image Preview Area** | Shows the chosen image. Empty until you pick one. |
| 3 | **Img path and Browse...** | Shows the selected file's path and opens a file dialog to choose an image. |
| 4 | **Brightness / Update / Turn off** | Sets brightness, sends the image to the monitor, or turns that monitor's output off. |

---

## 3. The Five Panels

Each panel below is identical in layout, so only the direction and default brightness change.
The **TOP** panel defaults to `5%` because the top monitor sits farther from the camera and needs more intensity to show up clearly in the captured image.
The four side panels, **N**, **W**, **S**, and **E**, all default to `1%`.

<Figure id="fig-3" number="3" caption="TOP monitor panel.">

![Monitor Viewer TOP panel](../assets/images/top-panel.png)

</Figure>

<Figure id="fig-4" number="4" caption="North monitor panel.">

![Monitor Viewer North panel](../assets/images/north-panel.png)

</Figure>

<Figure id="fig-5" number="5" caption="West monitor panel.">

![Monitor Viewer West panel](../assets/images/west-panel.png)

</Figure>

<Figure id="fig-6" number="6" caption="South monitor panel.">

![Monitor Viewer South panel](../assets/images/south-panel.png)

</Figure>

<Figure id="fig-7" number="7" caption="East monitor panel.">

![Monitor Viewer East panel](../assets/images/east-panel.png)

</Figure>

---

## 4. Main Functions

### 4.1 Browse Image

Click **Browse...** to pick a calibration image from your computer.
Once selected, its path appears in the **Img path** field and the image appears in the preview area.

### 4.2 Update Monitor Display

Click **Update** to send the selected image to that monitor at the current brightness.
Do this after picking an image and setting brightness, for each direction you want to display.

### 4.3 Brightness Control

The **Brightness** field sets how bright the pattern appears on that monitor, as a percentage.
A lower value, such as `1%`, reduces intensity and can help avoid overexposure.
A higher value, such as `5%`, increases intensity and can help if the pattern looks too dark.

Use the lowest brightness that still gives clear black-and-white contrast in the captured fisheye image.
Too much brightness overexposes the pattern and reduces calibration accuracy.

### 4.4 Turn Off Monitor

Click a panel's **Turn off** button to disable that monitor's output.
This only affects the one direction, so it is useful for testing one monitor at a time or for clearing a display after a capture.

### 4.5 4Side Turn Off

The **Control** menu's **4Side turn off** action turns off the four side monitors, **N**, **W**, **S**, and **E**, in a single click instead of four.
It is marked as area **6** in [Figure 1](#fig-1).
Use it when you want to check the TOP pattern on its own, for example to confirm the concentric pattern is centered before adding the side patterns back.

---

## 5. Pattern Display Workflow

Follow this order when setting up the monitors for calibration:

1. Open **PCT Pattern Generator** and generate the pattern images you need.
2. Open **Monitor Viewer**.
3. Use **Browse...** to pick the correct image for each direction.
4. Set the brightness for each direction.
5. Click **Update** for each direction.
6. Confirm the patterns look correct on the physical monitors.
7. Capture the calibration image from the main camera window.

---

## 6. Integration with PCT Pattern Generator

The Monitor Viewer works together with two other windows.

| Window | Role |
|---|---|
| **PCT Pattern Generator** | Creates and saves pattern images, such as concentric and stripline patterns. |
| **Monitor Viewer** | Loads those images and displays each one on the correct monitor. |
| **Main Window** | Captures the positive or negative calibration image once the patterns are displayed. |

A typical setup pairs each direction with a pattern type:

| Monitor Direction | Common Pattern Type |
|---|---|
| **TOP** | Concentric pattern. |
| **N** | Stripline pattern. |
| **W** | Stripline pattern. |
| **S** | Stripline pattern. |
| **E** | Stripline pattern. |

Make sure each image is sent to the correct direction.
Sending the wrong pattern to the wrong monitor produces a calibration image that does not match the expected layout.

---

## 7. Technical Notes

| Item | Description |
|---|---|
| **Monitor sections** | TOP, N, W, S, and E. |
| **Image source** | Pattern images generated by the PCT Pattern Generator, or any image chosen manually. |
| **Default brightness** | `5%` for TOP, `1%` for N, W, S, and E. |
| **Panel order** | Left to right: TOP, N, W, S, E. |
| **Control menu** | Provides **4Side turn off**, which disables N, W, S, and E together. |

---

## 8. Troubleshooting

| Problem | Possible Cause | Solution |
|---|---|---|
| Preview image does not appear | Image path is empty or invalid. | Click **Browse...** again and select a valid image file. |
| Monitor does not update | Monitor server connection is not active. | Check the Monitor URL in the Main Window and reconnect. |
| Wrong pattern appears on monitor | Image was selected for the wrong direction. | Recheck the TOP, N, W, S, and E image paths. |
| Pattern is too bright | Brightness value is too high. | Reduce the brightness value and click **Update** again. |
| Pattern is too dark | Brightness value is too low. | Increase the brightness value gradually and click **Update** again. |
| Calibration result looks unstable | Pattern direction or brightness may be incorrect. | Verify monitor direction, pattern visibility, and captured fisheye image quality. |
| All four side monitors went dark at once | **Control → 4Side turn off** was used. | Reselect the image for each side direction and click **Update** on each panel. |
| Pattern went to the wrong side monitor | Panels are ordered TOP, N, W, S, E, which is not compass order. | Read the direction header on the panel instead of counting positions. |

---

## Summary

The **Monitor Viewer** controls the calibration pattern images shown on five monitors: **TOP**, **N**, **W**, **S**, and **E**.
Every panel shares the same layout: a preview, an image path with browse button, a brightness setting, an update button, and a turn-off button.
The **Control** menu adds **4Side turn off** to clear the four side monitors together.
Use this window after generating patterns in the PCT Pattern Generator and before capturing calibration images in the main workflow.
