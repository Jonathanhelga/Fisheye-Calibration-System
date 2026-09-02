@echo off
REM One-shot dump of /system/status into st.txt, for pasting into a bug report.
REM The server must already be running; this only reads.
setlocal
call "%~dp0scripts\moil_env.bat"
if errorlevel 1 exit /b 1
if defined ROS_LOCAL_SETUP call "%ROS_LOCAL_SETUP%"
if exist "%MOIL_INSTALL_BASE%\setup.bat" call "%MOIL_INSTALL_BASE%\setup.bat"
set "ROS_DISCOVERY_SERVER="
set ROS_DOMAIN_ID=42
cd /d "%~dp0"
ros2 topic echo /system/status --once > "%~dp0st.txt" 2>&1
