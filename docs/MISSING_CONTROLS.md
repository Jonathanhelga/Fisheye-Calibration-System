# Backend that has no button

**As of 2026-09-07**, on branch `v2.1_2026_New-UI-CPP-ROS_Wiring` (merge `8b60b33`).
**Revised 2026-09-08** — each entry now names the control it had in the old Qt
Widgets client (`cpp/`, on `v2.0_2026_main-cpp-ros`), because "what was this
called and where did the operator find it" turned out to be the fastest way to
settle both what a control should do and whether it ever existed. Three of these
have *no* old-UI counterpart, which is worth knowing before designing one.

Every control in the UI now reaches a backend service. This document is the
*other* direction: capability that exists, compiles and works, but that no
control in the app can invoke.

It was produced by walking each `Q_INVOKABLE` in `src/*.h` and looking for a QML
caller. That check is worth re-running after any wiring work, because it catches
a different class of gap than the usual one:

```bash
# backend methods with no QML caller
cd <repo>
for h in src/*.h; do
  for m in $(grep -oE 'Q_INVOKABLE [a-zA-Z0-9_:<>* ]+ [a-zA-Z0-9_]+\(' "$h" \
             | sed 's/.* \([a-zA-Z0-9_]*\)($/\1/' | grep -vE '^apply' | sort -u); do
    n=$(grep -rl "\.$m(" --include='*.qml' qml/ 2>/dev/null | wc -l)
    [ "$n" -eq 0 ] && echo "no QML caller: $(basename $h) :: $m"
  done
done
```

To find what a control was called in the old client, and where the operator found
it — `cpp/` is not in this branch, but it is one `git show` away:

```bash
git grep -n "<the C++ method>"  v2.0_2026_main-cpp-ros -- cpp/     # find the handler
git grep -n "btn_<name>"        v2.0_2026_main-cpp-ros -- cpp/     # handler -> objectName
git show v2.0_2026_main-cpp-ros:cpp/ui/cali_result.ui | grep -n QPushButton
git show v2.0_2026_main-cpp-ros:cpp/ui/<file>.ui | sed -n '<line-40>,<line+40>p'
```

The `.ui` files use absolute `geometry` rects rather than layouts at the top
level, so the surrounding `<rect>` and the sibling widgets in the same
`gridLayout` tell you what a button was grouped *with* — which is usually a
better guide to where it belongs now than the label is. Do not skip this step
and design from the method name: it is how this document ended up asserting a
wrong answer to the PCT question below.

## The four checks, and why one is not enough

Run 2026-09-08. Each catches a class the others miss, which is why the count kept
changing as checks were added rather than converging from the first one.

| | Check | Method | Result |
|---|---|---|---|
| **A** | control → backend | every clickable handler traced through panel signals and local functions to a controller call | 118 handlers, **0 dead** |
| **B** | `Q_INVOKABLE` → caller | every backend method walked against its QML callers | 77 methods, 12 unreached |
| **C** | toggle → consumer | every toggle-backed property checked for a reader | all consumed |
| **D** | writable `Q_PROPERTY` → writer | every settable property checked for a QML writer | 3 written, 1 fixed by design |

**Check D exists because check B cannot see it.** `singleDistance`,
`baseDistance`, `folder` and `fov` are set from QML by assignment, not by calling
an invokable, so a `Q_INVOKABLE` audit reports nothing about them either way. The
one with no writer is `CalibrationController::regressionDegree`, and it is
**correctly** not exposed: it is fixed at 4 because the camera parameter block
holds exactly four coefficients (c₁…c₄ → `parameter5`…`parameter2`, with
`parameter0`/`parameter1` left at 0 and c₀ dropped). A degree control would
produce a fit that does not fit the output format. Do not "wire" it.

**Check A's verdict is that no button in this app does nothing.** Every control
either reaches a controller or is legitimate view state (opening a popup,
flipping a preview flag). Two things it surfaced that are worth knowing but are
not faults:

