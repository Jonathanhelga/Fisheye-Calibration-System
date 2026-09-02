# Screenshot Capture List

This note tracks the screenshots that still need to be recaptured from the **C++ application** to replace the older **Python application** screenshots used in the documentation site.

Documentation repository: `~/Desktop/moilcalib_documentation`
Target folder for new images: `versioned_docs/version-1.1/assets/images/`

## Status summary

| Item | Count |
|---|---:|
| C++ screenshots and diagrams now in use | 48 |
| Python screenshots still in use | 20 |
| Of those, app UI that still needs recapture | 13 |
| Of those, external or non-UI images that can stay | 7 |
| New C++ features with no screenshot at all | 1 |
| Python screenshots deleted as unnecessary | 40 |

Everything that can be captured **without real calibration data** is done, and the first batch of captures made **with** real data is now wired in.
What remains is listed below: two round popups that were captured from the wrong widget, the filled panels in the Aggr tab, the Graphs tab plots, the Overlap tab, the Clean Noise dialog, and four windows outside Cali Result.

## Naming convention

Replaced images use lowercase kebab-case describing the window and the view, for example:

```
calibration-result-main-window.png
result-table-control-row.png
panel-vgap-hgap.png
graph-entrance-pupil-shift.png
```

Please follow the same pattern for new captures.
Avoid the old `img_NN.png` style, because those names carry no meaning and made it hard to tell which screenshot belonged to which part of the app.

## Capture conditions

Everything left in this list needs calibration data **loaded**, unless the entry says "empty".
Several figures deliberately show both the empty and the filled state, and the empty half of those pairs has already been captured.

## What not to capture

A screenshot earns its place when it shows **real data, a real panel, or a layout that is hard to describe in words**. If a sentence does the job, write the sentence.
Screenshots of a text field with text typed into it, a standard OS file or save dialog, or a right-click menu whose actions are listed in a table right below it were deleted rather than recaptured.

---

# Still needed

## 1. Clean Noise dialog

| Suggested name | What to capture |
|---|---|
| `calibration-result-clean-noise-dialog.png` | The **Clean Noise** dialog, opened from any round. Show the three direction groups (`N & S`, `W & E`, diagonals), the min/max band fields with auto-detected values filled in, and the "Apply to ALL rounds with data" checkbox. Referenced by `result-table-view.md` section 2.4 and `cali-result/index.md`, neither of which has an image for it. |

Auto-detection only proposes a band when a group has at least four gap samples, so open the dialog on a round that actually has ICT data, otherwise the fields come up blank.

## 2. Calibration Data window

| Old image | Suggested name | What to capture |
|---|---|---|
| `img_55.png` | `database-calibration-data-window.png` | The **Calibration Data** window with its search filters and record list. |

## 3. The two round popup graphs, recapture needed

Both files delivered under these names show the **embedded** ZFL-IH plot on the `parameter` tab, not the popups these figures document. The embedded plot is titled `ZFL-IH  (Round: 1, 3, 5, 7, 9)` and carries the coloured IH range bands. The popups are titled `ZFL-IH (Round 1)` and `Overlap (Round 1)`, take a single round, and have no bands.

To reach them: right-click a **round tab** in the Cali Result window and choose the menu entry.

| Old image | Suggested name | What to capture |
|---|---|---|
| `img_63.png` | `cali-result-zfl-ih-popup.png` | Right-click round tab → **Show ZFL-IH Graph (Round N)**. Title must read `ZFL-IH (Round N)` and the X-axis must read `IH(pixel)`. |
| `img_64.png` | `cali-result-overlap-popup.png` | Right-click round tab → **Show Overlap Graph (Round N)**. Title must read `Overlap (Round N)` and the X-axis must read `ICT(pixel)`. |

In `controller_cali_result.cpp` both menu entries plot the same `ictZflPoints(round)` data, so the two popups differ only in title and X-axis label. Capture them anyway, because the documentation describes them as two separate menu actions.

## 4. Filled panels in the "Aggr by Distance and Range" tab

The empty state of each of these panels is already captured. What is missing is the same panel after a calculation has run.

| Old image | Suggested name | What to capture |
|---|---|---|
| `img_68.png` | `panel-interval-result-filled.png` | The interval result table filled after running **Min Aggregation by interval**. |
| `img_70.png` | `panel-aggr-by-range-filled.png` | The Aggr by Range and Distance panel with a real result. |

