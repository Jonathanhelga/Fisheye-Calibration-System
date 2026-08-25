# Moil Calibration Result panel, a QML design pass

## Context

The old QWidget "Moil Calibration Result" screen is one of two panels in this rebuild that still sit behind an inert `ActionButton` placeholder in `qml/windows/Main.qml`.
Reference implementation: `cpp/ui/cali_result.ui` and `cpp/src/controllers/controller_cali_result.cpp` in `../moil-fisheye-calibration-system`.

A screenshot of the old panel surfaced several real usability problems worth fixing in the rebuild, not just porting as-is.
Destructive "Clear Table" buttons are visually indistinguishable from routine actions.
Thirteen tabs are used to represent what is structurally eleven writable round slots plus four unrelated views, with a leftover `test` tab that looks like debug cruft.
Calibration formulas float as loose text inside the data grid instead of living somewhere a reader would expect documentation.
Column tinting is the only structural cue across a table with more than thirty columns, and it is too subtle to actually help.

This plan covers a full redesign of the **Data** view, plus placeholder shells for the other four views, so the panel gets a complete navigational shell without committing yet to porting the compute engine, plotting, or range-search UI.
Per the project's established workflow, this is QML layout, properties, and signals only.
No C++/Bridge wiring, no real Excel or database loading, no alpha/ZFL math.
All data is placeholder, the same way axis readouts elsewhere in the app sit at "?" until backend work happens, and that backend work is something Jonathan is building himself rather than delegating.

## Reference semantics, confirmed from the old codebase

Confirmed from `CaliCompute.cpp`, `controller_cali_result.cpp`, and `cali_result.ui` in the reference repo, not assumed.

The "current" tab plus `round_1` through `round_10` are eleven independently loadable and editable tables, where current is round 0.
They are not one dataset filtered eleven ways, each round can hold entirely different capture data, and a `*` marker on a tab means that round has data in it.

Every round table shares the same column order, grouped exactly as the reference groups them, with three visual dividers marking the group boundaries:

1. Raw, editable group: `Round`, `Side`, `PCT (mm)`, then `ICT N / S / W / E / NW / SE / SW / NE (px)`, eleven columns total.
2. A divider.
3. Computed core: `ICT avg (px)`, `PCT cal (mm)`, `Distance (mm)`.
4. A divider.
5. Computed, per direction, interleaved rather than grouped by metric: `alpha N, ZFL N, alpha S, ZFL S, alpha W, ZFL W, alpha E, ZFL E, alpha SE, ZFL SE, alpha NW, ZFL NW, alpha SW, ZFL SW, alpha NE, ZFL NE`.
6. A divider.
7. Computed averages: `alpha avg`, `ZFL avg`.

For readers who want the plain-language version of what PCT and ICT actually mean physically, see `PCT_AND_ICT.md` in this repo, PCT is the known, hand-designed pattern distance, ICT (also called IH) is what the camera actually measured in pixels, in eight directions, after the fisheye lens bent it.

The formulas shown in the old UI, kept here for the info-popup copy rather than for implementation:
`alpha = atan(PCT / distance)` for the top monitor,
`alpha = pi/2 - atan[(distance - PCT_V_Gap) / H_Gap]` for the side monitor,
`ZFL = 1/tan(alpha) * image_height`.

The "Select Cali System" dropdown is a static four-item list in the reference: "Yuanman - SIDE (EV2785)", "Yuanman - SIDE (EV2730Q)", "Yinda", "Broland C++".

"Cali Folder" is a filesystem path field, backed by a folder browser in the reference app.
A plain text field is enough here for now, since no backend loading happens yet.

## Approach

### 1. New window: `qml/windows/MoilCalibrationResult.qml`

Follow `qml/windows/PatternAndMonitor.qml`'s shape exactly.
A plain `Window`, not an `ApplicationWindow` or `Popup`, sized to `Screen.desktopAvailableWidth` / `desktopAvailableHeight`, with no internal `visible` override, visibility stays owned entirely by the caller, the same way `patternAndMonitor.visible` works today.

Top to bottom, the window holds:

- A session bar: the Cali Folder text field, the Select Cali System combo box with the four hardcoded options above, and a `StatusDot`.
- An action bar: routine actions such as `Load All Excel`, `Load Excel`, `Save to Excel`, `Update Table`, `Load Database`, and `Stop`, as `ActionButton` with `tone: "neutral"` or `"accent"`; destructive actions `Clear Table` and `Clear All Table` as `tone: "danger"`, visually grouped apart from the routine row, for example right-aligned in their own cluster, so a misclick cannot land on them straight from "Save" or "Update"; a `Single Distance` checkbox alongside.
- A view switcher: a `SegmentedControl` with five entries, Data, Parameters, Overlap, Aggregation, and Graphs, each backing panel instantiated as a sibling and toggled with `visible: root.viewIndex === N`, the same all-instances-alive pattern `PatternAndMonitor.qml` already uses for its three sub-panels.