- **Nine signals are declared and emitted that nothing handles** —
  `updateTableRequested`, `clearTableRequested`, `clearAllTablesRequested`,
  `loadExcelRequested`, `loadAllExcelRequested`, `loadDatabaseRequested`,
  `saveExcelRequested` (all `MoilCalibrationResult`), plus
  `CenteringPanel.centerChanged` and `PatternAndMonitor.fourSideBrowseRequested`.
  Every one of those buttons reaches its backend by another route — a direct
  controller call on the line above, or a `FileDialog`'s `onAccepted`. They are
  spare hooks for a parent that was never written, the same shape as
  `PatternAndMonitor.applyMappingRequested`, which *is* handled in `Main.qml`.
  Harmless; leave them.
- **A signals→handlers check passes this app completely.** Every button emits
  something. That is precisely why it is not one of the four.

Two related checks, for completeness:

- **Signals with no handler** — finds a button whose click goes nowhere.
- **Properties with no consumer** — finds a control whose handler works fine and
  writes to a property nothing ever reads. This is the one that hides: every
  control has a handler, every handler does something, and the value stops dead.
  Eight Centering controls and three Auto Update switches passed the signal check
  while being completely inert. See README section D7.

---

## The 20-range IH analysis workspace — added 2026-09-09

**The largest remaining gap, and it is a whole feature area rather than a button.**

`client_prepared` (`C:\Users\Bahri\Desktop\13082026_Nasyah_developt - Copy\…`)
carries `checkbox_enable_range_1..20`, each with `lineedit_distance_range_N`,
`alpha_min/max_N` and `aggregation_min/max_range_N`, driven by six controls this
app has no equivalent for:

| Old control | What it does |
|---|---|
| **Range Window** | loads `range_min` / `range_max` / `step` from a JSON file |
| **Aggr by Range and Distance** | aggregation for an IH range given as **percent** of `maxIctAllRounds`, at a distance |
| **Min Aggregation by Interval** | loops rounds 1–10, per-round minimum via `find_min_aggr_single_round(i, 1, 500)`, skipping OFF and empty rounds, cancellable |
| **Show Graph Dist-Alpha** | distance against α across the enabled ranges |
| **Show Graph Dist-IH-Range** | distance against aggregation across the enabled ranges |
| **Save History Distance** | writes the best distance per enabled range to a folder |

**No server op is missing** — every one of these composes ops this app already
calls. The gap is entirely client-side state and presentation.

Two semantic traps for anyone porting it:

- Our *Limit to ICT window* takes **absolute ICT pixels**; the old *Aggr by Range
  and Distance* takes a **percentage** of max ICT. An operator moving across
  enters `20` meaning 20% and gets 20 pixels.
- Our *Find Min (all rounds)* is `find_min_aggregation_in_window` — one global
  minimum. The old *Min Aggregation by Interval* gives each round **its own**
  minimum, which is how a single bad round is spotted.

Also absent, and much smaller: **Keep Round Data**, which copied the "current"
table into round N. Less needed here because our round selector includes
*Current* and Update Table writes straight to the selected round.

## Neither client measures the side panels

Found 2026-09-09 by reading `moil_cali_result.xlsx` from a real run
(`LRCP_U3JMX577_31_253_3040x3040_yuanman_Bahri_20260818\1`), which is the **old
client's own output from that folder's two captures**.

Layers 0–11 carry all eight directions. Layer 12 is the `*`, and from there down
**every one of the eight columns is blank**, to layer 74 — while the PCT column
dutifully carries `120` for all sixty-odd side layers.

The captures clearly show stripes on all four side panels running out to the frame
edge (~1500 px radius). The largest ICT in that reference table is **930 px**. So
detection stops at the top panel and the side stripes never enter the table, in
**either** client. Those rows cannot contribute to the fit.

