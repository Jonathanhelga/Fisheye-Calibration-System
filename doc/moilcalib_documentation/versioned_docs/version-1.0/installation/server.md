---
id: server
slug: /installation/server
title: Server Installation
sidebar_label: Server Installation
---

# Server Installation

This page explains how to install and configure the **Calibration System Server** on **Windows 11 x64**.

The server computer is responsible for running the HTTP services used by the calibration client. These services control the axis stage, monitor display, and camera capture system.

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

<div className="custom-note custom-important">
  <div className="custom-note-title">📌 Read First</div>
  <p>This installation guide is for the server-side computer. The server must run three HTTP services: <strong> Axis Server</strong>, <strong>Monitor Server</strong>, and <strong>Camera Server</strong>. The calibration client connects to these services through the server IP address.</p>
</div>

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

### 1.1 Install Git

Download installer:

```text
Git-2.46.0-64-bit.exe
```

Run the installer and keep all installation options as **default settings**.

<div className="custom-note custom-tip">
  <div className="custom-note-title">Recommended</div>
  <p>Use the default Git installation settings unless the project maintainer gives different instructions.</p>
</div>

---

### 1.2 Install Python 3.8.10

Download installer:

```text
python-3.8.10-amd64.exe
```

Run the installer and complete the Python installation.

<div className="custom-note custom-warning">
  <div className="custom-note-title">Important</div>
  <p>This project uses Python 3.8.10. Avoid installing a different Python version unless the project has already been tested with that version.</p>
</div>

---

### 1.3 Install Visual Studio Build Tools

Download installer:

```text
vs_BuildTools.exe
```

Run the installer and install the required C++ build tools.

<div className="custom-note custom-important">
  <div className="custom-note-title">Restart Required</div>
  <p>Restart the computer after Visual Studio Build Tools installation is complete.</p>
</div>

---

### 1.4 Install Arduino IDE

This step is required only for the **Yuanman / 元滿** setup.

Download installer:

```text
arduino-ide_2.3.2_Windows_64bit.exe
```

Run the installer and keep the default installation options.

<div className="custom-note custom-warning">
  <div className="custom-note-title">Permission Notice</div>
  <p>During installation, accept all required permission inquiries.</p>
</div>

---

### 1.5 Install Axis Stage Driver

This step is required only for the **Yuanman / 元滿** axis stage.

Driver file:

```text
KOHZU_USB_DRIVER.zip
```

Extract the ZIP file, then install the driver manually.

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

<div className="custom-note custom-important">
  <div className="custom-note-title">Windows 11 Driver Note</div>
  <p>For Windows 11, use the Windows 7 installer directly. The KOHZU Windows 7 driver is used because a dedicated Windows 11 installer is not provided.</p>
</div>

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

Clone the project with submodules:

```bat
git clone --recurse-submodules https://github.com/perseverance-tech-tw/moil-fisheye-calisys.git
```

<div className="custom-note custom-warning">
  <div className="custom-note-title">GitHub Authentication Required</div>
  <p>This repository may require GitHub authentication. Use a GitHub account or token that has permission to access the repository.</p>
</div>

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

Run the following commands inside the activated virtual environment:

```bat
pip install setuptools==59.6
python -m pip install pip==22.0
pip install wheel
pip install -r requirements.server
```

<div className="custom-note custom-tip">
  <div className="custom-note-title">Installation Order</div>
  <p>Follow the command order above. The project requires specific versions of pip and setuptools for stable dependency installation.</p>
</div>

---

## 6. Install Moildev 2.7

Moildev 2.7 must be installed manually into the virtual environment.

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

<div className="custom-note custom-warning">
  <div className="custom-note-title">Important</div>
  <p>Make sure Moildev is extracted into the virtual environment's <code>site-packages</code> folder, not into the project root folder.</p>
</div>

---

## 7. Check Hardware Configuration Before Startup

Before starting any HTTP server, confirm that the hardware is connected and configured correctly.

| Hardware | Check Item |
|---|---|
| **Axis Stage** | USB cable connected, driver installed, COM port known. |
| **Monitor Displays** | All calibration monitors connected and detected by Windows. |
| **Camera** | Camera connected and correct driver selected. |
| **Network** | Server computer connected to the same network as the client computer. |

<div className="custom-note custom-important">
  <div className="custom-note-title">Hardware Check</div>
  <p>Do not start calibration testing before the axis stage, camera, and monitor hardware are connected correctly.</p>
</div>

---

## 8. Configure Axis USB COM Port

The Axis HTTP Server requires the correct USB COM port.

### 8.1 Yuanman / 元滿 Axis Module

For Yuanman hardware, edit:

```text
moil-fisheye-calisys\mvc_model\moil_axis\axis_module\axis_module_yuanman.py
```

Check and update the USB COM port according to the connected axis controller.

<div className="custom-note custom-warning">
  <div className="custom-note-title">Baud Rate Note</div>
  <p>The baud rate should follow the document provided by the Yuanman company.</p>
</div>

---

### 8.2 YinDa / 盈達 Axis Module

For YinDa hardware, edit:

```text
moil-fisheye-calisys\mvc_model\moil_axis\axis_module\axis_module_yinda.py
```

Check and update the USB COM port according to the connected axis controller.

---

## 9. Start HTTP Server - Axis

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

<div className="custom-note custom-tip">
  <div className="custom-note-title">Verification Result</div>
  <p>If the <code>/docs</code> page opens successfully, the HTTP server is running and can receive API requests.</p>
</div>

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

---

## Summary

The **Server Installation** process prepares the Windows 11 x64 server computer to control the calibration system hardware.

The installation flow is:

```text
Install required software
   ↓
Install hardware driver
   ↓
Clone project repository
   ↓
Create Python virtual environment
   ↓
Install Python modules
   ↓
Install Moildev 2.7
   ↓
Configure Axis COM port and Camera driver
   ↓
Start Axis, Monitor, and Camera HTTP servers
   ↓
Check server IP address
   ↓
Verify /docs pages
   ↓
Assign monitor display directions
```

After all server services are running successfully, continue with the **Client Installation Guide** and enter the server URLs into the client application.