---
slug: /installation/server
title: Server Installation
---

# Server Installation

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ UNCHANGED IN VERSION 1.1 — THIS IS STILL THE PYTHON SERVER</div>
  <div>
    The C++ rewrite in version 1.1 applies to the <strong>client application only</strong>. The server side did <strong>not</strong> change: it is still the <strong>Python 3.8.10 + FastAPI</strong> installation from version 1.0, running on Windows 11 x64. This page is identical to the version 1.0 guide.
    <ul>
      <li><strong>If your servers are already installed and running</strong> — do nothing. They work with the new C++ client as-is.</li>
      <li><strong>If you are setting up a new server computer</strong> — follow this guide exactly as written, including Python 3.8.10 and Moildev 2.7.</li>
    </ul>
  </div>
</div>

This page explains how to install and configure the **Calibration System Server** on **Windows 11 x64**.

The server computer is responsible for running the HTTP services used by the calibration client. These services control the axis stage, monitor display, and camera capture system. The server must run three HTTP services: an **Axis Server**, a **Monitor Server**, and a **Camera Server**. The calibration client connects to these services through the server IP address.

---

## Why the Server Stayed on Python

The client and the servers communicate over **HTTP**, not through shared code. The client only needs the three services to answer the same endpoints on ports `8000`, `8001`, and `8002` — it does not care what language they are written in. So rewriting the client in C++ required no change on the server side.

| Side | Version 1.0 | Version 1.1 |
|---|---|---|
| **Client** (calibration app) | Python 3.8 + PyQt6 | **C++17 + Qt6** — see the [Client Installation Guide](/moilcalib_documentation/docs/v1.1/installation/client) |
| **Server** (this page) | Python 3.8.10 + FastAPI on Windows 11 | **Python 3.8.10 + FastAPI on Windows 11 — unchanged** |

A version 1.1 C++ client talking to a version 1.0 Python server is the **expected and supported** combination.

<div className="custom-note custom-important">
  <div className="custom-note-title">📌 ABOUT THE C++ SERVER IN THE REPOSITORY</div>
  <div>
    The repository contains an experimental C++ server port (<code>cpp_server/</code>). Do <strong>not</strong> use it for real calibration work: only the camera server is a full port, while the axis motion and monitor display layers are still <strong>simulators</strong> (the real axis serial protocol and display control are unfinished). Follow this Python guide instead.
  </div>
</div>

---

## Before You Start

Before beginning the installation, make sure the server computer has the required software, network connection, and hardware access.

| Requirement | Description |
|---|---|
| **Operating System** | Windows 11 x64 |
| **Network** | The server computer and client computer must be connected to the same network. |
| **GitHub Access** | A GitHub account is required to clone the private project repository. |
| **O365 Access** | Required for downloading Moildev and some hardware driver files. |
| **Hardware Access** | Axis stage, monitor displays, and camera hardware must be connected correctly. |
| **Administrator Permission** | Required for driver installation and running server CMD terminals. |

---

## Overview

The **Calibration System Server** provides hardware-control services for the calibration system.

| Server Service | Port | Main Purpose |
|---|---:|---|
| **Axis HTTP Server** | `8000` | Controls the axis stage hardware. |
| **Monitor HTTP Server** | `8001` | Controls calibration pattern display on external monitors. |
| **Camera HTTP Server** | `8002` | Controls camera capture and image acquisition. |

After installation, each server can be checked from a browser using FastAPI interactive API documentation.

```text
Axis API    → http://<Server IP>:8000/docs
Monitor API → http://<Server IP>:8001/docs
Camera API  → http://<Server IP>:8002/docs
```

---

## 1. Install Required Software

This section installs the tools the server needs before any project code is touched: Git to fetch the repository, Python to run the server, Visual Studio Build Tools to compile some Python dependencies, and (for one specific hardware setup) Arduino IDE and an axis stage driver.

Each tool below is installed the normal Windows way: download the installer file from the vendor's own website in a browser, then double-click the downloaded file in File Explorer to run its setup wizard. None of these are typed into a terminal — the filename shown for each one is just what the downloaded file is called, so you can confirm you got the right thing.

### 1.1 Install Git

Download installer:

```text
Git-2.46.0-64-bit.exe
```