This is not a porting defect — it is the behaviour being ported. Worth knowing
before anyone reads blank side rows as a bug in this app, and worth deciding on
separately if side data is meant to count.

## Blocking — a calibration cannot be completed without these

### 1. Fill Round from Capture — WIRED 2026-09-09

**Closed.** The toolbar's *Update Table* now does what the old client's
`btn_update_table` did: measure the pair's crossings, fill the round with them
and the PCT from the pattern panels, then recompute. `Main.qml` owns the join,
because the crossings belong to `ComputeController`, the PCT to the pattern
panels and the table to the Cali Result window, and only that window sees all
three.

Two refusals rather than plausible output: it needs a pair with a centre on each,
and it refuses a PCT of all zeros (an unopened pattern window) rather than
sending 75 of them — `pct_cal` is a running sum, so a wrong PCT is a wrong curve,
and a pattern whose side intervals are all equal makes it invisible in the table.

Note it re-measures its own nodes rather than reusing the last Direction Diff's,
because the noise-cleaning toggle deliberately refreshes only the curves now. The
old client did the same for the same reason.

The rest of this entry is kept for the reasoning, which is still the reference for
anyone touching that path.

### 1b. Original entry

| | |
|---|---|
| **Backend** | `CalibrationController::updateFromCapture(round, pct, nodes)` |
| **Service** | `/compute/cali`, op `update_table_from_capture` |
| **State** | written, compiles, **no caller** |
| **Old UI** | `btn_update_table`, labelled **"Update Table"** — *Calibration Result* window, top toolbar, directly beneath *Save to Excel*. Tooltip: "Update Tabel System from Calibration System". Handler `ControllerCaliResult::updateTable()`, which ports Python's `onclick_btn_update_table`. |
| **⚠️ Name taken** | The new UI **already has a button labelled "Update Table"** and it is a different operation. See immediately below. |

#### The label is already in use, by something else

