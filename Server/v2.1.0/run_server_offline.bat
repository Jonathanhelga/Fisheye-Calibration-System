@echo off
REM Start the MoilCali server FOR DESK WORK -- no rig, and unable to reach one.
REM
REM Use this when you want the measurement path without the rig: load a saved
REM capture into the app with Open Img, then Find Pos / Find Neg / Direction Diff
REM run against this server exactly as they would against the rig's. The compute
REM node needs no hardware. It comes up "compute OK -- self-test passed, max drift
REM 0" on a laptop while "axis FAILED -- COM4 is not found", because the nodes
REM fail independently.
REM
REM Same engine as the rig, because it IS the rig's server binary. Nothing here
REM re-implements any maths, so a desk answer and a rig answer cannot disagree.
REM
REM ------------------------------------------------------------------------
REM WHY THIS IS A SEPARATE FILE RATHER THAN A FLAG YOU REMEMBER TO PASS
REM ------------------------------------------------------------------------
REM
REM On 2026-09-09 a server was started on domain 42 on a developer laptop while
REM the rig was live on the same domain. It advertises /moil_axis /moil_camera
REM /moil_monitor /moil_compute ... under the SAME node and service names as the
REM rig, with the same type hashes, because the interfaces are identical. A client
REM on 42 therefore discovers BOTH: both receive every request and both answer,
REM and which reply the client keeps is not something you can rely on.
REM
REM Concretely, one Capture press fires the rig's camera AND the laptop's webcam;
REM one Show on Monitor can put a pattern on the rig's calibration panels from
REM someone's desk test. Nothing logs a warning, because nothing is wrong as far
REM as DDS is concerned.
REM
REM So the safe thing is not a flag on the normal launcher -- it is a launcher you
REM cannot run by accident while meaning to start the rig.
REM
REM ------------------------------------------------------------------------
REM WHAT IT DOES DIFFERENTLY, AND WHY BOTH GUARDS, NOT ONE
REM ------------------------------------------------------------------------
REM
REM   ROS_DOMAIN_ID       a domain of its own, so there is no overlap at all
REM   ROS_LOCALHOST_ONLY  set, so even a domain mistake cannot reach the LAN
REM
REM One variable sets both, in run_server.bat. A domain change WITHOUT
REM localhost-only still broadcasts -- just somewhere else -- and sooner or later
REM someone sets one and forgets the other.
REM
REM This file is a wrapper, not a copy. Everything else -- finding Qt, OpenCV and
REM the ROS distro, the pixi re-invocation, the install space -- happens once, in
REM run_server.bat. A duplicate would drift from it silently the first time that
REM script gained a line, and a server that starts and then misbehaves is a much
REM worse afternoon than one that does not start.
REM
REM Pass a domain to override the default:  run_server_offline.bat 77
REM Anything after that goes on to the server, as with run_server.bat.

if "%~1"=="" (
  set "MOIL_OFFLINE_DOMAIN=91"
) else (
  set "MOIL_OFFLINE_DOMAIN=%~1"
  shift
)

echo.
echo   Desk server -- domain %MOIL_OFFLINE_DOMAIN%, localhost only.
echo   The rig cannot see this, and this cannot see the rig.
echo   Set the app's Server panel to %MOIL_OFFLINE_DOMAIN% and press Update.
echo.

call "%~dp0run_server.bat" %*