This is the official 64-bit Git-for-Windows installer, downloaded from Git's own website (git-scm.com). Run the installer and keep all installation options as **default settings** unless the project maintainer gives different instructions.

---

### 1.2 Install Python 3.8.10

Download installer:

```text
python-3.8.10-amd64.exe
```

This is the official Python 3.8.10 64-bit Windows installer, downloaded from Python's own website (python.org). Run the installer and complete the Python installation. This project is built and tested against Python 3.8.10 specifically, so avoid installing a different version unless the project has already been tested with it.

---

### 1.3 Install Visual Studio Build Tools

Download installer:

```text
vs_BuildTools.exe
```

This is Microsoft's Visual Studio Build Tools installer, downloaded from Microsoft's Visual Studio downloads page. It doesn't install the full Visual Studio IDE — only the C++ compiler tools some Python packages need to build.

1. Run `vs_BuildTools.exe` and install the required C++ build tools.
2. Restart the computer once installation finishes — the build tools are not fully active until after a reboot.

---

### 1.4 Install Arduino IDE

This step is required only for the **Yuanman / 元滿** setup.

Download installer:

```text
arduino-ide_2.3.2_Windows_64bit.exe
```

This is the official Arduino IDE Windows installer, downloaded from Arduino's own website (arduino.cc). Run the installer, keep the default installation options, and accept any permission prompts Windows shows during installation.

---

### 1.5 Install Axis Stage Driver

This step is required only for the **Yuanman / 元滿** axis stage.

Driver file:

```text
KOHZU_USB_DRIVER.zip
```

This is a USB driver package provided by the axis stage's manufacturer (KOHZU), not something downloaded from a general software site — it should come from wherever your team keeps hardware driver files (for example, the O365 / OneDrive source mentioned above).

Extract the ZIP file, then install the driver manually. There is no dedicated Windows 11 installer for this hardware, so even on Windows 11 you use the Windows 7 driver directly:

Driver installation path:

```text
KOHZE_USB_DRIVE\CRUX_USB_DRIVE64\Windows7\CRUX_USB_DRIVE64.inf
```

Installation steps:

1. Extract `KOHZU_USB_DRIVER.zip`.
2. Open the extracted driver folder.
3. Go to:

   ```text
   KOHZE_USB_DRIVE\CRUX_USB_DRIVE64\Windows7
   ```

4. Right-click `CRUX_USB_DRIVE64.inf`.
5. Select **Install**.

---

## 2. Open CMD as Administrator

Most server setup and startup commands should be executed from **Windows CMD as Administrator**.

Steps:

1. Press **Windows Key + R**.
2. Type:

   ```text
   cmd
   ```

3. Press **Ctrl + Shift + Enter**.
4. Click **Yes** when Windows asks for administrator permission.

---

## 3. Clone the Project

### 3.1 Cache Git Username and Token

In Administrator CMD, run:

```bat
git config --global credential.helper cache
```

This allows Git to cache the GitHub username and token during clone or submodule operations.

---

### 3.2 Clone Repository

Go to the Documents folder:

```bat
cd C:\Users\%USERNAME%\Documents
```

Clone the project with submodules. This repository is private, so Git will prompt for a GitHub account or token with permission to access it:

```bat
git clone --recurse-submodules https://github.com/perseverance-tech-tw/moil-fisheye-calisys.git
```

---

### 3.3 Update Submodules

Move into the project folder:

```bat
cd moil-fisheye-calisys
```

Update submodules:

```bat
git submodule update --remote
```

---

## 4. Create Python Virtual Environment

A virtual environment keeps this project's Python packages separate from any other Python installation on the computer, so its dependencies don't conflict with anything else.

Inside the project folder, create a Python virtual environment:

```bat
python -m venv venv
```

Activate the virtual environment:

```bat
venv\Scripts\activate
```

After activation, the CMD line should show:

```text
(venv)
```

---

## 5. Install Python Modules

Run the following commands inside the activated virtual environment, in this exact order — the project needs these specific versions of `pip` and `setuptools` for the rest of the dependencies to install correctly:

```bat
pip install setuptools==59.6
python -m pip install pip==22.0
pip install wheel
pip install -r requirements.server
```

---

## 6. Install Moildev 2.7

Moildev 2.7 is a private package that isn't published to pip, so it must be installed manually into the virtual environment instead of via `pip install`.

Download these folders or ZIP files from the authorized O365 / OneDrive source:

```text
Moildev 2.7.zip
Moildev-2.7.0.dist-info.zip
```

Extract both items into:

```text
C:\Users\%USERNAME%\Documents\moil-fisheye-calisys\venv\Lib\site-packages\
```

Expected result:

```text
moil-fisheye-calisys/
└── venv/
    └── Lib/
        └── site-packages/
            ├── Moildev 2.7/
            └── Moildev-2.7.0.dist-info/
```

Double-check both folders end up inside the virtual environment's `site-packages` folder shown above, not in the project root — Python will not find Moildev otherwise.

---

## 7. Check Hardware Configuration Before Startup

Before starting any HTTP server, confirm that the hardware is connected and configured correctly.

| Hardware | Check Item |
|---|---|
| **Axis Stage** | USB cable connected, driver installed, COM port known. |
| **Monitor Displays** | All calibration monitors connected and detected by Windows. |
| **Camera** | Camera connected and correct driver selected. |
| **Network** | Server computer connected to the same network as the client computer. |

---

## 8. Configure Axis USB COM Port

The Axis HTTP Server talks to the axis stage over a serial connection, so it needs to know the exact USB COM port the stage is connected to — this must match the physical hardware or the server won't be able to control the stage.

### 8.1 Yuanman / 元滿 Axis Module

For Yuanman hardware, edit:

```text
moil-fisheye-calisys\mvc_model\moil_axis\axis_module\axis_module_yuanman.py
```

Check and update the USB COM port according to the connected axis controller. Set the baud rate to match the document provided by the Yuanman company.

---

### 8.2 YinDa / 盈達 Axis Module

For YinDa hardware, edit:

```text
moil-fisheye-calisys\mvc_model\moil_axis\axis_module\axis_module_yinda.py
```

Check and update the USB COM port according to the connected axis controller.

---

## 9. Start HTTP Server - Axis

The Axis, Monitor, and Camera servers each run as their own long-lived process, so each one needs its own CMD terminal window that stays open while the server is in use.

Open a new **Windows CMD terminal as Administrator**.

Go to the project folder:

```bat
cd C:\Users\%USERNAME%\Documents\moil-fisheye-calisys
```

Activate the virtual environment:

```bat
.\venv\Scripts\activate.bat
```

Start the Axis HTTP Server:

```bat
python .\mvc_model\moil_axis\axis_http_server.py
```

The Axis server should run on:

```text
http://<Server IP>:8000
```

---

## 10. Start HTTP Server - Monitor

Open another new **Windows CMD terminal as Administrator**.

Go to the project folder:

```bat
cd C:\Users\%USERNAME%\Documents\moil-fisheye-calisys
```

Activate the virtual environment:

```bat
.\venv\Scripts\activate.bat
```

Start the Monitor HTTP Server:

```bat
python .\mvc_model\moil_monitor\monitor_http_server.py
```

The Monitor server should run on:

```text
http://<Server IP>:8001
```

---

## 11. Start HTTP Server - Camera

Before starting the Camera server, check the camera driver selection inside:

```text
moil-fisheye-calisys\mvc_model\moil_camera\camera_http_server.py
```

Select the correct camera driver by commenting or uncommenting the required code section.

Then open another new **Windows CMD terminal as Administrator**.

Go to the project folder:

```bat
cd C:\Users\%USERNAME%\Documents\moil-fisheye-calisys
```

Activate the virtual environment:

```bat
.\venv\Scripts\activate.bat
```

Start the Camera HTTP Server:

```bat
python .\mvc_model\moil_camera\camera_http_server.py
```

The Camera server should run on:

```text
http://<Server IP>:8002
```

---

## 12. Check Server IP Address

Open a normal Windows CMD terminal.

Run:

```bat
ipconfig
```

Find the server computer IPv4 address.

Example:

```text
Server IP Address = 192.168.113.52
```

This IP address is used by the calibration client to connect to the server.

---

## 13. Verify API Documentation

After all three HTTP servers are running, open a browser and check each API documentation page.

### 13.1 Axis API

```text
http://<Server IP>:8000/docs
```

Example:

```text
http://192.168.113.52:8000/docs
```

---

### 13.2 Monitor API

```text
http://<Server IP>:8001/docs
```

Example:

```text
http://192.168.113.52:8001/docs
```

---