### 2. New panel: `qml/panels/CaliResultDataPanel.qml`, the full-detail Data view

A round selector replaces the old thirteen-tab bar: a `SegmentedControl` with eleven entries, `current` then `1` through `10`.
The `test` tab is dropped entirely, since it reads as leftover debug scaffolding in the reference, not a real user-facing view.
Each entry switches which round's row-data array feeds the table below.

A sub-toolbar holds `pos_iCx`, `pos_iCy`, `neg_iCx`, `neg_iCy` as a `LabeledField` / `ValueField` group, an `Aggr Round N` button, a `Clean Noise` button, an `Aggregation` field, a `Distance` field, and a `Calculate Result` button right-aligned.

The data table follows the pattern already established in `ConcentricPanel.qml` and `StripelinePanel.qml`, precisely:

- A `QtObject { id: tableColumns }` holding one width property per column, shared by the header row and every row delegate so header and rows can never drift apart from each other, see `ConcentricPanel.qml:230-238`.
- A bold-caption header `RowLayout` plus a one-pixel `Theme.panelBorder` divider, see `ConcentricPanel.qml:246-262`.
- A `ListView` with `clip: true`, `boundsBehavior: Flickable.StopAtBounds`, and a `ScrollBar.vertical`, whose delegate alternates row background using `cell.index % 2`, see `ConcentricPanel.qml:264-293`.
- The three column-group boundaries render as a single thin vertical rule each, not full-cell tinting, with light zebra striping carrying row readability instead.
- `Theme.statusOk` / `statusFailed` / `statusPartial` are reserved for cells that are actually out of tolerance later, not applied now since there is no real computation to compare against yet, but the column structure is built so per-cell coloring can hang off it once that computation exists.

The three formula strings move out of the grid into a small info popup opened from a button next to `Calculate Result`, instead of permanently occupying floating text beside a thirty-plus column table.

### 3. New control: `qml/controls/CaliResultRow.qml`

One table row delegate, following `ConcentricLayerRow.qml` and `StripelineLayerRow.qml` exactly.
A `required property` per column: round, side, pct, the eight ICT values, ictAvg, pctCal, distance, the eight alpha values, the eight ZFL values, alphaAvg, and zflAvg.
Column-width properties are passed in from the parent panel's `tableColumns`.
`ValueField` renders the editable group-one cells, plain `Label`s render the computed cells, and `edited(...)` signals fire per editable field, mirroring `intervalEdited` and `colorEdited` from the existing row components.

### 4. Placeholder shells for the other four views

`qml/panels/CaliResultParametersPanel.qml`, `CaliResultOverlapPanel.qml`, `CaliResultAggregationPanel.qml`, and `CaliResultGraphsPanel.qml`, at the same weight as the existing `PanelPlaceholder`.
Each gets a title, a short description, and the reference app's relevant action buttons rendered inert.
Parameters gets the six coefficient `LabeledField`s plus a `Save Parameters` button.
Overlap and Graphs each get their `Update ___` button or buttons plus an empty chart-shaped placeholder area.
Aggregation gets its `Update` / range-search button plus a placeholder note.
No plotting, no range-search grid, and no database browser go into this pass, those are explicitly out of scope for now.

### 5. Wire into `qml/windows/Main.qml`

Instantiate `MoilCalibrationResult { id: moilResult }` near the existing `PatternAndMonitor` instance, around `Main.qml:199-206`.
Give the existing inert "Moil Calibration Result" `ActionButton`, around `Main.qml:168-172`, `checked: moilResult.visible` and `onClicked: moilResult.visible = !moilResult.visible`, mirroring the Pattern & Monitor button at `Main.qml:165-166` exactly.

### 6. Register the new files in `CMakeLists.txt`

Add to the existing `QML_FILES` list at `CMakeLists.txt:25-67`.
`qml/windows/MoilCalibrationResult.qml` goes under the windows group, the five new panels go under the panels group, and `CaliResultRow.qml` goes under the controls group.

## Explicit non-goals for this pass

No C++ or Bridge wiring, no real Excel or database loading, no alpha/ZFL computation, placeholder data only.
No plotting engine for Overlap or Graphs, that would extend something like `HistogramPlotView.qml` later.
No Aggregation range-search grid, the twenty-one row Global-plus-Range table from the reference.
No Load Database sub-browser, the button exists but stays inert.
The reference `test` tab is dropped, not ported.

## Verification

This is a QML-only change with no backend behavior to exercise.
Registering the files in `CMakeLists.txt` is enough for the next build to pick them up.
Building, running, and visual review stay with Jonathan, per the existing project workflow, rather than launching the app for self-verification.
