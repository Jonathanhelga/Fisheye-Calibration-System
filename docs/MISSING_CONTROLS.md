# Backend that has no button

**As of 2026-09-07**, on branch `v2.1_2026_New-UI-CPP-ROS_Wiring` (merge `8b60b33`).

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

Two related checks, for completeness:

- **Signals with no handler** — finds a button whose click goes nowhere.
- **Properties with no consumer** — finds a control whose handler works fine and
  writes to a property nothing ever reads. This is the one that hides: every
  control has a handler, every handler does something, and the value stops dead.
  Eight Centering controls and three Auto Update switches passed the signal check
  while being completely inert. See README section D7.

---

## Blocking — a calibration cannot be completed without these

### 1. Fill Round from Capture

| | |
|---|---|
| **Backend** | `CalibrationController::updateFromCapture(round, pct, nodes)` |
| **Service** | `/compute/cali`, op `update_table_from_capture` |
| **State** | written, compiles, **no caller** |
| **Suggested home** | Data tab, beside *Calculate Result* |
| **Enable when** | `ComputeController.hasNodes` |

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

The server side is substantial and already done: it clears the round, rewrites
the Side column, and lays the eight ICT columns out **by ring** so that row *k*
in the N column and row *k* in the E column are the same ring. See
`CaliComputePipeline.cpp`.

Three inputs are needed. Two are obvious:

- **round** — the Data tab's round selector
- **nodes** — `ComputeController.nodes`, after a Direction Diff
- **PCT list** — ⚠️ **open question, see below**

#### Open question: where does the PCT list come from?

`updateTableFromCapture` takes `pctList` as a `QVector<QString>` — one entry per
layer. The likely source is the **concentric pattern's layer radii**, in layer
order, since that is what was physically on the glass when the pair was shot.

This needs confirming before it is wired. Writing the table off the wrong
reference produces a plausible-looking result and a wrong calibration, and
nothing downstream would flag it.

Also unresolved if the radii are correct: the pattern lives in the *Pattern &
Monitor* window and the table lives in *Cali Result*, so the button has to reach
across two top-level windows — or the radii have to be captured at shot time and
carried with the frame.

### 2. Per-round enable checkboxes

| | |
|---|---|
| **Backend** | `CalibrationController::setRoundEnabled(round, enabled)` |
| **State** | written, compiles, **no caller**; no QML reads `roundEnabled` either |
| **Suggested home** | a checkbox row under the Data tab's round selector, or beside the Graphs tab legend |

`round_enabled` already travels with **every** request — the compute node has no
session, so a stale copy would silently include or drop a whole round. The server
uses it to decide which rounds count in:

- `aggregation_all_rounds_by_distance`
- `ih_alpha_regression`
- `global_ict_alpha` — the pooled Overlap plot
- all three `CaliJob` distance searches

It is currently hardcoded to all-true with no way to change it. So "use rounds
1–5 only" is impossible, and the Aggregation tab silently includes every round
whether or not it is any good.

---

## Useful — backend exists, no button

| Control | Backend | Why it is worth having |
|---|---|---|
| **Read** per monitor slot | `MonitorController::readBrightness()` | Brightness is write-only today: the spin box shows what was last typed, not what the monitor is at. The rig is the authority and the handler that accepts its answer is already wired — there is just no way to ask. |
| **Per-axis Stop** | `AxisController::stopAxis(axis)` | Only *Stop All* is exposed. Stopping one axis mid-jog needs this. |
| **Disconnect** | `AxisController::disconnectFromRig()` | There is *Update* to connect and nothing to drop the link deliberately. |

---

## Housekeeping — low value

| Control | Backend |
|---|---|
| **Clear** per capture slot | `CameraController::clearSlot(slot)` |
| **Clear results** | `ComputeController::clearResults()` |
| **Open in browser** | `HttpServerProbe::urlFor(port)` — pre-existing, never had a caller |

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
