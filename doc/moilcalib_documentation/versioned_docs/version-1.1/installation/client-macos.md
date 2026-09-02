---
id: client-macos
slug: /installation/client/macos
title: Client Installation on macOS
---

# Client Installation on macOS

This guide builds and runs **`moilcali`**, the desktop calibration client, natively on a Mac.
Follow it start to finish.
No prior CMake or C++ experience is assumed.

This page covers the **client only**.
The three HTTP servers are a separate project on the server computer, and they are unchanged in version 1.1 (see [Server Installation](/moilcalib_documentation/docs/v1.1/installation/server)).

The client was originally written for Linux, so it is fair to expect some Mac-specific patching.
There is none.
The source has no platform `#ifdef` guards, no Linux-only headers, and no hardcoded absolute paths.
The two external programs it calls, `zip` and `unzip` (used by `XlsxIO.cpp` for Excel export), both ship with macOS.
Everything on this page is about setting up the **build environment**, not editing code.

---

## Before You Start

| Requirement | Description |
|---|---|
| **Operating System** | macOS on Apple Silicon (arm64). Verified on Darwin 24.4.0 |
| **Xcode Command Line Tools** | Provides the C++ compiler and the macOS system headers |
| **Homebrew** | The package manager used to install every library. See [brew.sh](https://brew.sh) |
| **Internet Connection** | Qt and OpenCV are large downloads |
| **GitHub Access** | Required to clone the private repository |
| **Disk Space** | Roughly 5 GB for the toolchain and libraries |

The one thing that breaks everything: install **`opencv@4`**, not `opencv`.
Homebrew's default `opencv` formula is now version 5, which this codebase cannot compile against.
Worse, the mistake does not surface when you make it: configure succeeds happily, and the failure appears later as a wall of compiler errors.
If you read nothing else on this page, read [Why the OpenCV Path Must Come First](#22-why-the-opencv-path-must-come-first).

---

## Quick Start

If you only want the commands, here they are.
If anything fails, or you want to understand what these do, use the numbered sections below instead.

```bash
# 1. One-time setup
xcode-select --install

# Compiler and build tools
brew install cmake ninja

# Qt6 user interface, networking, and serial port
brew install qt

# Image processing and calibration math
brew install opencv@4 eigen

# Optional: lets Eigen use multiple CPU cores
brew install libomp

# 2. Configure (from the project root)
/opt/homebrew/bin/cmake -S cpp -B cpp/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/opencv@4;/opt/homebrew"

# 3. Build
/opt/homebrew/bin/cmake --build cpp/build -j6 --target moilcali

# 4. Run
cd cpp/build
./moilcali
```

---

Before you continue, read [How the Build Works](/moilcalib_documentation/docs/v1.1/installation/client#how-the-build-works) on the Client Installation Guide.
It explains what configure, build, and run each do, and why knowing which one failed tells you where to look.

On macOS specifically, the OpenCV 5 mistake described later on this page is made at **configure** time but only fails at **build** time.
The error appears in a completely different step from the one where you caused it, which is exactly what makes it hard to diagnose.

---

## 1. Install the Build Tools

### 1.1 Xcode Command Line Tools

This gives you the C++ compiler itself, plus the macOS system headers.

```bash
xcode-select --install
```

If it is already installed, the command says so and does nothing.
To check first:

```bash
xcode-select -p
```

A printed path means the tools are present.

### 1.2 Homebrew Packages

Install these one group at a time.
Each command below covers one part of the app, so it is clear what you are adding and why.
Qt and OpenCV are both large, so expect this to take a while on a slow connection.

**Compiler and build tools**

`cmake` reads the recipe and works out how to build everything.
`ninja` does the actual compiling, quickly and in parallel.

```bash
brew install cmake ninja
```

**Qt6 user interface**

`qt` provides the entire user interface: windows, buttons, tables, plus networking and serial port access.

```bash
brew install qt
```

**OpenCV and Eigen**

`opencv@4` handles image processing, chessboard corner detection, and camera geometry.
`eigen` provides the matrix and linear algebra math used by the calibration computations.

It says `opencv@4`, not `opencv`, and that is deliberate.
Typing `brew install opencv` gets you OpenCV 5, and the build will fail later with errors about `findChessboardCornersSB` and `__sort3`.
See [Why OpenCV 4 Is Pinned](#why-opencv-4-is-pinned) for the full story.

```bash
brew install opencv@4 eigen
```

**libomp (optional)**

`libomp` would let Eigen use multiple CPU cores.
See [OpenMP Is Not Enabled](#openmp-is-not-enabled).

```bash
brew install libomp
```

### 1.3 If You Already Have OpenCV 5 Installed

That is completely fine, and you do **not** need to uninstall it.
The two versions coexist peacefully because `opencv@4` is what Homebrew calls **keg-only**.

Think of your Homebrew installation as a kitchen.
Normal packages go on the main shelf, where anything can grab them by default.
A keg-only package goes into a clearly labelled side cupboard instead, specifically so that nothing picks it up by accident.

So OpenCV 4 is installed but is *not* the default.
You have to point at it explicitly, which is exactly what `CMAKE_PREFIX_PATH` does in the next step.

### 1.4 Versions This Guide Was Verified Against

| Component | Version |
|---|---|
| macOS | Darwin 24.4.0, Apple Silicon (arm64) |
| Apple clang | 17.0.0 |
| Qt | 6.11.1 (Homebrew) |
| OpenCV | 4.14.0 (`opencv@4`, keg-only) |
| Eigen | 5.0.1 |
| CMake | 4.4.2 (Homebrew) |
| Ninja | 1.13.2 |

Homebrew installs Eigen 5, but `CMakeLists.txt` asks for Eigen 3, and this works anyway.
Eigen 5 still ships a compatibility file named `Eigen3Config.cmake`, so the `Eigen3::Eigen` target still resolves.
Unlike OpenCV, Eigen did not break the parts of its API this project uses.

---

## 2. Configure

Configure is the "check the pantry" step.
CMake reads `cpp/CMakeLists.txt`, hunts down every library, and writes a concrete build plan into `cpp/build/`.

Run this from the **project root**:

```bash
cd /path/to/moil-fisheye-calisys

/opt/homebrew/bin/cmake -S cpp -B cpp/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/opencv@4;/opt/homebrew"
```

### 2.1 What Each Flag Means

| Flag | Meaning |
|---|---|
| `-S cpp` | **S**ource: the recipe lives in the `cpp/` folder |
| `-B cpp/build` | **B**uild: put all generated files in `cpp/build/` |
| `-G Ninja` | **G**enerator: produce a build plan for Ninja, which is faster than the default Make |
| `-DCMAKE_BUILD_TYPE=Release` | Optimise for speed rather than for debugging |
| `-DCMAKE_PREFIX_PATH=...` | Where to search for libraries, **and in what order** |

Keeping every generated file inside `cpp/build/` is what makes cleanup easy.
Deleting that one folder resets everything, which is what [Clean Rebuild](#clean-rebuild) tells you to do when things get stuck.

### 2.2 Why the OpenCV Path Must Come First

`CMAKE_PREFIX_PATH` is a list of places to look for libraries, separated by semicolons.

Think of it as handing someone a list of shops and saying "get me OpenCV, try these in order."
They go to the first shop on the list.
If it has OpenCV, they buy it and come straight home.
They never check the second shop.

Our list is:

```text
/opt/homebrew/opt/opencv@4     <- the side cupboard holding OpenCV 4
/opt/homebrew                  <- the main shelf, holding Qt, Eigen, and OpenCV 5
```

Because the OpenCV 4 cupboard is listed first, CMake finds OpenCV 4 and stops looking.
Qt and Eigen are not in that first location, so the search continues to `/opt/homebrew` and finds them there.

Swap the order and CMake finds OpenCV **5** on the main shelf first.
Configure still succeeds, cheerfully reporting that it found OpenCV.
The failure only surfaces later, in the middle of the build, as a wall of compiler errors.

### 2.3 Why the Full Path to `cmake`

The command says `/opt/homebrew/bin/cmake` rather than just `cmake`.

If you have MacPorts installed alongside Homebrew, `/opt/local/bin/cmake` usually sits earlier in your `PATH` and gets picked instead.
That version is often older and may not understand a modern Qt 6 installation.

Check which one your shell would choose:

```bash
which -a cmake
```

This lists every `cmake` on your system, in the order your shell searches them.
The first line wins.
If that first line is already `/opt/homebrew/bin/cmake`, you can safely type just `cmake` from here on.

Spelling out the full path is simply the version that works for everyone regardless of setup.

### 2.4 What a Successful Configure Looks Like

You will see roughly thirty lines of output.
These are the ones that matter:

```text
-- Found OpenCV: /opt/homebrew/Cellar/opencv@4/4.14.0 (found version "4.14.0")
-- Could NOT find OpenMP_CXX (missing: OpenMP_CXX_FLAGS OpenMP_CXX_LIB_NAMES)
-- Configuring done
-- Generating done
-- Build files have been written to: .../cpp/build
```

Check that the OpenCV line says 4.x.
If it reports `5.x`, stop now and fix `CMAKE_PREFIX_PATH`, because the build will fail.
You will also need a [clean rebuild](#clean-rebuild), since CMake caches this decision and simply re-running configure will not change it.

Two warnings are **expected** and harmless:

| Warning | Why it appears |
|---|---|
| `Could NOT find OpenMP_CXX` | Apple's clang does not enable OpenMP by default. See [OpenMP Is Not Enabled](#openmp-is-not-enabled) |
| `Could NOT find WrapVulkanHeaders` | Qt looking for an optional graphics backend that macOS does not use |

---

## 3. Build

This is the actual compiling.

```bash
/opt/homebrew/bin/cmake --build cpp/build -j6 --target moilcali
```

| Flag | Meaning |
|---|---|
| `--build cpp/build` | Execute the plan that configure generated |
| `-j6` | Compile 6 files simultaneously |
| `--target moilcali` | Build only the app, not the unit tests |

Progress appears as numbered lines like `[17/36]`, counting source files as they finish.
The final step, `[36/36]`, is the link step that combines everything into one executable.

### 3.1 Choosing a Number for `-j`

`-j6` means six compiler processes at once.
More is faster, until you run out of memory and the machine starts struggling.

`-j6` was verified comfortable on a 16 GB Mac.
Lower it to `-j4` or `-j2` if you have 8 GB, or if your Mac becomes unresponsive while building.

Compared to Linux: the Linux guide insists on `-j2`, because higher values can exhaust memory and get the compiler killed by the system (exit code 137).
Macs generally tolerate more, but the underlying build is memory-hungry, so do not go wild.

### 3.2 Confirming the Build Really Succeeded

Do not trust the exit code if you pipe the build.
When you write `cmake --build ... | tail -40`, the shell reports the exit status of the **last** command in the chain, which is `tail`.
`tail` succeeds at its job of printing lines even when the compiler feeding it failed, so the build can report success while nothing was produced.

The reliable check is to look for the file itself:

```bash
ls -lh cpp/build/moilcali
file cpp/build/moilcali
```

You want to see:

```text
cpp/build/moilcali: Mach-O 64-bit executable arm64
```

`Mach-O` means a macOS-native executable.
`arm64` means it is built for Apple Silicon rather than running under Rosetta translation.

### 3.3 Building the Tests (optional)

Drop `--target moilcali` to build everything, including the ten test executables:

```bash
/opt/homebrew/bin/cmake --build cpp/build -j6
```

Each test is a standalone program that prints its results and exits `0` on success.
There is no test framework, so you run them directly:

```bash
./cpp/build/calicompute_test
```

Or run them all in sequence:

```bash
for t in cpp/build/*_test; do echo "== $t =="; "$t" || break; done
```

---

## 4. Run

```bash
cd cpp/build
./moilcali
```

### 4.1 You Must Run It From `cpp/build`

This is not a style preference.
After compiling, CMake copies `config/cali_system/*.json` and `config/camera_parameters.json` to sit next to the binary.
The app looks for those files using a path relative to the **current working directory**, not relative to where the executable lives.
Launch from anywhere else and the app still starts, with no error message, but the **Select Cali System** dropdown is silently empty.
That silent failure is exactly why this is worth remembering.

### 4.2 Do Not Run `cmake --install`

The `install()` section of `CMakeLists.txt` was written for Linux desktops.
It writes a `.desktop` launcher file and copies icons into `share/icons/hicolor/`, and neither of those means anything on macOS.

There is no reason to install.
Running the binary directly out of `cpp/build` is the intended macOS workflow.

---

## 5. Connect to the Servers

In the app, fill in the server URL fields and click **Update**:

| Field | Local server | Remote rig |
|---|---|---|
| **Axis** | `http://127.0.0.1:8000/` | `http://<rig-ip>:8000/` |
| **Monitor** | `http://127.0.0.1:8001/` | `http://<rig-ip>:8001/` |
| **Camera** | `http://127.0.0.1:8002/` | `http://<rig-ip>:8002/` |

---

## Daily Usage

After the first installation you do not repeat all steps.
Rebuild only what changed, then run:

```bash
cd /path/to/moil-fisheye-calisys
git pull origin main_development
/opt/homebrew/bin/cmake --build cpp/build -j6 --target moilcali
cd cpp/build && ./moilcali
```

Configure only needs re-running when files are added or removed from the project.

---

## Complete Installation Flow

| Step | Action | Command / Result |
|---|---|---|
| 1 | Install Xcode Command Line Tools | `xcode-select --install` |
| 2 | Install libraries with Homebrew | `brew install qt opencv@4 eigen libomp cmake ninja` |
| 3 | Configure | `cmake -S cpp -B cpp/build -G Ninja …` with `opencv@4` **first** in the prefix path |
| 4 | Verify configure output | The OpenCV line reports `4.x`, not `5.x` |
| 5 | Build | `cmake --build cpp/build -j6 --target moilcali` |
| 6 | Verify the binary exists | `file cpp/build/moilcali` shows `Mach-O … arm64` |
| 7 | Run from `cpp/build` | `cd cpp/build && ./moilcali` opens the client window |

---

## macOS Differences to Know About

These work differently on macOS than on Linux.
None of them prevent the app from building or running.

### OpenMP Is Not Enabled

**What OpenMP is.** A system that lets one program spread heavy math across multiple CPU cores at once.

**Why it is missing.** Apple's bundled clang compiler does not enable OpenMP by default, so CMake cannot find it even though you installed `libomp`.

**Why the build works anyway.** `CMakeLists.txt` treats OpenMP as strictly optional:

```cmake
$<$<TARGET_EXISTS:OpenMP::OpenMP_CXX>:OpenMP::OpenMP_CXX>
```

That reads as "link OpenMP only if it exists", so its absence is handled gracefully.

**What it costs you.** Eigen runs single-threaded, so heavy calibration math is slower than the same code on Linux.
Results are numerically identical.
Only the speed differs.

Wiring it up means passing `libomp`'s include and library paths to clang by hand.
Worth doing only if calibration speed becomes an actual problem in practice.

### OpenGL Is Deprecated on macOS

The 3D point viewer in `src/views/Point3dGlView.cpp` draws using an older style of OpenGL known as the **fixed-function pipeline**, with calls like `glBegin` and `glVertex` across 27 sites.

macOS offers two OpenGL modes, and only one of them supports that older style:

| Mode | Supports `glBegin`? |
|---|---|
| Legacy OpenGL 2.1 (compatibility profile) | Yes |
| OpenGL 3.2+ Core profile | No, it was removed |

Fortunately the code already asks for the right one, at `Point3dGlView.cpp:109`:

```cpp
fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
```

Apple has deprecated OpenGL in favour of Metal, but it still functions.
Expect a deprecation warning in the console when the 3D view opens.
That warning is noise, not an error.

This has not yet been verified: the main window launches cleanly, but the 3D viewer had not been opened at the time of writing.
If **Auto 3D Measurement** renders blank or crashes, this compatibility profile is the first thing to investigate.

### Serial Port Device Names Differ

`QSerialPort` works on macOS, but connected devices are named differently than on Linux:

| Platform | Typical device path |
|---|---|
| Linux | `/dev/ttyUSB0`, `/dev/ttyACM0` |
| macOS | `/dev/cu.usbserial-*`, `/dev/cu.usbmodem*` |

You may also need a vendor driver for your specific USB-to-serial adapter chipset.

List what your Mac currently sees:

```bash
ls /dev/cu.*
```

---

## Troubleshooting

### Clean Rebuild

Try this first for anything build-related.
CMake caches what it found at configure time inside `cpp/build/`, so changing `CMAKE_PREFIX_PATH` and reconfiguring doesn't always pick it up.

Delete the build folder and start fresh, then repeat [Configure](#2-configure) and [Build](#3-build):

```bash
rm -rf cpp/build
```

Safe to run anytime.
Everything in `cpp/build/` is generated, so nothing of yours is lost.

### Quick Reference

| Symptom | Cause | Fix |
|---|---|---|
| `no member named 'findChessboardCornersSB'` | Building against OpenCV 5 | Put `/opt/homebrew/opt/opencv@4` first in `CMAKE_PREFIX_PATH`, then clean rebuild |
| `no matching function for call to '__sort3'` | Same as above | Same as above |
| Configure reports `found version "5.x"` for OpenCV | Wrong `CMAKE_PREFIX_PATH` order | Same as above |
| `Could NOT find Qt6` | Homebrew Qt not on the prefix path | Verify with `brew --prefix qt`, and keep `/opt/homebrew` in `CMAKE_PREFIX_PATH` |
| CMake complains about an unsupported Qt configuration | MacPorts `cmake` is being used | Call `/opt/homebrew/bin/cmake` by full path |
| Build says success, but no binary exists | Exit code came from a pipe, not the compiler | Check `ls cpp/build/moilcali` directly |
| **Select Cali System** dropdown is empty | Launched from the wrong folder | `cd cpp/build` first, then `./moilcali` |
| App seems not to open | It may be on another display | Check your other monitors, and `pgrep -lx moilcali` |
| `Could NOT find OpenMP` | Expected on macOS | Not a problem, see [OpenMP Is Not Enabled](#openmp-is-not-enabled) |
| `Could NOT find WrapVulkanHeaders` | Expected on macOS | Not a problem, ignore |

---

## Background: Design Decisions

You do not need this section to install the client.
It records *why* two things are the way they are, so they do not get re-proposed later.

### Why OpenCV 4 Is Pinned

Homebrew updated its default `opencv` formula to OpenCV 5.
A new major version means the authors allowed themselves to make breaking changes.
This codebase was written against OpenCV 4, and the Linux build still uses OpenCV 4 (Ubuntu 20.04 ships 4.2).

Imagine your code cites a reference book by chapter number.
A new edition comes out, the publisher reorganises it, moving a chapter and rewriting some entries.
Your citations still *look* fine, but they now point at the wrong pages or at nothing at all.

That is precisely what happened, in two separate places.

**Break 1: functions moved to a different file.**

```text
AnypointChessboard.cpp:98: error: no member named 'CALIB_CB_NORMALIZE_IMAGE' in namespace 'cv'
AnypointChessboard.cpp:98: error: no member named 'CALIB_CB_EXHAUSTIVE' in namespace 'cv'
AnypointChessboard.cpp:99: error: no member named 'CALIB_CB_ACCURACY' in namespace 'cv'
AnypointChessboard.cpp:104: error: no member named 'findChessboardCornersSB' in namespace 'cv'
```

In OpenCV 5, chessboard detection moved out of `calib3d.hpp` and into `objdetect.hpp`.
`AnypointChessboard.cpp` includes only `calib3d.hpp`, so as far as the compiler is concerned those functions no longer exist.

**Break 2: a function signature changed.**

```text
moil_3d_algorithm.cpp:70: error: no matching function for call to '__sort3'
```

This looks alarming because it points into Apple's C++ standard library rather than into project code.
The real cause is at `moil_3d_algorithm.cpp:70`, where `std::sort` is given a comparison function that calls `cv::contourArea`.
OpenCV 5 changed that function's signature enough that the comparison no longer type-checks, and the error surfaces deep inside the sorting machinery instead of at the call site.

**Why pin rather than fix the code.**
Both breaks are patchable.
But the fix would have to keep working on Linux and Windows, which are still on OpenCV 4, so it would mean scattering version checks like `#if CV_VERSION_MAJOR >= 5` through the source.
That is permanent complexity added purely to work around one machine's package manager default.

Installing `opencv@4` costs nothing, requires no source changes, and keeps every platform compiling identical code.
Migrating the project to OpenCV 5 may well be worth doing one day, but it should be a deliberate, tested, project-wide decision, not a side effect of setting up a Mac.

### Why Not Docker

Containerising this app for macOS development was considered and deliberately set aside.

A Linux container produces a **Linux** binary, and macOS cannot run Linux binaries.
So a dependency-only container gives you a compile loop with no way to actually launch and see the GUI.
You could confirm that code compiles, but never that it works.

Closing that gap means running a virtual display inside the container and streaming it to a browser.
That works, but it costs GPU acceleration on the 3D viewer, and it rules out serial port access entirely.

Building natively, as documented above, avoids all of that and gives full GPU and hardware access.

The existing `packaging/Dockerfile.ubuntu2004` is still the correct tool for its actual job, which is producing distributable Linux binaries linked against an old glibc for maximum compatibility.
It was never intended as a development environment.
