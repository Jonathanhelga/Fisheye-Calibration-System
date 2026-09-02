---
id: 3d-verification
slug: /system-overview/3d-verification
title: 3D Verification
sidebar_label: 3D Verification
---

# 3D Verification

The **3D Verification** dialog provides automated testing and validation of calibration results through 3D measurement sequences.

---

## Overview

The 3D verification system performs automated measurement tests to validate calibration accuracy and system reliability. It uses the calibrated camera parameters to measure known 3D targets and compares results against expected values.

---

## 1. Main Components

### 1.1 Control Panel

| Component | Function |
|---|---|
| **Direction Input** | Specifies the measurement direction/path for the verification sequence |
| **Round Input** | Defines the number of measurement rounds to perform |
| **Start Button** | Initiates the automated verification sequence |
| **Stop Button** | Terminates the verification process immediately |

### 1.2 Display Areas

| Display Area | Content |
|---|---|
| **Left Camera Original** | Shows the original camera feed for the left camera |
| **Left Camera Detection** | Displays detected features/targets in the left camera view |
| **Right Camera Original** | Shows the original camera feed for the right camera |
| **Right Camera Detection** | Displays detected features/targets in the right camera view |
| **Result Text Browser** | Logs measurement results and system status messages |

---

## 2. Verification Workflow

### 2.1 Setup Phase
- Configure measurement direction and number of rounds
- Initialize camera connections and calibration parameters
- Home the X-axis to establish reference position

### 2.2 Measurement Sequence
- Move axis to starting position
- Capture synchronized stereo images
- Detect target features in both camera views
- Calculate 3D coordinates using calibrated parameters
- Log results and move to next position

### 2.3 Analysis Phase
- Compare measured coordinates against expected values
- Calculate accuracy metrics and error statistics
- Generate verification report

---

## 3. Controller Implementation

The `Controller3dMeasurement` class manages the verification process with the following key features:

- **Threading**: Uses background threads for non-blocking UI operation
- **Axis Integration**: Coordinates with the main axis controller for precise positioning
- **Image Processing**: Implements real-time feature detection and 3D reconstruction
- **Result Logging**: Maintains detailed logs of all measurement attempts

---

## 4. Safety Features

- **Emergency Stop**: Immediate termination capability at any point
- **Position Validation**: Verifies axis position before each measurement
- **Error Handling**: Graceful handling of detection failures and communication errors

---

## Usage Guidelines

<div className="custom-note custom-important">
  <div className="custom-note-title">✅ Verification Prerequisites</div>
  <ol>
    <li>Complete calibration process with valid results</li>
    <li>Ensure camera connections are stable</li>
    <li>Verify axis homing and positioning accuracy</li>
    <li>Set appropriate measurement parameters</li>
  </ol>
</div>

<div className="custom-note custom-warning">
  <div className="custom-note-title">⚠️ Safety Considerations</div>
  <p>Keep clear of the measurement area during automated sequences. The system moves axes automatically and may cause injury if safety precautions are not observed.</p>
</div>