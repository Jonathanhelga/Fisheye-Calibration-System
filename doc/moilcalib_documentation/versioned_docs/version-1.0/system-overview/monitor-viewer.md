---
id: monitor-viewer
slug: /system-overview/monitor-viewer
title: Monitor Viewer
sidebar_label: Monitor Viewer
---

# Monitor Viewer

The **Monitor Viewer** window is used to manage calibration pattern images displayed on the monitor setup. It provides five monitor sections: **TOP**, **N**, **W**, **S**, and **E**. Each section has its own preview area, image path field, browse button, brightness setting, update button, and turn-off button.

This window is normally used together with the **PCT Pattern Generator**. The Pattern Generator creates the calibration images, while the Monitor Viewer loads and displays those images on the correct monitor direction.

---

## 1. Monitor Viewer Overview

<Figure number="1" caption="Monitor Viewer main window overview.">

![Monitor Viewer overview](../assets/images/img_82.png)

</Figure>

The Monitor Viewer contains five display panels arranged from left to right.

| No. | Monitor Section | Purpose |
|---:|---|---|
| 1 | **TOP** | Displays the pattern for the top monitor. This area commonly uses the concentric pattern. |
| 2 | **N** | Displays the pattern for the north monitor direction. |
| 3 | **W** | Displays the pattern for the west monitor direction. |
| 4 | **S** | Displays the pattern for the south monitor direction. |
| 5 | **E** | Displays the pattern for the east monitor direction. |

<div className="custom-note custom-important">
  <div className="custom-note-title">Main Goal</div>
  <p>The main goal of the Monitor Viewer is to make sure every calibration pattern is sent to the correct monitor direction before calibration images are captured.</p>
</div>

---

## 2. Common Panel Layout

Each monitor section uses the same basic layout. The example below shows the **N** monitor panel.

<Figure number="2" caption="Monitor Viewer panel structure using the N direction as an example.">

![Monitor Viewer N panel](../assets/images/img_92.png)

</Figure>

Each direction panel contains four main areas.

| No. | Area | Function |
|---:|---|---|
| 1 | **Direction Header** | Shows the active monitor direction, such as `TOP`, `N`, `W`, `S`, or `E`. |
| 2 | **Image Preview Area** | Displays the selected calibration image before or after it is sent to the monitor. |
| 3 | **Image Path and Browse** | Allows the user to select a calibration image file from the computer. |
| 4 | **Brightness / Update / Turn off** | Controls the brightness level, sends the image to the monitor, or turns off the selected monitor output. |

---

## 3. TOP Monitor Panel

<Figure number="3" caption="TOP monitor panel.">

![Monitor Viewer TOP panel](../assets/images/img_94.png)

</Figure>

The **TOP** panel is used for the top monitor display. In the calibration setup, this panel is commonly used to show the central concentric pattern.

| Component | Function |
|---|---|
| **TOP header** | Indicates that this panel controls the top monitor section. |
| **Preview area** | Shows the selected TOP calibration image. |
| **Img path** | Displays the selected image file path. |
| **Browse...** | Opens a file dialog so the user can choose an image for the TOP monitor. |
| **Brightness** | Sets the display brightness percentage. In the shown example, TOP uses `5%`. |
| **Update** | Sends or refreshes the selected TOP image on the monitor. |
| **Turn off** | Turns off the TOP monitor output. |

---

## 4. North Monitor Panel

<Figure number="4" caption="North monitor panel.">

![Monitor Viewer North panel](../assets/images/img_92.png)

</Figure>

The **N** panel controls the north monitor direction.

| Component | Function |
|---|---|
| **N header** | Indicates that this panel controls the north monitor direction. |
| **Preview area** | Shows the selected image for the north monitor. |
| **Img path** | Displays the selected north image file path. |
| **Browse...** | Selects the image file for the north monitor. |
| **Brightness** | Sets the brightness level for the north monitor. |
| **Update** | Updates the north monitor display using the selected image and brightness value. |
| **Turn off** | Turns off the north monitor output. |

---

## 5. South Monitor Panel

<Figure number="5" caption="South monitor panel.">

![Monitor Viewer South panel](../assets/images/img_93.png)

</Figure>

The **S** panel controls the south monitor direction.

| Component | Function |
|---|---|
| **S header** | Indicates that this panel controls the south monitor direction. |
| **Preview area** | Shows the selected image for the south monitor. |
| **Img path** | Displays the selected south image file path. |
| **Browse...** | Selects the image file for the south monitor. |
| **Brightness** | Sets the brightness level for the south monitor. |
| **Update** | Updates the south monitor display using the selected image and brightness value. |
| **Turn off** | Turns off the south monitor output. |

---

## 6. East Monitor Panel

<Figure number="6" caption="East monitor panel.">

![Monitor Viewer East panel](../assets/images/img_95.png)

</Figure>

The **E** panel controls the east monitor direction.

| Component | Function |
|---|---|
| **E header** | Indicates that this panel controls the east monitor direction. |
| **Preview area** | Shows the selected image for the east monitor. |
| **Img path** | Displays the selected east image file path. |
| **Browse...** | Selects the image file for the east monitor. |
| **Brightness** | Sets the brightness level for the east monitor. |
| **Update** | Updates the east monitor display using the selected image and brightness value. |
| **Turn off** | Turns off the east monitor output. |

---

## 7. West Monitor Panel

