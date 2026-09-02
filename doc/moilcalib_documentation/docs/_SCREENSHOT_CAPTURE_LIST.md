# Screenshot Capture List — v2.0

Every page reserves space for its screenshots with a `<Shot>` placeholder that
names the file and describes what to capture. **Nothing has been captured yet** —
`docs/assets/images/` is empty.

- Target folder: `docs/assets/images/`
- Capture from: the `calibration_inROS` build (`moilcali`) — **not** from v1.1
- Naming: lowercase kebab-case, `<window>-<view>.png`

## How to swap a placeholder for a real image

Save the file, then replace the `<Shot>` block with a `<Figure>` carrying the
same `id`, `number` and `caption`:

```mdx
<Figure id="fig-2" number="2" caption="Axis Control Panel.">

![Axis Control Panel](../assets/images/main-window-axis-panel.png)

</Figure>
```

`<Figure>` needs a **markdown image** in its children, not a `src` prop — only
markdown image nodes are rewritten to webpack requires, so a string path would
ship unresolved.

<!-- markdownlint-disable-next-line MD028 -->
> **A markdown image pointing at a missing file fails the build.** That is why
> the placeholders hold no `<img>`. Save the file first, then swap the block.

---

## Priority 1 — the 26 placeholders already in the pages

These have space reserved and a caption written. Filling them completes the
pages.

| # | File | Page |
|--:|---|---|
| 1 | `main-window-overview.png` | Main Window Reference |
| 2 | `main-window-header-strip.png` | Main Window Reference |
| 3 | `main-window-axis-panel.png` | Main Window Reference |
| 4 | `main-window-init-sensors-dialog.png` | Main Window Reference |
| 5 | `main-window-check-home-dialog.png` | Main Window Reference |
| 6 | `main-window-camera-panel.png` | Main Window Reference |
| 7 | `main-window-direction-diff.png` | Main Window Reference |
| 8 | `main-window-centering-panel.png` | Main Window Reference |
| 9 | `main-window-histogram-panel.png` | Main Window Reference |
| 10 | `main-window-auto-calibrate-running.png` | Main Window Reference |
| 11 | `pattern-generator-overview.png` | PCT Pattern Generator |
| 12 | `pattern-generator-concentric.png` | PCT Pattern Generator |
| 13 | `pattern-generator-concentric-table.png` | PCT Pattern Generator |
| 14 | `pattern-generator-stripline.png` | PCT Pattern Generator |
| 15 | `pattern-generator-chessboard.png` | PCT Pattern Generator |
| 16 | `pattern-generator-file-menu.png` | PCT Pattern Generator |
| 17 | `monitor-viewer-overview.png` | Monitor Viewer |
| 18 | `monitor-viewer-setup-direction.png` | Monitor Viewer |
| 19 | `camera-calibration-ready.png` | Camera Calibration |
| 20 | `camera-calibration-direction-diff.png` | Camera Calibration |
| 21 | `camera-calibration-histogram-compare.png` | Camera Calibration |
| 22 | `cali-result-overview.png` | Cali Result — overview |
| 23 | `cali-result-tab-strip.png` | Cali Result — overview |
| 24 | `cali-result-tab-menu.png` | Cali Result — overview |
| 25 | `cali-result-frame.png` | Cali Result — Main Window Overview |
| 26 | `cali-result-round-table.png` | Result Table View |
| 27 | `cali-result-parameter-tab.png` | Parameter View |
| 28 | `cali-result-zfl-ih-ranges.png` | Parameter View |
| 29 | `cali-result-overlap-tab.png` | Overlap & Aggregation |
| 30 | `cali-result-aggr-tab.png` | Overlap & Aggregation |
| 31 | `cali-result-graphs-tab.png` | Entrance-Pupil Analysis |
| 32 | `pct-recommend-overview.png` | PCT Recommend |
| 33 | `pct-recommend-reference.png` | PCT Recommend |
| 34 | `pct-recommend-gain-model.png` | PCT Recommend |
| 35 | `pct-recommend-series.png` | PCT Recommend |
| 36 | `reload-loaded-folder.png` | Reload Calibration Data |
| 37 | `setup-center-overview.png` | Setup Center |
| 38 | `3d-verification-overview.png` | 3D Verification |
| 39 | `3d-verification-ori-det.png` | 3D Verification |
| 40 | `database-camera-list.png` | Database Overview |
| 41 | `database-rounds-view.png` | Database Overview |
| 42 | `database-context-menu.png` | Database Overview |
| 43 | `sharepoint-credentials-dialog.png` | Load to System |

## Priority 2 — the comparison pairs

Each page lists extras under *"worth having"*. These few are the ones that teach
something no amount of prose does, because they show a **good and a bad case side
by side**:

| Pair | Files |
|---|---|
| A correct vs a wrong camera centre | `setup-center-guides-good.png`, `setup-center-guides-bad.png` |
| A smooth vs a zig-zag Overlap snake line | `cali-result-overlap-good-vs-bad.png` |
| An uneven measured PCT series vs the recommended even one | before/after of the PCT Recommend tables |
| A well-centred vs an off-centre capture | `camera-calibration-preview-good.png`, `camera-calibration-preview-bad.png` |

If capture time is short, **do these before finishing Priority 1.**

## Priority 3 — everything else

Each page's *"Screenshots this page needs"* section lists its own extras.

---

## Capture conditions

Most of these need the **rig connected and real calibration data loaded**.
Grouping them by what has to be set up saves a lot of re-setup:

| Session | Needs | Covers |
|---|---|---|
| **A — offline** | Nothing but the app | 11–16, 32–35, and the Pattern Generator / PCT Recommend extras |
| **B — rig connected, no data** | Rig up, patterns on screen | 1–10, 17–18 |
| **C — mid-calibration** | A live round with Pos and Neg shots | 19–21, 37, and the good/bad capture pair |
| **D — data loaded** | A complete calibration folder | 22–31, 36, and the Cali Result extras |
| **E — 3D target** | Two cameras and the chessboard target | 38–39 and the 3D extras |
| **F — database** | The database plus Graph credentials | 40–43 |

---

## Do not publish a real client secret

`sharepoint-credentials-dialog.png` shows the Microsoft Graph sign-in, and it has
a **Show secret** checkbox.

<!-- markdownlint-disable-next-line MD028 -->
> Leave the secret field **empty**, or type obvious placeholder text, before
> capturing. Cropping to the status line alone — `sharepoint-status-ok.png`,
> `sharepoint-status-error.png` — keeps the field out of frame entirely.

A published screenshot of a working secret is exactly the leak that moving the
credentials out of the binary was meant to prevent.

---

## Not reusable from v1.1

Some v1.1 captures are still accurate — the compute-only tabs. These are **not**:

| Window / view | Why |
|---|---|
| Main window, top area | The HTTP Server URL panel is gone; header buttons and menu moved into the `.ui` |
| Main window, Monitor / Pattern panel | Compass layout |
| Monitor Viewer, all of it | Compass layout |
| Calibration Data window | Rewritten to match the Python original |
| Cali Result, round tabs | Per-round Clean Noise dropped; star marker added |
| 3D Verification, all six tabs | `CbDetect`, `PanelCornerRecovery`, `PlaneFit3dViz`, `Point3dGlView` all changed |
