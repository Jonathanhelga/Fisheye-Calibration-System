---
id: codebase-overview
slug: /development/codebase-overview
title: Codebase Overview
---

# Codebase Overview

This page is for developers working on the client itself.
It describes how the version 1.1 C++ client is put together: the toolchain it replaced, the source layout, the compute core and its tests, and how the application is built and packaged.

<div className="custom-note custom-important">
  <div className="custom-note-title">📖 WHAT THIS PAGE IS</div>
  <div>
    This is <strong>internal structure</strong>, not a usage guide. If you only need to install and run the application, go to the <a href="/moilcalib_documentation/docs/v1.1/installation/client"><strong>Client Installation Guide</strong></a> instead.
  </div>
</div>

---

## 1. Toolchain: Python to C++

The client is now compiled native code instead of interpreted Python.
This is what each layer was replaced with:

| Where | Version 1.0 (Python) | Version 1.1 (C++) |
|---|---|---|
| **Calibration math** | NumPy + Python loops | C++17 with **Eigen3** for linear algebra |
| **Image processing** | OpenCV through the Python binding | **OpenCV** called directly, no binding overhead |
| **Result tables and plots** | PyQt6 widgets driven from Python | Qt6 widgets driven from C++ |
| **Pattern generation** | Python rendering loops | C++ rendering |
| **Parallel work** | Limited by the Python GIL | **QtConcurrent** threads and **OpenMP**, no GIL |

The heavy analysis paths benefit most: multi-round aggregation, range search, regression fitting, and ZFL/IH curve building.
Those are exactly the loops that Python executed one element at a time.

### Threading

The axis stage is polled by a background worker, one `QtConcurrent` task per axis, every 20 ms.
Each task reads the limit sensors, the origin sensor, the motion state, and the position, then updates the LEDs, coordinates, and alpha/beta values live.
Because this runs off the UI thread, the window stays responsive while the stage is moving.

---

## 2. Source Layout

Version 1.0 spread the client across `mvc_model/`, `mvc_controller/`, and `mvc_view/`, with very large controller files: `controller_main.py` was about 3,700 lines and `controller_cali_result.py` about 7,900.
Version 1.1 puts the whole client under `cpp/`, split by responsibility:

```text
cpp/
  CMakeLists.txt              # Qt6 + OpenCV + Eigen; AUTOUIC / AUTOMOC
  ui/*.ui                     # Qt Designer layouts (mainwindow, cali_result,
                              # pattern_generator, monitor_viewer, 3d_measurement)
  app/                        # main.cpp: QApplication, theme, MainWindow
  src/
    controllers/              # one controller per window/dialog
    core/
      cali/                   # CaliCompute, CaliMath, CaliRound, Regression
      algorithm/              # MoilCali detection algorithms
      measure3d/              # Moil3d triangulation
      pattern/                # pattern generator core
      moildev/                # Moildev interface
    models/device/            # axis / camera / monitor HTTP clients
    io/                       # XlsxIO, HttpSync
    views/                    # HistogramPlot, Point3dGlView, theme, widgets
  tests/                      # one test binary per compute module
  packaging/                  # portable .7z, desktop entry, Inno Setup script
```

The point of this split is that the **compute core is separated from the UI**, which is what makes the module tests below possible.

---

## 3. Compute Core and Tests

Because the math lives in standalone modules under `src/core/`, each one has its own test binary that runs without launching the application:

| Test | Covers |
|---|---|
| `calicompute_test` | Multi-round calibration computation |
| `calimath_test` / `caliround_test` | Calibration math and per-round handling |
| `regression_test` | Curve fitting |
| `moilcali_test` | Center/ROI detection algorithm |
| `moil3d_test` / `measure3d_test` | 3D triangulation |
| `patterngen_test` | Pattern generation |
| `xlsx_test` | Excel import/export |
| `database_test` | Calibration result database |

Every one of these modules was checked **numerically against the version 1.0 Python results**.
That is the basis for the guarantee that version 1.1 produces the same values, only faster.

---

## 4. Build and Packaging

| | Version 1.0 | Version 1.1 |
|---|---|---|
| **Runtime needed** | Python 3.8.10 built from source, `venv`, pinned `pip` 22.0 / `setuptools` 59.6, `requirements.client`, manual Moildev 2.7 | None, a compiled binary |
| **Dependencies** | Installed per machine with `pip` | Found at build time by CMake (`find_package`) |
| **Submodules** | `git submodule update --remote` required | No submodules |
| **Platforms** | Ubuntu | **Ubuntu and Windows (MSVC)** |
| **Distribution** | Copy the source tree and rebuild the environment | Portable Linux `.7z` (bundles Qt + OpenCV), Windows Inno Setup installer, Linux desktop launcher |

There is no virtual environment to activate.
After pulling new code you **rebuild** rather than reinstalling packages.
The per-platform steps are in the [Client Installation Guide](/moilcalib_documentation/docs/v1.1/installation/client).
