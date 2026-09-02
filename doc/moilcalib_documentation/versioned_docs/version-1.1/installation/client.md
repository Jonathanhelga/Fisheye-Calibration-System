---
id: client-installation-guide
slug: /installation/client
title: Calibration System Client Installation Guide (C++ / Qt6)
---

# Calibration System Client Installation Guide (C++ / Qt6)

The **Calibration System Client** is the desktop calibration application. In version 1.1 it was rewritten from Python / PyQt6 to **C++ / Qt6** (with OpenCV and Eigen), but what it does is unchanged: it talks to the axis, monitor, and camera HTTP servers to run a calibration.

Building it means installing a handful of libraries, then compiling the source with CMake — there's no more Python or virtual environment involved.

*A few screenshots on this page are still reused from the version 1.0 documentation; the application window itself will be replaced with a version 1.1 capture later.*

---

## Choose Your Platform

The exact commands differ per operating system, so pick your platform below for a full, start-to-finish guide:

| Platform | Guide |
|---|---|
| **Linux** (Ubuntu / Debian) | [Client Installation on Linux](/moilcalib_documentation/docs/v1.1/installation/client/linux) |
| **macOS** (Apple Silicon) | [Client Installation on macOS](/moilcalib_documentation/docs/v1.1/installation/client/macos) |

Each guide is self-contained — you don't need to read this page first, and you don't need to read the other platform's guide.

---

## How the Build Works

Understanding the shape of this makes every later error message much easier to read, no matter which platform you're on.

Turning source code into a running app is like cooking a meal from a recipe:

| Cooking | Software | What it means here |
|---|---|---|
| The recipe | `CMakeLists.txt` | A file describing what to build and what it needs |
| Ingredients | Qt, OpenCV, Eigen | Pre-written libraries this app depends on |
| Checking the pantry | **Configure** | CMake looks for each library and records where it found it |
| Actually cooking | **Build** | The compiler turns source code into a program |
| Eating | **Run** | Launching the finished app |

**Configure**, **build**, and **run** are three separate steps that fail in three different ways.
Knowing which step you are on tells you where to look:

| Step that failed | What it means |
|---|---|
| **Configure** | A library is missing entirely |
| **Build** | The libraries are present but do not fit together |
| **Run** | The app compiled but cannot start |

---

## What You'll Need, Regardless of Platform

- **GitHub access**, with a personal access token (GitHub no longer accepts your account password for Git operations).
- **An internet connection**, since Qt and OpenCV are sizeable downloads.
- **About 5 GB of disk space** for the toolchain and libraries.

The default branch (`main`) still holds the version 1.0 Python client.
The C++ / Qt6 client lives on [`main_development`](https://github.com/perseverance-tech-tw/moil-fisheye-calisys/tree/main_development), which is the branch every platform guide clones.
If you already cloned the repository on `main`, switch with `git checkout main_development`.

---

## After Installation: Connect to the Servers

Once built and running, fill in the server URL fields in the app and click **Update**:

| Field | Local server | Remote rig |
|---|---|---|
| **Axis** | `http://127.0.0.1:8000/` | `http://<rig-ip>:8000/` |
| **Monitor** | `http://127.0.0.1:8001/` | `http://<rig-ip>:8001/` |
| **Camera** | `http://127.0.0.1:8002/` | `http://<rig-ip>:8002/` |

The servers themselves are a separate installation, on the server computer, and are unchanged from version 1.0 — see the [Server Installation Guide](/moilcalib_documentation/docs/v1.1/installation/server).