## 5. Graphs tab

The three plots moved out of the header into the new `Graphs` tab. The tab itself is captured in `main-window-overview-header-management.png`, but its plots are empty there.

| Old image | Suggested name | What to capture |
|---|---|---|
| `img_49.png` | `graph-entrance-pupil-shift.png` | The **Shift of Entrance Pupil** plot with ray curves drawn. |
| `img_50.png` | `graph-distance-vs-ih-range.png` | The **Distance vs IH Range** plot with range points labelled. |
| `img_51.png` | `graph-distance-vs-alpha.png` | The **Distance vs Alpha** plot. |

A capture of the whole `Graphs` tab with all three plots populated would also be an improvement over the current empty one.

## 6. Parameter view, second pass

`parameter-panel.png` is wired in, but it was taken with `cameraName`, `cameraFov`, `iCx`, `iCy`, `imageWidth`, `imageHeight`, and `calibrationRatio` still empty. Only `parameter2` to `parameter5` hold values. A recapture with every field populated would show the complete intrinsic model the page describes.

## 7. Overlap view

| Old image | Suggested name | What to capture |
|---|---|---|
| `img_71.png` | `overlap-view-loaded.png` | The Overlap tab with calibration data loaded, so both the Overlap graph and the Aggregation vs. Distance graph show real curves. |

## 8. Other windows

These are outside the Cali Result window. Please confirm each window still exists in the C++ application before capturing.

| Old image | Used in | Suggested name | What to capture |
|---|---|---|---|
| `img_11.png` | `installation/client.md` | `client-main-window.png` | The Calibration System Client main window. |
| `img_101.png` | `verification/setup-center.md` | `setup-center-window.png` | The Setup Center window. |
| `img_99.png` | `verification/3d-verification.md` | `3d-verification-anypoint.png` | Anypoint detection on the Camera Right tab. |
| `img_100.png` | `verification/3d-verification.md` | `3d-verification-ori-det.png` | ORI_DET manual corner picking on the original images. |

---

# Done

Captured from the C++ application and already wired into the documentation.

| New image | Replaced | Used in |
|---|---|---|
| `main-window-overview-header-management.png` | `img_28.png` | `main-window-overview.md` |
| `cali-result-header-area.png` | `img_28.png` | `reload-calibration-data.md` |
| `cali-result-tab-strip.png` | `img_37.png` | `main-window-overview.md` |
| `panel-interval-result-table.png` | `img_38.png` | `main-window-overview.md` |
| `panel-vgap-hgap.png` | `img_39.png` | `main-window-overview.md` |
| `panel-pixel-size-distance.png` | `img_40.png` | `main-window-overview.md` |
| `panel-min-aggregation-interval.png` | `img_41.png` | `main-window-overview.md` |
| `panel-min-aggregation-filled.png` | `img_69.png` | `main-window-overview.md` |
| `panel-aggr-by-range-distance.png` | `img_42.png` | `main-window-overview.md` |
| `panel-range-analysis-matrix.png` | `img_43.png` | `main-window-overview.md` |
| `graphs-info-entrance-pupil.png` | new, no predecessor | `main-window-overview.md` |
| `result-table-control-row.png` | `img_80.png` | `result-table-view.md` |
| `result-table-column-structure.png` | `img_89.png` | `result-table-view.md` |
| `result-table-empty.png` | `img_79.png` | `result-table-view.md` |
| `result-table-formula-panel.png` | `img_81.png` | `result-table-view.md` |
| `control-and-input-result-table.png` | new, no predecessor | `cali-result/index.md` |
| `parameter-ih-alpha-graph.png` | `img_72.png` | `parameter-view.md` |
| `parameter-zfl-ih-graph.png` | `img_73.png` | `parameter-view.md` |
| `parameter-info-ih-alpha.png` | new, no predecessor | `parameter-view.md` |
| `cali-result-loaded-data.png` | `img_48.png` | `main-window-overview.md`, `reload-calibration-data.md` |
| `panel-range-history-values.png` | `img_57.png` | `main-window-overview.md` |
| `result-table-filled.png` | `img_88.png` | `result-table-view.md` |
| `parameter-ih-alpha-filled.png` | `img_75.png` | `parameter-view.md` |
| `parameter-zfl-ih-filled.png` | `img_76.png` | `parameter-view.md` |
| `parameter-panel.png` | `img_74.png` | `parameter-view.md` |

