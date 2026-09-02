---
id: intro
slug: /intro
title: Calibration Result System
---

# Calibration Result System (v1.1)

The **Calibration Result System** calibrates fisheye cameras from end to end.
A full run moves through five steps, and the client drives all of them:

1. **Show the pattern.** Generate a calibration pattern and push it to the monitors surrounding the camera.
2. **Move into position.** Drive the 5-axis stage until the camera sits at the spot the current round calls for.
3. **Capture.** With the camera in place, take the positive and negative shots.
4. **Compute.** Derive the camera parameters from the captured images across all rounds.
5. **Verify and store.** Confirm the parameters can be trusted, then save them.

What you actually launch and work in is a **desktop client**.
The client never touches the hardware directly.
It sends HTTP requests to three small servers running on the rig computer: one for the 5-axis motion stage, one for the pattern monitors, and one for the camera.
That client and server split is the single most important thing to understand about version 1.1, for the reason in the next section.

Version 1.1 tracks the [`main_development`](https://github.com/perseverance-tech-tw/moil-fisheye-calisys/tree/main_development) branch of the `moil-fisheye-calisys` repository.

---

## ⚠️ Scope: The C++ Rewrite Is Client-Only

Only the **client** was rewritten in C++.
The server side is completely unchanged from version 1.0, so do **not** reinstall or rebuild it: if your servers already run, they work with the new client as is.

| | Client (the calibration app) | Server (axis / monitor / camera services) |
|---|---|---|
| **Version 1.0** | Python 3.8 + PyQt6 | Python 3.8.10 + FastAPI on Windows 11 |
| **Version 1.1** | **C++17 + Qt6** ← rewritten | **Python 3.8.10 + FastAPI — unchanged** |
| **Where it runs** | Ubuntu or Windows workstation | Windows 11 x64 server computer |
| **Installation guide** | [Client Installation Guide](/moilcalib_documentation/docs/v1.1/installation/client) — completely new steps | [Server Installation](/moilcalib_documentation/docs/v1.1/installation/server) — identical to version 1.0 |
| **What to do when upgrading** | Install the build tools and compile the new client | **Nothing** |

The two sides talk over **HTTP**, not through shared code.
As long as the three services answer the same endpoints on ports `8000` (axis), `8001` (monitor), and `8002` (camera), the client does not care what language they are written in, which is why a **version 1.1 C++ client** paired with a **version 1.0 Python server** is the expected, supported combination.

---

## What Changed in Version 1.1

Version 1.1 is **not a new calibration method**.
The calibration procedure, the hardware, and the results you get are the same as version 1.0.
What changed is how the client application is built, and four of those changes are visible while you work.

### 1. The application is faster

The client is compiled C++17 and Qt6 instead of interpreted Python and PyQt6.
The heavy analysis paths gain the most: multi-round aggregation, range search, regression fitting, and ZFL/IH curve building.
The numbers themselves do not change, because every compute module was checked against the version 1.0 Python results before release.

### 2. The window no longer freezes while the stage moves

Axis sensors, positions, and alpha/beta values are now read on a background worker, so the interface stays responsive during motion.
Two safety behaviours came with it:

- **Axis safety-lock.** While an axis is moving, every control except that axis's **STOP** button is locked, so a second command cannot be issued into a moving stage.
- **Blocking homing.** `ALL HOME` runs the axes in a fixed order (yaw → pitch → X → Y → Z), skips axes that are already homed, and waits for a confirmed stop before continuing, with a progress dialog throughout.

### 3. Installation no longer involves Python

There is no virtual environment, no `pip` install, and no submodules.
The client is a compiled binary, it now runs on **Windows as well as Ubuntu**, and it ships as a portable Linux `.7z`, a Windows installer, or a desktop launcher.
After pulling new code you **rebuild** instead of reinstalling packages.
See the [Client Installation Guide](/moilcalib_documentation/docs/v1.1/installation/client).

### 4. New tool: Setup Center

Version 1.1 adds **[Setup Center](/moilcalib_documentation/docs/v1.1/verification/setup-center)**, opened from the **Calibration Result / 3D Validation** panel of the main window.

It verifies the camera centre point (`iCx`, `iCy`) in a camera-parameter JSON: load a fisheye image, nudge the centre by clicking the image or using the spin boxes, and confirm it against guide rings, a panorama preview, and an anypoint view. If the centre is wrong, every result computed from those parameters is wrong — so this check now has its own tool, and the corrected centre can be saved straight back into the parameter file.

Working on the client itself?
The toolchain, source layout, compute-core tests, and packaging are documented in [Codebase Overview](/moilcalib_documentation/docs/v1.1/development/codebase-overview).

---

## Known Limitations in Version 1.1

There is one thing to know about reloading old calibrations, and it explains both of the limitations below.

**The database does not contain your calibration numbers.**
Think of it as a library catalogue rather than the library itself.
A record tells you that a calibration exists, when it was made, and which camera it belongs to, but the measured values live in Excel files kept in cloud storage.

That leads to two things the application will not do for you:

**You cannot open a calibration directly from the Database window.**
You can browse and search the records there, but they will never fill the round tables in the Cali Result window.
This is by design, not a fault: the record has nothing to load, because it only points at files stored elsewhere.

**You cannot paste a cloud link into the Cali Folder field.**
Typing an `http://` or `https://` address returns *"Remote (cloud) links are not supported in this build. Enter a local folder or .xlsx path."*
The field reads folders on your own disk only.

**So to reopen an old calibration, do this:**

1. Find the calibration you want in the Database window and note where its folder lives.
2. Download that folder from cloud storage onto your own computer.
3. In the Cali Result window, use **Load All Excel** and point it at the folder you just downloaded.

The full procedure, including what each button does, is in [Reload Calibration Data](/moilcalib_documentation/docs/v1.1/calibration/reload-calibration-data).

One small tool from version 1.0, the repeat-experiment helper, was not carried over.
It was a separate script that the main application never had a button for, and the histogram it produced is already available in the main window.

---

## Where to Start

Begin with [Installation](/moilcalib_documentation/docs/v1.1/installation/server).
Install the server first, exactly as in version 1.0, then build the client, whose steps are completely new.

Next read the [Main Window Reference](/moilcalib_documentation/docs/v1.1/system-overview/main-window), which describes the window every calibration is run from.

After that the sidebar follows the order of a real calibration, from pattern setup through to the database.
One thing worth knowing before you begin: the **PCT Pattern Generator** and **Monitor Viewer** are step 1, not side utilities.
The pattern has to be generated and pushed to the correct monitor before any capture means anything.
 