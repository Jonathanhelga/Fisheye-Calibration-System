@echo off
REM Is the server up, and is it right? One pass, PASS/FAIL per rung, no GUI.
REM
REM Run this ON THE RIG after run_server.bat, or from a client machine to check
REM the link as well. Each rung isolates a different layer and they are in this
REM order on purpose -- diagnosing rung 5 before rung 3 passes is wasted work.
REM
REM Rung 3 empty while rung 1 passes is a NETWORK problem, not a server problem:
REM almost always inbound UDP blocked, or a leftover ROS_DISCOVERY_SERVER.
REM
REM Nothing to edit here; see scripts\moil_env.bat.

setlocal
set "RIG=%1"
if "%RIG%"=="" set "RIG=localhost"

call "%~dp0scripts\moil_env.bat"
if errorlevel 1 exit /b 1

if defined PIXI_IN_SHELL goto :main
if not defined PIXI_MANIFEST goto :main
set "PATH=%LOCALAPPDATA%\pixi\bin;%PATH%"
pixi run --manifest-path "%PIXI_MANIFEST%" cmd /c "%~f0" %*
exit /b %ERRORLEVEL%

:main
set "ROS_DISCOVERY_SERVER="
set "FASTDDS_DEFAULT_PROFILES_FILE="
set "FASTRTPS_DEFAULT_PROFILES_FILE="
set ROS_DOMAIN_ID=42
set RMW_IMPLEMENTATION=rmw_fastrtps_cpp
if defined ROS_LOCAL_SETUP call "%ROS_LOCAL_SETUP%"
REM Tolerated when absent: this script is also run from a client machine, which
REM has no server install space and does not need one to see the rig.
if exist "%MOIL_INSTALL_BASE%\setup.bat" call "%MOIL_INSTALL_BASE%\setup.bat" 2>nul

echo.
echo === 1. the daemon caches the old environment; clear it first ===
ros2 daemon stop >nul 2>&1

echo.
echo === 2. discovery: which nodes can be seen? ===
echo     expect: /moil_axis /moil_camera /moil_monitor /moil_compute
echo             /moil_session /moil_measure3d /moil_jobs /moil_supervisor
ros2 node list --spin-time 8

echo.
echo === 3. the interface contract ===
echo     a type-hash mismatch makes every call of that service fail to connect
echo     WHILE THE TOPICS KEEP FLOWING -- the rig looks alive and no button works.
ros2 service type /monitor/show_pattern_spec
ros2 interface show moil_interfaces/srv/ShowPatternSpec ^| findstr /C:"spec_json"

echo.
echo === 4. is the rig answering, and what did it find? ===
ros2 service call /rig/info moil_interfaces/srv/RigInfo "{}"

echo.
echo === 5. does the camera actually stream? ===
echo     discovery can succeed while data cannot come back
ros2 topic hz /camera/image_raw/compressed --window 20

echo.
echo === 6. THE CALCULATION CHECK ===
echo     every rung above tests the transport. This one tests the maths:
echo     the pipeline is run against a fixed fixture and compared to the
echo     recorded baseline. A compute node with a changed formula answers
echo     instantly, successfully, and wrongly -- and passes rungs 1-5.
ros2 service call /compute/self_test moil_interfaces/srv/SelfTest "{suite: 'all'}"

echo.
echo === 7. the live status the server console is printing ===
ros2 topic echo /system/status --once

echo.
echo Done. If rung 2 listed the nodes and rung 6 says passed=true, the server is
echo healthy and its arithmetic has not moved since the baseline was recorded.
pause
