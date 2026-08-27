# QML lint debt

Snapshot of every `qmllint` finding in the repo.
First taken 2026-08-25 right after `CaliResultParameterPanel.qml` landed, and re-verified unchanged on 2026-08-26.
None of these were introduced by that panel: `CaliResultParameterPanel.qml` and `HistogramPlotView.qml` both come back clean.

Reproduce with:

```bash
# macOS
/opt/homebrew/bin/cmake --build build --target all_qmllint

# miniPC
cmake --build build --target all_qmllint
```

Current totals, by lint category:

| Category | Count |
|---|---|
| `[unqualified]` | 10 |
| `[Quick.layout-positioning]` | 6 |
| `[missing-property]` | 4 |
| `[property-override]` | 1 |
| `[unused-imports]` | 1 |

The build itself is green.
`qmllint` is stricter than the compiler, so nothing here breaks the build today.
Two of the findings are real bugs, four are latent hazards, and three are false positives that the linter cannot resolve.

## Real bugs

### Two dead buttons in ChessboardPanel

`qml/panels/ChessboardPanel.qml:71-72`

```qml
GhostButton { text: qsTr("Import"); onClicked: panel.importRequested() }
GhostButton { text: qsTr("Export"); onClicked: panel.exportRequested() }
```

The panel declares only three signals at lines 29-31: `generateRequested()`, `saveImageRequested()`, and `updateRequested(string direction)`.
Neither `importRequested` nor `exportRequested` exists, so both buttons are inert.
Clicking them produces a runtime TypeError in the console and nothing else.

The fix is to decide whether these buttons are meant to work.
If yes, declare the two signals on the panel and handle them at the window.
If they were speculative, delete them so the UI does not offer an action it cannot perform.

### PatternColorButton shadows a built-in property

`qml/controls/PatternColorButton.qml:13`

```qml
readonly property var palette: ["#000000", "#ffffff", "#b4b4b4"].concat(Theme.curvePalette)
```

`palette` is a real property on `QQuickItem`, carrying the control's colour group.
Redefining it as an array of colour strings means any child that reads `palette.window` or similar gets an array instead.
It also silently changes what `palette` means for anything that inherits from this control later.

Rename it to something like `swatches` or `choices`.

## Latent hazards

### Separators sized with `height` inside a layout

Four panels place a 1px divider `Rectangle` inside a `ColumnLayout` and set `height: 1` directly.

| File | Line |
|---|---|
| `qml/panels/CaliResultDataPanel.qml` | 455 |
| `qml/panels/ConcentricPanel.qml` | 94, 260 |
| `qml/panels/StriplinePanel.qml` | 98, 259 |
| `qml/panels/ChessboardPanel.qml` | 78 |

A layout owns the geometry of its children, so writing `height` directly is undefined behaviour.
It happens to render correctly right now, but the layout is free to overwrite it on any re-layout pass.

Replace `height: 1` with `Layout.preferredHeight: 1` in all six places.

### Unqualified property access in DpadMonitorViewer

`qml/panels/DpadMonitorViewer.qml`, ten occurrences at lines 43, 44, 58, 59, 71, 72, 84, 85, 99, 100.

Every one is a bare `slotMinimumWidth` or `slotMinimumHeight` that should read `root.slotMinimumWidth` / `root.slotMinimumHeight`.
These resolve through the parent scope today.
They will stop resolving the moment this file gains `pragma ComponentBehavior: Bound`, which every other panel in the repo already has.

The same file also carries an unused `import QtQuick.Controls` at line 2.

## False positives

These three are limits of static analysis, not defects.
Leave them alone.

- `qml/Theme.qml:7` reports `Member "font" not found on type "QQmlApplication"` for `Qt.application.font`.
  That property is real and resolves at runtime.
- `qml/controls/SegmentedControl.qml:20` reports `Member "advanceWidth" not found on type "QObject"` for `measurer.objectAt(i).advanceWidth`.
  `Instantiator.objectAt()` is typed as returning `QObject`, so the linter cannot see that the delegate is a `TextMetrics`, which does have `advanceWidth`.
- `qml/panels/DpadMonitorViewer.qml:2` unused import is only an Info, and disappears once the file is cleaned up anyway.

## Suggested order

1. `ChessboardPanel` dead buttons, because that is user-visible breakage.
2. The six `height: 1` separators, a mechanical one-line change per site.
3. `DpadMonitorViewer` qualification plus the unused import, ideally in the same pass that adds `pragma ComponentBehavior: Bound` to that file.
4. `PatternColorButton.palette` rename, which touches every caller of that control.

## See also

- `PERFORMANCE_NOTES.md` for the separate performance and correctness audit.
  The two lists do not overlap: `qmllint` finds what static analysis can see, that one finds what it cannot.