`result-table-formula-panel.png` is a redrawn diagram rather than a screenshot. The old `img_81.png` showed the Alpha and ZFL formulas as one sequential chain and used `PCT` where the code uses `PCT_CAL`. The replacement shows the TOP and SIDE branches as the mutually exclusive choice they actually are.

---

# Images that do NOT need recapture

These 7 are not C++ application UI, so the existing files remain valid.

| Image | Why it can stay |
|---|---|
| `img_1.png` | Git installation page, external website. |
| `img_47.png` | Example Excel layout, shown in a spreadsheet application. |
| `img_103.png` | Figure 2 from Gennery 2006, an academic paper illustration. Must not be replaced. |
| `img_102.png` | Ray curve output graph. Recapture only if the plot styling changed. |
| `img_97.png` | Captured positive shot from the fisheye camera. Camera output, not UI. |
| `img_98.png` | Captured negative shot from the fisheye camera. Camera output, not UI. |
| `img_91.png` | Generated calibration pattern. Pattern output, not UI. |

---

# Deleted, do not recapture

These 40 files were removed from `versioned_docs/version-1.1/`. Either the figure that used them was cut as surface-level UI, or a new capture replaced them. They are listed only so nobody reshoots them from an older copy of this note. The `version-1.0` copies are untouched.

| Image | Was |
|---|---|
| `img_28.png` | Header area, replaced |
| `img_29.png` | URL pasted into the Cali Folder field |
| `img_30.png` | The same URL, selected, before processing finished |
| `img_31.png` | The same field showing a local cache path |
| `img_32.png` | Tree view of the calibration folder |
| `img_33.png` | **Clear All Table** confirmation dialog |
| `img_37.png` | Tab strip, replaced |
| `img_38.png` | Interval result table, replaced |
| `img_39.png` | V_Gap / H_Gap panel, replaced |
| `img_40.png` | Pixel Size and Dis / Round panel, replaced |
| `img_41.png` | Min Aggregation by Interval panel, replaced |
| `img_42.png` | Aggr by Range and Distance panel, replaced |
| `img_43.png` | Range Analysis Matrix, replaced |
| `img_44.png` | Save dialog from **Save to Excel** |
| `img_45.png` | File dialog from **Load Excel** |
| `img_46.png` | Folder dialog from **Load All Excel** |
| `img_52.png` | Terminal output after pressing **Stop**. The message is quoted in the text instead |
| `img_53.png` | OneDrive folder opened in a browser |
| `img_54.png` | Database source selection dialog |
| `img_56.png` | Database right-click menu. Its actions are listed in a table instead |
| `img_58.png` | Select Cali System dropdown. Its options are listed in the Configuration Mapping table |
| `img_60.png` | Round tab showing the `*` loaded marker |
| `img_61.png` | Round tab showing the `[OFF]` disabled state |
| `img_62.png` | Round tab right-click menu. Its actions are listed in a table instead |
| `img_65.png` | CSV save dialog |
| `img_66.png` | CSV saved confirmation message |
| `img_67.png` | Interval CSV opened in LibreOffice Calc |
| `img_69.png` | Min Aggregation panel with input filled, replaced |
| `img_72.png` | IH-Alpha graph area, replaced |
| `img_73.png` | ZFL-IH graph area, replaced |
| `img_79.png` | Empty result table, replaced |
| `img_80.png` | Result table control row, replaced |
| `img_81.png` | Formula panel, replaced by a redrawn diagram |
| `img_89.png` | Result table column structure, replaced |
| `img_48.png` | Loaded data example, replaced |
| `img_57.png` | Range and history distance values, replaced |
| `img_74.png` | Parameter panel, replaced |
| `img_75.png` | IH-Alpha filled graph, replaced |
| `img_76.png` | ZFL-IH filled graph, replaced |
| `img_88.png` | Filled result table, replaced |

---

# After capturing

1. Copy the new files into `versioned_docs/version-1.1/assets/images/`.
2. Update the `![alt](../../assets/images/...)` reference on each page listed above.
3. On `main-window-overview.md`, remove the "Screenshots pending replacement" warning box once every figure on that page is replaced.
4. Run `npm run build` in the documentation repository. The build fails on broken links and is the fastest way to catch a mistyped path.
5. Delete the replaced `img_NN.png` files only after the build passes.
