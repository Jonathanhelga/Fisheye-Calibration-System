@echo off
REM Start the MoilCali server. One window, one process, everything running.
REM
REM Leave this window OPEN -- it is the server. It prints a status table that
REM refreshes whenever something changes:
REM
REM   [14:22:07]
REM   MoilCali server 2.1.0     up 00:41:12     clients: 1
REM   ----------------------------------------------------------------------
REM   axis       OK        yuanman: arduino=COM3 crux=COM4
REM   camera     OK        id=0 mf 3040x3040
REM   monitor    DEGRADED  7 screens, 5 mapped
REM   compute    OK        self-test passed 12s ago, max drift 0
REM   session    OK        3 sessions, none open
REM   jobs       OK        idle
REM
REM The `compute` line is the one worth reading. It is not a ping: the server runs
REM the calibration pipeline against a fixed fixture and compares the answers to
REM the recorded baseline. OK there means the maths still gives the numbers it
REM gave. See SelfTest.srv for what that does and does not prove.
REM
REM STOP WITH Ctrl+C, NOT by closing the window. Force-killing skips the serial
REM port close, and the next start reads sensor values that are garbage but look
REM plausible.
REM
REM ONLY ONE AT A TIME. Two servers fight over COM3/COM4 and the second one gets
REM "Access is denied" permanently.
REM
REM Nothing to edit here. Qt, OpenCV and the ROS distro are found by
REM scripts\moil_env.bat, and the install space is the one build_server.bat wrote
REM -- both sides read it from the same script, so they cannot drift apart.

call "%~dp0scripts\moil_env.bat"
if errorlevel 1 exit /b 1

if not exist "%MOIL_INSTALL_BASE%\setup.bat" (
  echo Nothing is built: %MOIL_INSTALL_BASE% has no setup.bat.
  echo Run build_server.bat first.
  pause
  exit /b 1
)

if defined PIXI_IN_SHELL goto :main
if not defined PIXI_MANIFEST goto :main
set "PATH=%LOCALAPPDATA%\pixi\bin;%PATH%"
pixi run --manifest-path "%PIXI_MANIFEST%" cmd /c "%~f0" %*
exit /b %ERRORLEVEL%

:main
REM Plain LAN multicast, exactly as the rig has always run. These three are
REM blanked deliberately: a leftover discovery-server or Fast DDS profile makes
REM discovery silently find nothing, which looks like a broken network and is not.
set "ROS_DISCOVERY_SERVER="
set "FASTDDS_DEFAULT_PROFILES_FILE="
set "FASTRTPS_DEFAULT_PROFILES_FILE="
set RMW_IMPLEMENTATION=rmw_fastrtps_cpp

REM OFFLINE MODE -- a server on a developer machine, for working without the rig.
REM Added 2026-09-09.
REM
REM The compute node needs no hardware, so a laptop can serve the whole
REM measurement path against saved captures. What it must NEVER do is serve it on
REM domain 42. This server advertises /moil_axis /moil_camera /moil_monitor
REM /moil_compute ... under the same names as the rig and with the same type
REM hashes, so a client on 42 discovers BOTH, both receive every request, and both
REM answer. Which reply the client keeps is not something you can rely on. One
REM Capture press then fires the rig's camera AND the laptop's webcam; one Show on
REM Monitor can put a pattern on the rig's calibration panels from a desk test.
REM
REM That happened on 2026-09-09.
REM
REM ONE VARIABLE SETS BOTH GUARDS, and that is the point -- a domain change
REM without localhost-only still broadcasts, just somewhere else, and someone will
REM eventually set one and forget the other:
REM
REM   set MOIL_OFFLINE_DOMAIN=91
REM   run_server.bat
REM
REM Then type the same number into the app's Server panel. Deliberately its own
REM variable rather than honouring a bare ROS_DOMAIN_ID: a stray ROS_DOMAIN_ID in
REM someone's profile must never be able to quietly move the RIG's server off 42.
if defined MOIL_OFFLINE_DOMAIN (
  set "ROS_DOMAIN_ID=%MOIL_OFFLINE_DOMAIN%"
  set "ROS_LOCALHOST_ONLY=1"
  echo OFFLINE MODE: domain %MOIL_OFFLINE_DOMAIN%, localhost only -- the rig cannot see this.
) else (
  REM Plain LAN multicast, exactly as the rig has always run. ROS_LOCALHOST_ONLY
  REM is blanked deliberately: a leftover value makes discovery silently find
  REM nothing, which looks like a broken network and is not.
  set "ROS_LOCALHOST_ONLY="
  set ROS_DOMAIN_ID=42
)

if defined ROS_LOCAL_SETUP call "%ROS_LOCAL_SETUP%"
call "%MOIL_INSTALL_BASE%\setup.bat"

REM Qt and OpenCV DLLs. Without this the server dies at load with exit code
REM -1073741515 (0xC0000135, STATUS_DLL_NOT_FOUND) and produces no output at all
REM -- which looks exactly like a silent crash.
set "PATH=%QT_BIN%;%OPENCV_BIN%;%PATH%"
set "QT_PLUGIN_PATH=%QT_PLUGINS%"

REM The working directory is the server folder, not the install space: the
REM sessions the server writes and the calibration images it reads are addressed
REM from here.
cd /d "%~dp0"
echo Starting MoilCali server 2.1.0 on ROS_DOMAIN_ID=%ROS_DOMAIN_ID% ...
echo.
ros2 run moil_server moil_server %*

echo.
echo Server exited with code %ERRORLEVEL%.
pause