<Figure number="7" caption="West monitor panel.">

![Monitor Viewer West panel](../assets/images/img_96.png)

</Figure>

The **W** panel controls the west monitor direction.

| Component | Function |
|---|---|
| **W header** | Indicates that this panel controls the west monitor direction. |
| **Preview area** | Shows the selected image for the west monitor. |
| **Img path** | Displays the selected west image file path. |
| **Browse...** | Selects the image file for the west monitor. |
| **Brightness** | Sets the brightness level for the west monitor. |
| **Update** | Updates the west monitor display using the selected image and brightness value. |
| **Turn off** | Turns off the west monitor output. |

---

## 8. Main Functions

### 8.1 Browse Image

The **Browse...** button is used to select a calibration image from the computer. After an image is selected, its path is shown in the **Img path** field.

Typical usage:

```text
Click Browse...
   ↓
Select calibration pattern image
   ↓
Image path appears in Img path field
   ↓
Preview area shows the selected image
```

### 8.2 Update Monitor Display

The **Update** button sends or refreshes the selected image for that monitor direction.

Typical usage:

```text
Select image file
   ↓
Set brightness value
   ↓
Click Update
   ↓
Monitor displays the selected pattern
```

### 8.3 Brightness Control

The **Brightness** field controls the monitor brightness percentage for each direction. In the UI examples, most side directions use `1%`, while the TOP direction can use a different value such as `5%`.

| Brightness Value | Meaning |
|---|---|
| Low value, for example `1%` | Reduces display intensity and may help avoid overexposure. |
| Higher value, for example `5%` | Increases display intensity and may help if the pattern is too dark. |

<div className="custom-note custom-tip">
  <div className="custom-note-title">Brightness Tip</div>
  <p>Use the lowest brightness that still gives clear black-white pattern contrast in the captured fisheye image. Too much brightness can make the pattern overexposed and reduce calibration accuracy.</p>
</div>

### 8.4 Turn Off Monitor

The **Turn off** button disables the selected monitor output. This is useful when testing one monitor direction at a time or when stopping the pattern display after calibration capture.

---

## 9. Pattern Display Workflow

Use the following workflow when preparing the monitor display for calibration.

```text
Open Monitor Viewer
   ↓
Select image for TOP, N, W, S, and E
   ↓
Set brightness for each monitor section
   ↓
Click Update for each section
   ↓
Check that all monitor patterns are visible
   ↓
Capture calibration image from the fisheye camera
```

Recommended order:

1. Open **PCT Pattern Generator** and generate the needed pattern images.
2. Open **Monitor Viewer**.
3. Use **Browse...** to select the correct image for each direction.
4. Set brightness values.
5. Click **Update** for each direction.
6. Confirm the patterns are displayed correctly on the physical monitors.
7. Capture the calibration image from the main camera window.

---

## 10. Integration with PCT Pattern Generator

The Monitor Viewer is closely related to the PCT Pattern Generator.

| Window | Role |
|---|---|
| **PCT Pattern Generator** | Creates and saves pattern images such as concentric and stripline patterns. |
| **Monitor Viewer** | Loads those generated pattern images and displays them on the correct monitor direction. |
| **Main Window** | Captures the positive or negative calibration image after the patterns are displayed. |

A common calibration setup is:

| Monitor Direction | Common Pattern Type |
|---|---|
| **TOP** | Concentric pattern. |
| **N** | Stripline pattern. |
| **W** | Stripline pattern. |
| **S** | Stripline pattern. |
| **E** | Stripline pattern. |

<div className="custom-note custom-important">
  <div className="custom-note-title">Important</div>
  <p>The selected pattern image must match the physical monitor direction. If the wrong image is sent to the wrong direction, the captured calibration image may not match the expected pattern layout.</p>
</div>

---

## 11. Technical Notes

| Item | Description |
|---|---|
| **Monitor sections** | TOP, N, W, S, and E. |
| **Image source** | Pattern images generated by the PCT Pattern Generator or selected manually from local files. |
| **Image path field** | Stores the selected image location for each direction. |
| **Preview area** | Shows the selected image before display update. |
| **Brightness value** | Controls display intensity for each monitor section. |
| **Update action** | Sends the selected image to the monitor output. |
| **Turn off action** | Disables the selected monitor output. |

---

## 12. Troubleshooting

| Problem | Possible Cause | Solution |
|---|---|---|
| Preview image does not appear | Image path is empty or invalid. | Click **Browse...** again and select a valid image file. |
| Monitor does not update | Monitor server connection is not active. | Check the Monitor URL in the Main Window and reconnect. |
| Wrong pattern appears on monitor | Image was selected for the wrong direction. | Recheck TOP, N, W, S, and E image paths. |
| Pattern is too bright | Brightness value is too high. | Reduce the brightness value and click **Update** again. |
| Pattern is too dark | Brightness value is too low. | Increase the brightness value gradually and click **Update** again. |
| Calibration result looks unstable | Pattern direction or brightness may be incorrect. | Verify monitor direction, pattern visibility, and captured fisheye image quality. |

---

## Summary

The **Monitor Viewer** controls the display of calibration pattern images across five monitor directions: **TOP**, **N**, **W**, **S**, and **E**. Each direction has its own preview, image path, browse button, brightness setting, update button, and turn-off button. This window should be used after generating patterns in the PCT Pattern Generator and before capturing calibration images in the main calibration workflow.