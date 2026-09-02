---
id: parameter-view
slug: /calibration/cali-result/parameter-view
title: Parameter View
---

# Parameter View

The `parameter` tab — where a calibration turns into the numbers you actually
ship. Two graphs, a camera-parameter panel, and the button that writes it all to
a JSON file.

<Shot
  id="fig-1"
  number="1"
  file="cali-result-parameter-tab.png"
  caption="The parameter tab with both graphs drawn and the parameter panel filled."
  what="Load a full calibration, run Update All Cali Result, then capture the parameter tab so both the IH-Alpha and ZFL-IH plots have points and the parameter0–5 fields hold fitted values."
  height={540}
/>

---

## 1. The IH-Alpha graph

**X axis:** `ALPHA (degree)`, 0–90 · **Y axis:** `IH (pixel)`, 0–2000

This is the calibration curve. It plots, for every layer of every enabled round,
the field angle against the image height where that angle lands.

| What you see | What it is |
|---|---|
| Coloured scatter | One colour per round — see [§6](#6-round-colours) |
| **White curve** | A **degree-4 polynomial regression** fitted across all enabled rounds |
| Title | `IH-Alpha (Round: 1, 3, 5)` — which rounds contributed, or `(No Data)` |

**Update IH Alpha Graphics** (`btn_update_plot_ih_alpha`) redraws it.

<div className="custom-note custom-danger">
  <div className="custom-note-title">⚠️ REDRAWING THIS GRAPH REWRITES parameter0 … parameter5</div>
  <div>
    The fit is not just drawn — it is <strong>written into the six parameter fields</strong>. All six are reset to <code>0</code>, then four are filled from the degree-4 fit. Anything you typed into them by hand is <strong>lost the next time this graph redraws</strong>, which includes every Update All Cali Result.
  </div>
</div>

### How the coefficients map

The polynomial is `[c0, c1, c2, c3, c4]`. It is written **in reverse**, and the
constant term is dropped:

| Field | Gets |
|---|---|
| `parameter5` | `c1` |
| `parameter4` | `c2` |
| `parameter3` | `c3` |
| `parameter2` | `c4` |
| `parameter1` | stays `0` |
| `parameter0` | stays `0` |

`c0` is discarded because a zero field angle must map to zero image height —
a non-zero constant would be physically impossible.

**Information — IH-Alpha polynomial** (`btn_param_info`) explains the fit in the
application itself.

---

## 2. The ZFL-IH graph

**X axis:** `ICT (pixel)`, −2500 … 2500 · **Y axis:** `ZFL (pixel)`, −600 … 3000

Focal length against intersection radius, again one scatter colour per round.

**Update IH ZFL Graphics** (`btn_update_plot_ih_zfl`) redraws it.

### The coloured range bands

<Shot
  id="fig-2"
  number="2"
  file="cali-result-zfl-ih-ranges.png"
  caption="ZFL-IH with several IH range bands shaded."
  what="Fill in a few of the 20 range min/max fields — or use Range Window with a JSON — then capture the ZFL-IH plot so the translucent coloured bands are visible over the scatter."
  height={400}
/>

Translucent vertical bands mark the IH ranges from the
[Aggr by Distance and Range](./overlap-and-aggregation-view.md) tab. Each range's
**min% and max% are converted to pixels** against the largest ICT in the data,
and drawn in one of five rotating colours — red, orange, yellow, green, blue.

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ EVERY RANGE WITH VALID NUMBERS IS SHADED — NOT JUST THE TICKED ONES</div>
  <div>
    The enable checkbox controls whether a range takes part in the <em>computation</em>. It does not control whether the band is drawn. A range you unticked is still shaded here, which reads as "in use" when it is not.
  </div>
</div>

A range whose min is greater than its max is swapped rather than rejected.

---

## 3. Reading cursor coordinates

Both plots — and the Overlap and Aggregation plots — write the cursor position
into a pair of read-only fields as you move over them:

| Plot | Fields |
|---|---|
| IH-Alpha | `lineedit_alpha_x`, `lineedit_alpha_y` |
| ZFL-IH | `lineedit_zfl_x`, `lineedit_zfl_y` |
| Overlap | `lineedit_overlap_x`, `lineedit_overlap_y` |
| Aggregation vs. Distance | `lineedit_dist_vs_aggr_x`, `lineedit_dist_vs_aggr_y` |

They clear when the cursor leaves the plot. Values are shown to 2 decimal places.

---

## 4. The camera parameter panel

Sixteen fields, written into the JSON by **Save Parameters**.

| Field | JSON key | Type |
|---|---|---|
| Camera name | `cameraName` | text |
| Camera FoV | `cameraFov` | integer |
| Sensor width / height | `cameraSensorWidth` / `cameraSensorHeight` | integer |
| **iCx / iCy** | `iCx` / `iCy` | integer |
| Image width / height | `imageWidth` / `imageHeight` | integer |
| Ratio | `ratio` | float |
| Calibration ratio | `calibrationRatio` | float |
| **parameter0 … parameter5** | `parameter0` … `parameter5` | float |

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ AN UNPARSEABLE FIELD BECOMES ZERO, SILENTLY</div>
  <div>
    Every numeric field is read with a plain conversion and <strong>falls back to <code>0</code> on failure</strong>. A typo, a stray unit, or a decimal comma does not produce a warning — it produces a zero in the saved file, and a camera model that is quietly wrong. Check the JSON after saving.
  </div>
</div>

### Save Parameters

**Save Parameters** (`btn_save_parameter`) asks for a path and writes the JSON,
indented.

This is the file [Setup Center](../../verification/setup-center.md) reads and
writes back — so the round trip is: calibrate here → save → verify the centre
there → save the corrected `iCx`/`iCy` → reload here if you need to.

Failure: *"Cannot write [path]"*. Success: *"Saved to: [path]"*.

---

## 5. Calibration System and distance per round

| Control | Object name | What it does |
|---|---|---|
| System combo | `combox_type_of_system` | Picks the rig profile, e.g. `Yuanman - SIDE (EV2785)`. Applies that system's pixel sizes and h/v gaps |
| Distance / round | `lineedit_distance_round` | Physical spacing between rounds |
| **Save Configuration** | `btn_save_configuration_system` | Writes `main.json` |

`main.json` is two keys:

```json
{
  "systemType": "Yuanman - SIDE (EV2785)",
  "distance_per_round": 20
}
```

**This is the file Load All Excel reads back**, so saving it beside your round
folders makes the whole calibration reloadable with the right system and spacing
already selected. See
[Reload Calibration Data](../reload-calibration-data.md).

An unparseable distance is rejected properly here: *"Distance '…' not valid."*

---

## 6. Round colours

The scatter colour identifies the round on every plot in the window:

| Round | Colour | | Round | Colour |
|---:|---|---|---:|---|
| 1 | 🔴 red | | 6 | purple |
| 2 | 🟡 yellow | | 7 | dark red |
| 3 | 🔵 cyan | | 8 | olive |
| 4 | 🔵 blue | | 9 | dark yellow-green |
| 5 | 🩷 light red | | 10 | grey-green |

Round 0 — the `current` tab — is white.

---

## 7. Update All Cali Result

The header button that recomputes rounds 1–10 and redraws **all** of ZFL-IH,
IH-Alpha and Overlap, then refreshes the range data totals. It also sets the
global IH range: Min `0`, Max the largest ICT across every round.

> Because it redraws IH-Alpha, **it also rewrites parameter0–5**. Save your
> parameter JSON before pressing it if those fields hold anything you edited.

With nothing loaded it returns immediately rather than running the slow path.

---

## Recommended workflow

1. Load or capture every round.
2. **Update All Cali Result.**
3. Check the IH-Alpha scatter is smooth and the white curve follows it. Outliers
   here mean a bad round — turn it off from its tab.
4. Set the ranges you care about, and read the ZFL-IH bands.
5. Fill in camera name, FoV, sensor size, image size, ratios.
6. Copy `iCx`/`iCy` in — or leave them and fix them in Setup Center.
7. **Save Parameters.**
8. **Open the JSON and check it.** Zeros where you expected numbers mean a field
   did not parse.
9. Verify the centre in [Setup Center](../../verification/setup-center.md).
10. **Save Configuration** so the folder reloads cleanly later.

---

## Troubleshooting

| Symptom | Cause |
|---|---|
| Both graphs empty, title `(No Data)` | No round has computed data. Run **Update All Cali Result** |
| parameter0–5 keep resetting | Every IH-Alpha redraw rewrites them. That is the design |
| parameter1 and parameter0 always `0` | Correct — only four coefficients are written |
| A saved parameter is `0` unexpectedly | That field did not parse. No warning is given |
| Range bands showing for ranges you unticked | Expected — the checkbox controls computation, not drawing |
| Scatter has one obviously wrong colour group | That round is bad. Turn it off from its tab and recompute |
| *"Distance '…' not valid."* | The distance-per-round field is not a number |

---

## Screenshots this page needs

| Figure | File | What to capture |
|---|---|---|
| 1 | `cali-result-parameter-tab.png` | Parameter tab, both graphs drawn, fields filled |
| 2 | `cali-result-zfl-ih-ranges.png` | ZFL-IH with range bands shaded |

Two more worth having:

| File | What to capture |
|---|---|
| `cali-result-ih-alpha-fit.png` | IH-Alpha cropped tight — coloured scatter plus the white degree-4 curve |
| `cali-result-param-info.png` | The **Information — IH-Alpha polynomial** dialog |

---

## Source material

| What | Where |
|---|---|
| Layout | `cpp/ui/cali_result.ui`, `parameter` tab |
| Behaviour | `cpp/src/controllers/controller_cali_result.cpp` |
| Regression and series | `cpp/src/core/cali/Regression.cpp`, `CaliCompute.cpp` |
| Round colours | `CaliCompute::roundColor` |
| Rig profiles | `cpp/config/cali_system/*.json` |