### 13.3 Camera API

```text
http://<Server IP>:8002/docs
```

Example:

```text
http://192.168.113.52:8002/docs
```

If each `/docs` page opens successfully, that HTTP server is running and can receive API requests.

---

## 14. Final Monitor Direction Assignment

The Monitor HTTP Server uses `pywin32` to read monitor display numbers from the Windows system.

However, Windows may randomly change the display number of each monitor after the computer is restarted. Because of this, the user must manually assign which Windows display number belongs to each calibration direction.

---

### 14.1 Show Display Number

Send this request to the Monitor HTTP Server:

```text
/show_display_number
```

All display numbers will be shown on each monitor as an image or pattern.

Use the Monitor API docs page:

```text
http://<Server IP>:8001/docs
```

Then execute the `/show_display_number` API.

---

### 14.2 Check Display Number

Observe the physical monitors and record which display number appears on each monitor.

Example:

| Direction | Display Number |
|---|---:|
| Top | 1 |
| North | 2 |
| South | 3 |
| East | 4 |
| West | 5 |

The actual numbers may be different on your computer.

---

### 14.3 Set Display Number

Send this request to the Monitor HTTP Server:

```text
/set_display_number
```

Set the display number for each monitor direction according to the numbers shown on the physical monitors.

Use the Monitor API docs page:

```text
http://<Server IP>:8001/docs
```

Then execute the `/set_display_number` API with the correct parameters.

---

### 14.4 Close All Patterns

After verifying monitor direction assignment, close all displayed patterns using the related Monitor API endpoint.

This ensures the monitor display is clean before starting calibration.

---

## 15. Server URL Used by Client

After the server is running, enter these URLs in the Calibration System Client.

| Client Field | URL Format | Example |
|---|---|---|
| **Axis URL** | `http://<Server IP>:8000` | `http://192.168.113.52:8000` |
| **Monitor URL** | `http://<Server IP>:8001` | `http://192.168.113.52:8001` |
| **Camera URL** | `http://<Server IP>:8002` | `http://192.168.113.52:8002` |

In the client main window, press the **Update** button for each URL after entering the address.

---

## Troubleshooting

| Problem | Possible Cause | Solution |
|---|---|---|
| Git clone fails | GitHub authentication is missing or repository permission is not granted. | Login with the correct GitHub account or use a valid token. |
| `python` command not found | Python was not added to PATH or installation failed. | Reinstall Python 3.8.10 and make sure Python can be called from CMD. |
| Module import error | Python dependencies were not installed correctly. | Activate `venv` and run `pip install -r requirements.server` again. |
| Moildev not found | Moildev was extracted to the wrong folder. | Extract Moildev into `venv\Lib\site-packages\`. |
| Axis server cannot connect to stage | Wrong COM port or driver not installed. | Check Device Manager, update COM port in the correct axis module file, and confirm driver installation. |
| Monitor server runs but wrong monitor displays pattern | Windows display number changed. | Run `/show_display_number`, observe the monitors, then run `/set_display_number`. |
| Camera server cannot capture image | Wrong camera driver selected. | Check `camera_http_server.py` and select the correct driver code section. |
| Browser cannot open `/docs` | Server is not running or wrong IP/port used. | Confirm the CMD server process is still running and check the server IP using `ipconfig`. |
| Client cannot connect to server | Client and server are not on the same network. | Check network connection, firewall settings, and server IP address. |

---

## Complete Server Startup Checklist

Before using the client, confirm the following:

| Check | Status |
|---|---|
| Git installed | ☐ |
| Python 3.8.10 installed | ☐ |
| Visual Studio Build Tools installed | ☐ |
| Project cloned successfully | ☐ |
| Submodules updated | ☐ |
| Python virtual environment created | ☐ |
| Python modules installed | ☐ |
| Moildev 2.7 installed into `site-packages` | ☐ |
| Axis COM port configured | ☐ |
| Camera driver selected | ☐ |
| Axis HTTP Server running | ☐ |
| Monitor HTTP Server running | ☐ |
| Camera HTTP Server running | ☐ |
| Server IP checked using `ipconfig` | ☐ |
| Axis `/docs` page opens | ☐ |
| Monitor `/docs` page opens | ☐ |
| Camera `/docs` page opens | ☐ |
| Monitor direction assignment completed | ☐ |