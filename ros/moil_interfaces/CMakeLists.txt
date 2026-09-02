cmake_minimum_required(VERSION 3.8)
project(moil_interfaces)

# The contract between the two processes.
#
# The twelve axis and monitor definitions below are BYTE-IDENTICAL to the ones
# running on the rig in V2.0.0. That is deliberate and it is not sentimentality:
# ROS matches services by type hash, so a definition that differs by one
# character makes every call of that service fail to connect -- while the topics
# keep flowing, so the rig looks alive and no button works. Leaving them exactly
# as they are means a v2.1.0 client can drive a v2.0.0 rig node, and a v2.1.0
# server answers a v2.0.0 client, for everything those twelve cover.
#
# Everything after them is new surface for the work that used to happen inside
# the client: the calibration pipeline, the plot series, the session store, the
# 3D verification, and the long jobs.

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  # --- Axis: unchanged from V2.0.0 ----------------------------------------
  "msg/AxisSensors.msg"
  "srv/AxisMove.srv"
  "srv/AxisCommand.srv"
  "srv/AxisSensor.srv"
  # --- Monitor: unchanged from V2.0.0 -------------------------------------
  "srv/ShowPattern.srv"
  "srv/ShowPatternSpec.srv"
  # Prepared patterns: render once, keep the PNGs, blit them at shot time.
  "srv/PreparePatterns.srv"
  "srv/ShowPrepared.srv"
  "srv/ClosePattern.srv"
  "srv/SetBrightness.srv"
  "srv/GetBrightness.srv"
  "srv/SetDisplayDirection.srv"
  "srv/GetDisplayDirection.srv"
  "srv/MonitorCommand.srv"

  # --- Axis: new in v2.1.0 -------------------------------------------------
  # The counter-to-coordinate conversion and the software zero it needs, moved
  # off the client.
  "srv/AxisPosition.srv"
  "srv/AxisZero.srv"
  "srv/RigInfo.srv"
  # The sensor/position stream that replaced per-client polling.
  "msg/AxisState.msg"
  "srv/AxisWatch.srv"

  # --- Camera --------------------------------------------------------------
  "srv/Capture.srv"

  # --- Monitor: new in v2.1.0 ----------------------------------------------
  "srv/DescribeScreens.srv"
  "srv/RenderForDirection.srv"

  # --- Compute -------------------------------------------------------------
  "srv/DetectOp.srv"
  "srv/CaliOp.srv"
  "srv/CaliSeries.srv"
  "srv/RenderPattern.srv"
  "srv/Anypoint.srv"
  "srv/Panorama.srv"
  "srv/MoildevInfo.srv"
  "srv/XlsxIo.srv"

  # --- 3D verification -----------------------------------------------------
  "srv/Measure3dSource.srv"
  "srv/DetectCheckerboard.srv"
  "srv/Verify3d.srv"

  # --- Session -------------------------------------------------------------
  "msg/SessionEvent.msg"
  "srv/SessionCommand.srv"
  "srv/SessionEdit.srv"
  "srv/SessionState.srv"
  "srv/SessionCapture.srv"
  "srv/SessionImage.srv"

  # --- Health --------------------------------------------------------------
  "msg/SystemStatus.msg"
  "srv/SelfTest.srv"

  # --- Long jobs: cancellable, with progress -------------------------------
  "action/CaliJob.action"
  "action/RunCompute.action"
  "action/AutoCalibrate.action"
  "action/AxisLimitMove.action"
  "action/AxisHome.action"
  "action/AutoFrame.action"

  DEPENDENCIES std_msgs sensor_msgs
)

ament_package()