[MoilCalibrationResult.qml:314](../qml/windows/MoilCalibrationResult.qml#L314)
has an *Update Table* button, and it calls `CalibrationController.computeAll()`
— op `compute_all`, which recomputes the derived columns from data the table
already holds. That is the old client's **`btn_update_all_cali_result`**, labelled
*"Update All Cali Result"*, tooltip "Recalculate and refresh all calibration
results using the current parameters".

The two are unrelated operations that have swapped names:

| Old client | Op | New client |
|---|---|---|
| **Update Table** (`btn_update_table`) | `update_table_from_capture` — capture fills the round | *no control* |
| **Update All Cali Result** (`btn_update_all_cali_result`) | `compute_all` — recompute from existing data | **Update Table** |

This is worse than a plain gap, because it is not silent-but-empty — it is
silent-and-plausible. An operator who knows the old client presses *Update Table*
expecting a fresh pair of shots to fill the round, gets a recompute of whatever
was already there, and nothing reports that no capture was read. On an empty
round it recomputes nothing and reads as a dead button; on a round loaded from
Excel it produces a perfectly good result that is not the one asked for.

The current tooltip — "Refill the round tables from the selected calibration
system" — makes it worse rather than better: *refill* implies data arrives, and
*from the selected calibration system* is exactly the old *Update Table*
tooltip's phrasing ("Update Tabel System from Calibration System").

#### …and the correct name is already in the UI, on another tab

Found 2026-09-08 by mapping every button in `qml/` to the backend call it makes,
rather than checking suspected controls one at a time. **Two buttons call
`computeAll()`:**

| Button | Where | Route |
|---|---|---|
| **Update Table** | Cali Result toolbar | `CalibrationController.computeAll()` |
| **Update All Cali Result** | Cali Result → **Parameter tab** ([CaliResultParameterPanel.qml:323](../qml/panels/CaliResultParameterPanel.qml#L323)) | `updateAllRequested` → [MoilCalibrationResult.qml:434](../qml/windows/MoilCalibrationResult.qml#L434) → `computeAll()` |

So the operation already has a button under its correct old name, on the tab
where its result is read. The toolbar *Update Table* is a **duplicate of it,
carrying the name of the function that is missing** — redundant and misnamed at
once. The Parameter tab's tooltip even states the sequence correctly: "Load the
rounds, press Update All Cali Result, then Save Parameters."

That removes the awkward part of the fix. Nothing has to be invented and no
vocabulary has to be taught: the toolbar button can be **dropped**, or relabelled
and left as a convenient second entry point, and either way *Update Table* is
free for the capture fill when it is wired. The two empty-state strings that name
the button in
[CaliResultGraphsPanel.qml:150](../qml/panels/CaliResultGraphsPanel.qml#L150) and
[CaliResultOverlapPanel.qml:56](../qml/panels/CaliResultOverlapPanel.qml#L56)
have to move with it.

#### The button-to-backend map is the check that found this

Both the name collision and this duplicate were invisible to the
methods → callers audit, because `computeAll` *has* a caller — two, in fact, which
is precisely the problem. Walk it the other way as well:

```
for each qml file:
    track the most recent `text: qsTr("…")`
    at each onClicked / onToggled, record  label -> Owner.method(...) calls in the body
    follow panel signals (`panel.xRequested()`) to their handler in the window
```

122 handlers across 47 QML files, and it answers a question the other direction
cannot: *does this label describe what this button does, and does anything else
already do it?* Neither of those is a wiring fault, so nothing else catches them.

The remaining four gaps were re-checked by **method name** rather than by label,
which renaming cannot hide: `updateFromCapture`, `readBrightness`, `clearSlot`
and `clearResults` appear nowhere in `qml/`. The map agrees — the only Clear
controls in the app are *Clear Table* / *Clear All Table* (`clearTable` /
`clearAllTables`, the cali table) and the Graphs tab's *Clear*
(`fetchRoundPoints(0)`, which drops the inspected round). Nothing clears a
capture slot or a detect result, and no button reads brightness back.
| **Suggested home** | Data tab, beside *Calculate Result* |
| **Enable when** | `ComputeController.hasNodes` |

The old toolbar row it sat in, left to right, for anyone deciding where this
belongs now:

```
[Select Cali System v]
[Load All Excel] [Clear Table    ] [Save to Excel] [Stop         ] [ ] Single Distance
[Load Excel    ] [Clear All Table] [Update Table ] [Load Database]
```

Note what it is grouped with: the Excel load/save pair. In the old client
*Update Table* and *Load Excel* were the two ways to fill a round, sitting side
by side, and that is the choice the operator was making at that moment.

This is the missing link between the two halves of the app. The measurement path
ends at the crossings and the calibration path begins at a filled table, and
nothing joins them — so today the Cali Result table can only be populated from
Excel.

```
Pair Shot --> Direction Diff --> nodes_8dir --> ComputeController.nodes
                                                                  |
                                               (nothing crosses this gap)
                                                                  |
                                               updateFromCapture() --> round's ICT columns
```

**The left-hand side of that diagram already runs on every Direction Diff, and the
answer is discarded.** Verified 2026-09-08: `Main.qml`'s `runDirectionDiff()` calls
`histogram8Dir` *and* `nodes8Dir`. The nodes reply is parsed into `nodes_` and
`nodesChanged` is emitted
([ComputeController.cpp:272](../src/ComputeController.cpp#L272)) — and
**`ComputeController.nodes` and `hasNodes` have no QML reader anywhere.** The only
visible effect is the toast counting crossings.

Do not confuse this with the histogram panel's
`ComputeController.histogram["nodes"]`, which is a sub-field of the *histogram*
op's reply. Different ops, different data; the panel reading one says nothing
about the other.

So the rig is already doing the 8-direction detection, already sending the
crossings, and the client is already parsing them into the shape
`updateFromCapture(round, pct, nodes)` wants. The gap is narrower than this
document implied: not the measurement, only the button that hands the nodes and
the PCT list to the table. Whoever wires it should read `ComputeController.nodes`
rather than triggering a fresh `nodes_8dir` — a second detection would re-measure
the same slots and cost a full pass on the rig for data already in hand.

The server side is substantial and already done: it clears the round, rewrites
the Side column, and lays the eight ICT columns out **by ring** so that row *k*
in the N column and row *k* in the E column are the same ring. See
`CaliComputePipeline.cpp`.

Three inputs are needed. Two are obvious:

- **round** — the Data tab's round selector
- **nodes** — `ComputeController.nodes`, after a Direction Diff
- **PCT list** — answered 2026-09-08, see below

#### Where the PCT list comes from — SETTLED, and not what this document guessed

This section used to say the list was "the concentric pattern's layer radii".
**That is wrong, and wrong in a way that would not have shown up.** The old
client's `ControllerPatternGenerator::pctList()` is:

```cpp
constexpr int kConcentricLayers = 25;
constexpr int kStripelineLayers = 50;

QVector<QString> ControllerPatternGenerator::pctList() const {
    QVector<QString> out;
    for (int i = 1; i <= kConcentricLayers; ++i)   // lineedit_radius_<i>
        out.append(...);
    for (int i = 1; i <= kStripelineLayers; ++i)   // lineedit_interval_stripeline_<i>
        out.append(...);
    return out;
}
```

**75 entries: 25 concentric radii, then 50 stripeline intervals**, concatenated
in that fixed order — which is exactly the table's 75 layer rows. Both patterns'
values go in every time, regardless of which one is currently on the glass,
because the table needs both.

`CaliCompute::updateTableFromCapture` splits them at the `*` row:

```cpp
// PCT column. pctList = 25 concentric (TOP) + 50 stripeline (SIDE). Rows above
// the "*" take the TOP pattern in order; from the "*" (sideStartLayer) down
// they take the SIDE pattern, so the side pattern's line 1 lands on the "*" row.
const int src = (sideStartLayer >= 0 && layer >= sideStartLayer)
                    ? kTopCount + (layer - sideStartLayer)
                    : layer;
```

Rows above the `*` are the top panel and read entries 0–24; rows from the `*`
down are the **side** panel and read `25 + (layer - sideStartLayer)`. So had this
been wired from the concentric radii alone, every side row would have paired its
ICT with a concentric radius — and `pct_cal` is a **running sum**, so the error
compounds down the column rather than staying local. The comment at step 2 of
that function records the same class of bug from a different cause: one node
misplaced at the head of the side segment credited the first real side ring with
three stripes of pattern distance instead of one, and "invisible in the table,
because this pattern's 50 side intervals are all 150."

So a wrong PCT list here is a wrong calibration that nothing reports, and the
uniform side intervals mean an off-by-N is *invisible by inspection*. Send both
lists, in this order, or do not send.

What is still open is only the plumbing: the pattern lives in the *Pattern &
Monitor* window and the table in *Cali Result*, so the button reaches across two
top-level windows — or the values are captured at shot time and carried with the
frame. The old client took the first route, via a provider callback installed by
the main controller:

```cpp
caliWin_->setPctListProvider([this] {
    return patternWin_ ? patternWin_->pctList() : QVector<QString>();
});
```

Worth noting how that fails: `patternWin_` null, or a layer field left blank,
both degrade to `"0"` per entry without a word. A QML port should say the pattern
window has not been opened rather than fill the round with zeros.

### 2. Per-round enable checkboxes — WIRED 2026-09-08

| | |
|---|---|
| **Backend** | `CalibrationController::setRoundEnabled(round, enabled)` |
| **State** | **wired** — ten checkboxes in the Data tab, `Repeater { model: 10 }` |
| **Old UI** | **not a button.** Right-click a round tab in *Calibration Result* → the menu carrying "Show ZFL-IH Graph (Round N)" and "Show Overlap Graph (Round N)". `ControllerCaliResult::toggleRoundEnabled`, porting Python's `show_round_context_menu` + `toggle_round_enabled_status`. |

The old client greyed the whole round table and appended ` [OFF]` to the tab
text, then recomputed everything. Checkboxes were chosen here instead because a
right-click menu is undiscoverable, and this setting silently decides what four
different computations are averaging — see below. If the greying is wanted back,
the state to bind is `CalibrationController.roundEnabled`.

The rest of this entry is why it mattered, kept because it explains what the
setting actually controls:

`round_enabled` already travels with **every** request — the compute node has no
session, so a stale copy would silently include or drop a whole round. The server
uses it to decide which rounds count in:

- `aggregation_all_rounds_by_distance`
- `ih_alpha_regression`
- `global_ict_alpha` — the pooled Overlap plot
- all three `CaliJob` distance searches

Until 2026-09-08 it was hardcoded to all-true with no way to change it, so "use
rounds 1–5 only" was impossible and the Aggregation tab silently included every
round whether or not it was any good.

---

## Useful — backend exists, no button

| Control | Backend | Old UI | Why it is worth having |
|---|---|---|---|
| **Read** per monitor slot | `MonitorController::readBrightness()` | **none** — the old client was write-only too. `lineedit_brightness_<dir>` was pushed by that direction's **Update** button in *Monitor Viewer*; nothing ever asked the panel what it was at. | ⚠️ **Not a convenience — this one is a live bug.** See below. |
| **auto_center** | `ComputeController::autoCenter()` — unreachable since 2026-09-09, when Find Pos / Find Neg were removed. Auto now takes the middle of the frame and Manual seeds `roi_exact`, so nothing calls the cascade. `CenteringPanel.findCenter()` and its `onCenterFound` / `onCenterRefused` handlers are intact and dormant: restoring it is one button. The op itself is untouched on the rig and specified in `doc/auto_center_design.md`. |
| **Per-axis Stop** | `AxisController::stopAxis(axis)` | `btn_stop_x`, `btn_stop_y`, `btn_stop_z`, `btn_stop_pitch`, `btn_stop_yaw` — five buttons labelled **"Stop"**, one beside each axis's jog cluster in the *Main window*. There was no Stop All; `btn_all_home` was the only whole-rig button. | Only *Stop All* is exposed. Stopping one axis mid-jog needs this. |
| **Disconnect** | `AxisController::disconnectFromRig()` | **none** — the old client had no connect or disconnect control at all. | There is *Update* to connect and nothing to drop the link deliberately. |

The old rig panel is the reverse of this one: five per-axis Stops and no Stop
All, against our one Stop All and no per-axis. Both halves exist in the backend;
this is a layout decision, not a wiring one.

### Read brightness: the missing button leaves a dead signal and a stuck indicator

`brightnessRead` is emitted from exactly one place
([MonitorController.cpp:262](../src/MonitorController.cpp#L262)), reached only
from `readBrightness`'s reply. The `KindBrightnessSet` case in `applyCommand`
does nothing. **Nothing calls `readBrightness`, so the signal can never fire.**

Two panels nevertheless handle it —
[MonitorSlotPanel.qml:74](../qml/panels/MonitorSlotPanel.qml#L74) and
[DpadMonitorViewer.qml:97](../qml/panels/DpadMonitorViewer.qml#L97) — and in
`MonitorSlotPanel` it is the **only** writer of `appliedBrightness`:

```qml
property real appliedBrightness: 5              // line 29, never updated

readonly property bool pendingChanges: root.on
    && (root.imagePath !== root.appliedImagePath
        || root.brightness !== root.appliedBrightness)
```

`onImageShown` updates `appliedImagePath` but not `appliedBrightness`. So the
unsaved-changes indicator compares the typed brightness against the literal `5`
forever: a slot set to anything else reads "unsaved changes" permanently and
Update never clears it. That is exactly the confusion the comment at line 23 says
the property exists to prevent — an un-pushed edit and a pushed one looking
identical — arrived at from the other side.

Two fixes, and they are not equivalent:

1. **`appliedBrightness = root.brightness` in `onImageShown`.** Matches how
   `appliedImagePath` is handled, no extra round trip. Optimistic: `show_pattern`
   can succeed while the panel refuses `set_brightness` over DDC/CI, and the
   indicator would then clear on a brightness that never landed. The old client
   reported those two outcomes separately for this reason — see the four distinct
   messages in `controller_monitor_viewer.cpp` `updateDir()`.
2. **Add the Read button** and call `readBrightness(direction)` after a
   successful show. Truthful, costs a round trip, and is evidently what the panel
   was designed around.

Whichever is chosen, note that `MonitorSlotPanel.qml` has collided on every merge
with `v2.1_2026_New-UI-CPP-ROS` so far — agree the change before making it.

---

## Housekeeping — low value

| Control | Backend | Old UI |
|---|---|---|
| **Clear** per capture slot | `CameraController::clearSlot(slot)` | **none.** `btn_clear_table` / `btn_clear_all_table` existed but clear the *cali table*, not a capture — different thing, similar name. |
| **Clear results** | `ComputeController::clearResults()` | **none** |

Re-capturing into a slot overwrites it, so both Clears only matter for making
something *empty*. Low value stands.

### `HttpServerProbe::urlFor` is not a gap — removed 2026-09-08

It was listed here as "pre-existing, never had a caller". It has one:
[HttpServerProbe.cpp:93](../src/HttpServerProbe.cpp#L93) builds every probe
request with it. The audit script at the top of this document looks for a **QML**
caller, so a method used only from C++ shows up as a gap when it is not one.
Check for internal callers before adding an entry.

---

## Covered by a coarser control — a button would add granularity, not capability

Found 2026-09-08 by asking, for each entry, whether the new UI reaches the same
backend by another route.

| Control | Already reached by | What is actually missing |
|---|---|---|
| **Per-axis Stop** | `AxisController::stopAll()` — [AxisController.cpp:1479](../src/AxisController.cpp#L1479) is `for (AxisState *state : d_->all) stopAxis(state->name())` | Only the granularity. Stop All stops the axis you meant and four you did not. |
| **Disconnect** | the Server panel's **Update** — `connectTo()` calls `teardownSession()` before rebuilding ([AxisController.cpp:791](../src/AxisController.cpp#L791)) | Only reaching the `Disconnected` *state* deliberately. The teardown path itself runs on every Update. |

Neither is worth a button on its own. Both are worth knowing before someone
"fixes" a gap that is already covered.

---

## Not gaps

Methods the audit lists that are wired, just not from QML:

| Method | Called from |
|---|---|
| `MonitorController::showPrepared` | `CameraController`, for the Pos / Neg / Pair shot sequence |
| `PatternController::refreshDirection` | `PatternController::applyShow`, after a pattern reaches a screen |
| `MonitorController::showImage(QUrl)` | redundant overload of `showImagePath`; QML uses the path form |
| `AxisController::axesInGroup` | helper used by the home/limit paths |

---

## Standing caveat

Nothing in this app has been exercised against a real rig. Everything marked
"works" means it compiles, passes `qmllint`, loads, binds, and the control
reaches its controller. **No ROS service call has been proven end-to-end** — not
`/camera/capture`, not `/compute/detect`, not `/compute/cali`, not `/monitor/*`,
and in particular not the `CaliJob` action, whose client has never had a goal
accepted by a real server.

The cheapest end-to-end exercise when the rig is next available is the
Aggregation tab's **Find Min (this round)**: it is one goal, it reports progress,
and it can be cancelled.
