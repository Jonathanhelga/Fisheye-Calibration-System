---
id: client-linux
slug: /installation/client/linux
title: Client Installation on Linux
---

# Client Installation on Linux

This guide builds and runs **`moilcali`**, the desktop calibration client, on Ubuntu or Debian.
Follow it start to finish.
No prior CMake or C++ experience is assumed.

Linux is the platform the client was originally written for, so it is the most straightforward of the three builds.
Every dependency comes from `apt`, and there is no manual library installation.
Version 1.1 no longer uses Python, a `venv`, or `requirements.client`.
Everything is compiled with **CMake** instead.
`CMakeLists.txt` only calls `find_package`, so it never downloads anything: every dependency must already be installed before you configure the project.

This page covers the **client only**.
The three HTTP servers run on the Windows server computer and are unchanged in version 1.1.
See [Server Installation](/moilcalib_documentation/docs/v1.1/installation/server).

---

## Before You Start

| Requirement | Description |
|---|---|
| **Operating System** | Ubuntu 22.04 / 24.04, or another recent Debian-based distribution |
| **Internet Connection** | Required to install packages and clone the repository |
| **GitHub Access** | Required to download the project repository |
| **Personal Access Token** | Used as the GitHub password during authentication |
| **Disk Space** | Roughly 5 GB for the toolchain and libraries |
| **Memory** | 8 GB or more. See [Always Use `-j2`](#31-always-use--j2) |

---

## Quick Start

If you only want the commands, here they are.
If anything fails, or you want to understand what these do, use the numbered sections below instead.

```bash
# 1. One-time setup
sudo apt update

# Compiler and build tools
sudo apt install -y build-essential cmake

# Qt6 user interface and networking
sudo apt install -y qt6-base-dev qt6-base-dev-tools

# Qt6 serial port (separate package from qtbase)
sudo apt install -y qt6-serialport-dev

# Qt6 SQLite driver
sudo apt install -y libqt6sql6-sqlite

# Image processing and calibration math
sudo apt install -y libopencv-dev libeigen3-dev

# 2. Configure (from the project root)
cmake -S cpp -B cpp/build

# 3. Build
cmake --build cpp/build -j2

# 4. Run
cd cpp/build
./moilcali
```

---

Before you continue, read [How the Build Works](/moilcalib_documentation/docs/v1.1/installation/client#how-the-build-works) on the Client Installation Guide.
It explains what configure, build, and run each do, and why knowing which one failed tells you where to look.

---

## 1. Install the Build Dependencies

### 1.1 Install Git

Git is required to download the project from GitHub.

```bash
sudo apt update
sudo apt install -y git
git --version
```

### 1.2 Install the Libraries

Install these one group at a time.
Each command below covers one part of the app, so it is clear what you are adding and why.

**Compiler and build tools**

`build-essential` is the GCC C++ compiler and the standard build tools.
`cmake` reads the recipe and works out how to build everything.

```bash
sudo apt install -y build-essential cmake
```

**Qt6 user interface**

`qt6-base-dev` and `qt6-base-dev-tools` give the app its user interface: windows, buttons, tables, plus networking.
This also covers the Widgets, Network, Concurrent, and OpenGLWidgets modules the app needs.

```bash
sudo apt install -y qt6-base-dev qt6-base-dev-tools
```

**Qt6 serial port**

`qt6-serialport-dev` gives serial port access for the axis stage.
Unlike the modules above, SerialPort does not ship inside qtbase, so it has its own package.

```bash
sudo apt install -y qt6-serialport-dev
```

If your distribution does not have `qt6-serialport-dev`, try `libqt6serialport6-dev` instead.
Without it, configure fails at `find_package(Qt6 ... SerialPort)`.

**Qt6 SQLite driver**

`libqt6sql6-sqlite` is the SQLite driver used by the calibration result database.

```bash
sudo apt install -y libqt6sql6-sqlite
```

**OpenCV**

`libopencv-dev` handles image processing, chessboard corner detection, and camera geometry.

```bash
sudo apt install -y libopencv-dev
```

**Eigen**

`libeigen3-dev` provides the matrix and linear algebra math used by the calibration computations.

```bash
sudo apt install -y libeigen3-dev
```

### 1.3 Clone the Repository

The C++ client lives on the **`main_development`** branch, in the `cpp/` folder.
The default branch (`main`) still holds the version 1.0 Python client, so make sure you check out the right one:

```bash
cd ~/Documents/
git clone -b main_development https://github.com/perseverance-tech-tw/moil-fisheye-calisys.git
cd moil-fisheye-calisys
```

If you already cloned the repository on the wrong branch, switch with `git checkout main_development`.
Unlike version 1.0, this branch has no Git submodules, so a plain clone gives you the complete source; there is no `--recurse-submodules` flag or `git submodule update` step to remember.

To avoid retyping your credentials, enable the Git credential cache first:

```bash
git config --global credential.helper cache
```

When GitHub asks for authentication, use your **username** and a **personal access token** as the password.
GitHub no longer accepts normal account passwords for Git operations.

---

## 2. Configure

Configure is the "check the pantry" step.
CMake reads `cpp/CMakeLists.txt`, hunts down every library, and writes a concrete build plan into `cpp/build/`.

Run this from the **project root**, not from inside `cpp/`:

```bash
cd ~/Documents/moil-fisheye-calisys
cmake -S cpp -B cpp/build
```

| Flag | Meaning |
|---|---|
| `-S cpp` | **S**ource: the recipe lives in the `cpp/` folder |
| `-B cpp/build` | **B**uild: put all generated files in `cpp/build/` |

Keeping every generated file inside `cpp/build/` is what makes cleanup easy.
Deleting that one folder resets everything, which is what [Clean Rebuild](#clean-rebuild) tells you to do when things get stuck.

You only need to run configure once.
Run it again if source files are added or removed from the project, since CMake needs to notice them; ordinary edits to existing files do not require it.

By default CMake generates Makefiles.
If you want noticeably faster incremental builds, install Ninja (`sudo apt install -y ninja-build`) and add `-G Ninja` to the configure command.
This is optional, and everything on this page works either way.

### 2.1 What a Successful Configure Looks Like

The lines that matter:

```text
-- Found OpenCV: /usr (found version "4.5.4")
-- Configuring done
-- Generating done
-- Build files have been written to: .../cpp/build
```

---

## 3. Build

This is the actual compiling.

```bash
cmake --build cpp/build -j2
```

| Flag | Meaning |
|---|---|
| `--build cpp/build` | Execute the plan that configure generated |
| `-j2` | Compile 2 files simultaneously |

### 3.1 Always Use `-j2`

Do **not** use a bare `-j` or `-j$(nproc)`.
Those tell the compiler to use every core at once, and this build is memory-hungry enough that it can exhaust RAM and get killed by the system.
The symptom is the build stopping with **exit code 137**, which means the kernel terminated the process.
Always pass an explicit small number.

`-j2` is the safe default and is what the project's own build script uses.
If you have plenty of RAM you can raise it cautiously, but exit code 137 means you went too high.

### 3.2 Confirming the Build Really Succeeded

If you pipe the build output, for example `cmake --build ... | tail -40`, the shell reports the exit status of `tail`, not the compiler.
`tail` succeeds at printing lines even when the compiler feeding it failed, so the build can look successful while nothing was produced.

The reliable check is to look for the file itself:

```bash
ls -lh cpp/build/moilcali
file cpp/build/moilcali
```

You want to see `ELF 64-bit LSB executable`, which is the Linux native executable format.

### 3.3 Building the Tests (optional)

The build above compiles the app only.
To build everything, including the test executables, drop the target filter:

```bash
cmake --build cpp/build -j2
```

Each test is a standalone program that prints its results and exits `0` on success.
There is no test framework, so you run them directly:

```bash
for t in cpp/build/*_test; do echo "== $t =="; "$t" || break; done
# e.g. ./cpp/build/calicompute_test  ./cpp/build/moil3d_test  ./cpp/build/patterngen_test
```

---

## 4. Run

```bash
cd cpp/build
./moilcali
```

### 4.1 You Must Run It From `cpp/build`

After compiling, CMake copies `config/cali_system/*.json` and `config/camera_parameters.json` into `cpp/build/config/`, next to the binary.
The app looks for those files relative to the **current working directory**, not relative to where the executable lives.
There is no `config/` folder at the project root, so launching with `./cpp/build/moilcali` from the root means the app still starts, with no error message, but the **Select Cali System** dropdown is silently empty.
Always `cd cpp/build` first.

The same applies to the `image_cali/` and `pattern_json/` folders that the capture, pattern generator, and monitor viewer windows read and write.

---

## 5. Connect to the Servers

In the app, fill in the server URL fields and click **Update**:

| Field | Local server | Remote rig |
|---|---|---|
| **Axis** | `http://127.0.0.1:8000/` | `http://<rig-ip>:8000/` |
| **Monitor** | `http://127.0.0.1:8001/` | `http://<rig-ip>:8001/` |
| **Camera** | `http://127.0.0.1:8002/` | `http://<rig-ip>:8002/` |

Changing the **Axis** URL and clicking **Update** re-runs the sensor-init dialog.
To change the default server IP the app starts with, edit `ControllerMain::initUrls()` in `cpp/src/controllers/controller_main.cpp`.

---

## Daily Usage

After the first installation you do not repeat all steps.
Rebuild only what changed, then run:

```bash
cd ~/Documents/moil-fisheye-calisys
git pull origin main_development
cmake --build cpp/build -j2
cd cpp/build && ./moilcali
```

This ensures the running binary matches the current source.

---

## Complete Installation Flow

| Step | Action | Command / Result |
|---|---|---|
| 1 | Install Git | `sudo apt install -y git` |
| 2 | Install build dependencies | `build-essential`, `cmake`, Qt6 (incl. SerialPort), OpenCV, Eigen3 |
| 3 | Clone the `main_development` branch | Project downloaded to `~/Documents/moil-fisheye-calisys` |
| 4 | Configure | `cmake -S cpp -B cpp/build` |
| 5 | Build | `cmake --build cpp/build -j2` |
| 6 | Verify the binary exists | `file cpp/build/moilcali` shows `ELF 64-bit` |
| 7 | Run from `cpp/build` | `cd cpp/build && ./moilcali` opens the client window |

---

## Troubleshooting

### Clean Rebuild

Try this first for anything build-related.
CMake caches what it found at configure time inside `cpp/build/`, so installing a missing library and reconfiguring doesn't always pick it up.

Delete the build folder and start fresh:

```bash
rm -rf cpp/build && cmake -S cpp -B cpp/build && cmake --build cpp/build -j2
```

Safe to run anytime.
Everything in `cpp/build/` is generated, so nothing of yours is lost.

### Quick Reference

| Symptom | Cause | Fix |
|---|---|---|
| `find_package` fails for Qt6 SerialPort | SerialPort is a separate package, not part of qtbase | `sudo apt install -y qt6-serialport-dev` or `libqt6serialport6-dev` |
| Build stops with **exit code 137** | Out of memory from too many parallel jobs | Use `-j2`, never a bare `-j` or `-j$(nproc)` |
| `Could NOT find Qt6` | Qt6 development packages not installed | Install `qt6-base-dev` and `qt6-base-dev-tools` |
| `Could NOT find OpenCV` | OpenCV headers missing | `sudo apt install -y libopencv-dev` |
| Build says success, but no binary exists | Exit code came from a pipe, not the compiler | Check `ls cpp/build/moilcali` directly |
| **Select Cali System** dropdown is empty | Launched from the wrong folder | `cd cpp/build` first, then `./moilcali` |
| Captured images do not appear | Same working directory problem | `cd cpp/build` first, then `./moilcali` |
| Installing a library did not fix configure | CMake cached the old result | Do a [clean rebuild](#clean-rebuild) |
| GitHub authentication failed | Password used instead of a token | Use a personal access token as the password |
| App starts but cannot reach the servers | Servers not running, or wrong URLs | Check the ports in [section 5](#5-connect-to-the-servers) and click **Update** |
