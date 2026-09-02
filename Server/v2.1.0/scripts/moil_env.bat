@echo off
REM ===========================================================================
REM moil_env.bat -- the one place that knows where anything on this machine is.
REM
REM call'ed by build_server.bat, run_server.bat, check_server.bat and _st.bat.
REM No other script in this tree may contain an absolute path. If you find one,
REM it is a bug: it works on the rig and nowhere else, and it fails by finding
REM the WRONG thing rather than by finding nothing.
REM
REM Everything INSIDE the workspace is derived from this script's own location,
REM so the tree can be moved, renamed, copied to a second machine or checked out
REM under a different name and the scripts keep working untouched.
REM
REM Everything OUTSIDE it -- Qt, OpenCV, Eigen, the ROS distro, MSVC -- cannot be
REM relative to anything, so it is SEARCHED for, first hit wins:
REM
REM   1. the variable is already set in the environment
REM   2. server\deps.local.bat, if it exists (yours; not meant to be committed)
REM   3. <workspace>\third_party\..., if you vendored a copy into the tree
REM   4. the usual places on a Windows dev box, newest version last-and-winning
REM
REM The normal case needs no configuration. The unusual case needs one file that
REM is yours alone: copy deps.local.bat.example to deps.local.bat and override
REM only the lines you actually need.
REM
REM     scripts\moil_env.bat --check      show what it resolved, and exit
REM
REM Exports, all absolute, all discovered rather than assumed:
REM
REM   MOIL_ROOT          the workspace root (the folder holding server\ client\)
REM   MOIL_SERVER_DIR    <MOIL_ROOT>\server
REM   QT_DIR             Qt prefix, e.g. ...\6.5.3\msvc2019_64
REM   QT_CMAKE_DIR       <QT_DIR>\lib\cmake\Qt6      -- for -DQt6_DIR
REM   QT_BIN             <QT_DIR>\bin                -- DLLs, for PATH
REM   QT_PLUGINS         <QT_DIR>\plugins            -- for QT_PLUGIN_PATH
REM   OPENCV_DIR         ...\build\x64\vc15\lib      -- for -DOpenCV_DIR
REM   OPENCV_BIN         ...\build\x64\vc15\bin      -- DLLs, for PATH
REM   EIGEN_DIR          optional; usually empty, see :resolve_eigen
REM   PIXI_MANIFEST      pixi.toml of the ROS distro; empty if ROS is on PATH
REM   ROS_LOCAL_SETUP    the distro's local_setup.bat
REM   VCVARS             vcvars64.bat
REM   MOIL_WS_BASE       short scratch root for colcon -- see :resolve_ws
REM   MOIL_BUILD_BASE    <MOIL_WS_BASE>\build
REM   MOIL_INSTALL_BASE  <MOIL_WS_BASE>\install
REM   MOIL_LOG_BASE      <MOIL_WS_BASE>\log
REM ===========================================================================

set "MOIL_ENV_ERROR="

REM %ProgramFiles(x86)% is captured here, at the top, and used as %_PF86%
REM everywhere below. Its name contains parentheses, and a %VAR(x)% reference
REM inside a parenthesised if/for block closes the block early -- the script then
REM dies with a syntax error pointing at a line that is perfectly correct.
set "_PF86=%ProgramFiles(x86)%"
set "_PF64=%ProgramFiles%"

REM %%~fI normalises: it resolves the .. and drops the trailing backslash, so
REM every path built from these is clean rather than C:\a\server\scripts\..\..
for %%I in ("%~dp0..") do set "MOIL_SERVER_DIR=%%~fI"
for %%I in ("%MOIL_SERVER_DIR%\..") do set "MOIL_ROOT=%%~fI"

REM Your overrides, before any searching, so a value you set is never second-
REM guessed by the search below.
if exist "%MOIL_SERVER_DIR%\deps.local.bat" call "%MOIL_SERVER_DIR%\deps.local.bat"

REM --need-msvc: only the two scripts that COMPILE ask for it.
REM
REM MSVC is a build dependency and nothing else. run_server.bat, check_server.bat
REM and _st.bat all call this script and none of them compile
REM anything -- but :resolve_vcvars used to fail outright when Visual Studio was
REM absent, which set MOIL_ENV_ERROR and took the whole script to `exit /b 1`.
REM
REM That is the rig's own situation the moment the server is deployed rather than
REM built there: a machine with the ROS distro, Qt and OpenCV but no compiler could
REM not START the server it already had, and the message it printed told the
REM operator to install Visual Studio.
set "MOIL_NEED_MSVC="
for %%A in (%*) do if /i "%%~A"=="--need-msvc" set "MOIL_NEED_MSVC=1"

call :resolve_qt
call :resolve_opencv
call :resolve_eigen
call :resolve_pixi
call :resolve_ros
call :resolve_vcvars
call :resolve_ws

if defined MOIL_NEED_MSVC if not defined VCVARS call :fail "MSVC (vcvars64.bat)" "Install Visual Studio 2019/2022 with the 'Desktop development with C++' workload, or set VCVARS in server\deps.local.bat"

for %%A in (%*) do if /i "%%~A"=="--check" goto :report
if defined MOIL_ENV_ERROR goto :unresolved
exit /b 0


REM ---------------------------------------------------------------------------
:resolve_qt
REM Wanted: the Qt PREFIX (the msvc*_64 folder), not its lib\cmake\Qt6. The
REM CMakeLists reaches THROUGH Qt6_DIR for Qt's bundled zlib headers
REM (include\QtZlib, which XlsxIO needs to write .xlsx in-process), so a prefix
REM that is really a symlink farm or a partial copy fails there and not here.
if defined QT_DIR if exist "%QT_DIR%\lib\cmake\Qt6\Qt6Config.cmake" goto :qt_done
set "QT_DIR="
call :try_qt_root "%MOIL_ROOT%\third_party\Qt"
if defined QT_DIR goto :qt_done
call :try_qt_root "%SystemDrive%\Qt"
if defined QT_DIR goto :qt_done
call :try_qt_root "%_PF64%\Qt"
if defined QT_DIR goto :qt_done
if defined QTDIR if exist "%QTDIR%\lib\cmake\Qt6\Qt6Config.cmake" set "QT_DIR=%QTDIR%"
if defined QT_DIR goto :qt_done
call :fail "Qt 6 (msvc x64)" "Looked in %MOIL_ROOT%\third_party\Qt, %SystemDrive%\Qt and %_PF64%\Qt. Set QT_DIR in server\deps.local.bat to the prefix, e.g. C:\Qt\6.5.3\msvc2019_64"
goto :eof

:qt_done
set "QT_CMAKE_DIR=%QT_DIR%\lib\cmake\Qt6"
set "QT_BIN=%QT_DIR%\bin"
set "QT_PLUGINS=%QT_DIR%\plugins"
goto :eof

:try_qt_root
REM %1 = a Qt installation root such as C:\Qt, holding 6.x\msvc*_64.
REM
REM Compared NUMERICALLY, not with `dir /on`.
REM
REM /on is a LEXICAL sort and Qt version folders are dotted decimals, so "6.10.0"
REM sorts BEFORE "6.5.3" -- with the last assignment winning, the moment a 6.10 is
REM installed beside a 6.5 the OLDER Qt would be chosen, and chosen silently. The
REM previous comment here asserted the opposite ("the LAST assignment wins and that
REM is the newest version"), which is true only while 6.x stays single-digit.
REM
REM Nothing about that failure announces itself: the build succeeds against
REM whichever Qt was picked, and the mismatch only surfaces later as the two-Qt
REM problem: conda-forge's QtCore is built against system zlib and exports no z_*,
REM so XlsxIO fails to link with "unresolved external symbol __imp_z_crc32" naming
REM a function that is right there in the OTHER QtCore.
if not exist "%~1" goto :eof
setlocal enabledelayedexpansion
set "_QBEST="
set /a _QBESTKEY=-1
for /f "delims=" %%V in ('dir /b /ad "%~1\6.*" 2^>nul') do (
  for /f "tokens=1,2,3 delims=." %%a in ("%%V") do (
    set "_MA=%%a"
    set "_MI=%%b"
    set "_PA=%%c"
    if not defined _MI set "_MI=0"
    if not defined _PA set "_PA=0"
    set /a _QKEY=_MA*1000000+_MI*1000+_PA 2>nul
    for /f "delims=" %%K in ('dir /b /ad "%~1\%%V\msvc*_64" 2^>nul') do (
      if exist "%~1\%%V\%%K\lib\cmake\Qt6\Qt6Config.cmake" (
        if !_QKEY! GTR !_QBESTKEY! (
          set /a _QBESTKEY=!_QKEY!
          set "_QBEST=%~1\%%V\%%K"
        )
      )
    )
  )
)
REM endlocal and the set on ONE line, for the reason given in :resolve_vcvars.
endlocal & if not "%_QBEST%"=="" set "QT_DIR=%_QBEST%"
goto :eof


REM ---------------------------------------------------------------------------
:resolve_opencv
REM Wanted: build\x64\vc15\lib, and NOT build\.
REM
REM OpenCVConfig.cmake in build\ maps MSVC_VERSION to a vc14/vc15 subfolder, and
REM 4.6.0 shipped before MSVC 19.44 existed -- it leaves OpenCV_RUNTIME empty and
REM reports "no binaries compatible with your configuration" with the package
REM sitting right there in the folder it just looked at. vc15 is the VS2017+ ABI,
REM which VS2019 and VS2022 both share, so pointing straight at it is correct and
REM not a workaround.
if defined OPENCV_DIR if exist "%OPENCV_DIR%\OpenCVConfig.cmake" goto :opencv_done
set "OPENCV_DIR="
call :try_opencv_root "%MOIL_ROOT%\third_party\opencv"
if defined OPENCV_DIR goto :opencv_done
call :try_opencv_root "%MOIL_ROOT%\third_party\opencv\opencv"
if defined OPENCV_DIR goto :opencv_done
for /f "delims=" %%D in ('dir /b /ad /on "%SystemDrive%\dev\deps\opencv*" 2^>nul') do (
  if not defined OPENCV_DIR call :try_opencv_root "%SystemDrive%\dev\deps\%%D\opencv"
  if not defined OPENCV_DIR call :try_opencv_root "%SystemDrive%\dev\deps\%%D"
)
if defined OPENCV_DIR goto :opencv_done
call :try_opencv_root "%SystemDrive%\opencv"
if defined OPENCV_DIR goto :opencv_done
call :fail "OpenCV (x64, vc14/vc15 binaries)" "Looked in %MOIL_ROOT%\third_party\opencv, %SystemDrive%\dev\deps\opencv* and %SystemDrive%\opencv. Set OPENCV_DIR in server\deps.local.bat to the lib folder, e.g. C:\dev\deps\opencv460\opencv\build\x64\vc15\lib"
goto :eof

:opencv_done
REM bin sits beside lib in every official OpenCV Windows package. Derived rather
REM than configured, so the two can never point at different builds -- which
REM would link against one OpenCV and load another at runtime.
for %%I in ("%OPENCV_DIR%\..\bin") do set "OPENCV_BIN=%%~fI"
goto :eof

:try_opencv_root
REM %1 = an OpenCV root holding build\x64\vc*\lib. Ascending sort again, so vc15
REM wins over vc14 where both are present.
if not exist "%~1\build\x64" goto :eof
for /f "delims=" %%K in ('dir /b /ad /on "%~1\build\x64\vc*" 2^>nul') do (
  if exist "%~1\build\x64\%%K\lib\OpenCVConfig.cmake" set "OPENCV_DIR=%~1\build\x64\%%K\lib"
)
goto :eof


REM ---------------------------------------------------------------------------
:resolve_eigen
REM Eigen is normally NOT found here at all, and that is correct.
REM
REM The ROS distro ships eigen3 inside its own environment and colcon puts that
REM on CMAKE_PREFIX_PATH, so find_package(Eigen3) resolves to
REM <distro>\.pixi\envs\default\Library\share\eigen3\cmake and the Eigen3::Eigen
REM target comes from there. The old build script set EIGEN_DIR and passed
REM -DEIGEN3_INCLUDE_DIR; the CMake cache records that variable as UNINITIALIZED
REM and unread -- it was configuration that looked load-bearing and was not.
REM
REM So this is an OPTIONAL override, for a machine whose distro lacks Eigen.
REM Empty is the expected outcome and is not an error.
if defined EIGEN_DIR goto :eof
if exist "%MOIL_ROOT%\third_party\eigen3\share\eigen3\cmake\Eigen3Config.cmake" set "EIGEN_DIR=%MOIL_ROOT%\third_party\eigen3"
goto :eof


REM ---------------------------------------------------------------------------
:resolve_pixi
REM The ROS 2 distro. On this rig it is a pixi environment, which means the ROS
REM tools are on PATH only inside `pixi run`; outside it, `ros2` does not exist
REM and colcon is not installed either.
REM
REM If ROS is already on PATH (a conda env, a normal ROS install, or a shell that
REM is already inside pixi) then PIXI_MANIFEST stays EMPTY and the launchers skip
REM the re-exec entirely. That is what makes these scripts work on a machine that
REM never heard of pixi.
REM Resolved even when PIXI_IN_SHELL is already set -- i.e. in the re-exec'd child
REM process. The child does not need the re-exec, but it DOES still need
REM ROS_LOCAL_SETUP, which is derived from this: `pixi run` puts the pixi
REM environment on PATH, not the ROS overlay. Skipping the search here left the
REM child with no local_setup.bat to call and colcon reporting that none of the
REM message packages exist.
if defined PIXI_MANIFEST if exist "%PIXI_MANIFEST%" goto :eof
set "PIXI_MANIFEST="
call :try_pixi "%MOIL_ROOT%\third_party\ros"
if defined PIXI_MANIFEST goto :eof
call :try_pixi "%SystemDrive%\dev\lyrical"
if defined PIXI_MANIFEST goto :eof
for /f "delims=" %%D in ('dir /b /ad /on "%SystemDrive%\dev" 2^>nul') do (
  if not defined PIXI_MANIFEST call :try_pixi "%SystemDrive%\dev\%%D"
)
if defined PIXI_MANIFEST goto :eof
REM No pixi manifest anywhere. Fine IF ros2 is reachable as it stands.
where ros2 >nul 2>&1 && goto :eof
where colcon >nul 2>&1 && goto :eof
call :fail "a ROS 2 distro" "No pixi.toml with a local_setup.bat beside it under %MOIL_ROOT%\third_party\ros or %SystemDrive%\dev, and ros2 is not on PATH. Set PIXI_MANIFEST (or ROS_LOCAL_SETUP) in server\deps.local.bat"
goto :eof

:try_pixi
REM %1 = a candidate distro folder. Both files must be there: a pixi.toml with no
REM local_setup.bat beside it is some other project's manifest, and running the
REM build inside it would produce errors about missing packages that read like a
REM broken ROS install.
if not exist "%~1\pixi.toml" goto :eof
if not exist "%~1\local_setup.bat" goto :eof
set "PIXI_MANIFEST=%~1\pixi.toml"
goto :eof


REM ---------------------------------------------------------------------------
:resolve_ros
REM The distro's local_setup.bat, which is what actually puts ros2, colcon and
REM the message packages into the environment. It sits beside pixi.toml.
if defined ROS_LOCAL_SETUP if exist "%ROS_LOCAL_SETUP%" goto :eof
set "ROS_LOCAL_SETUP="
if not defined PIXI_MANIFEST goto :eof
for %%I in ("%PIXI_MANIFEST%") do set "ROS_LOCAL_SETUP=%%~dpIlocal_setup.bat"
if not exist "%ROS_LOCAL_SETUP%" set "ROS_LOCAL_SETUP="
goto :eof


REM ---------------------------------------------------------------------------
:resolve_vcvars
REM MSVC. Needed by the BUILD only: moil_interfaces generates and compiles C++,
REM and the server is C++. run_server.bat does not call this.
if defined VCVARS if exist "%VCVARS%" goto :eof
set "VCVARS="
REM DELAYED EXPANSION, for this routine only, and it is not optional.
REM
REM Every path here goes through "Program Files (x86)". %VAR% is substituted when
REM cmd PARSES a line, so inside a parenthesised for/if block the (x86) becomes a
REM literal ")" in the block's own text and closes it early. What that produced
REM was the vswhere call running as a bare `vswhere.exe` with the directory
REM stripped -- "'vswhere.exe' is not recognized", from a line naming the full
REM path. !VAR! is substituted at execution time, after the block is parsed, so
REM the parentheses in the value never reach the parser.
setlocal enabledelayedexpansion
set "_FOUND="
REM vswhere is the supported way to find an install and knows about editions and
REM years this script has never heard of. The sweep below is only for a machine
REM whose installer was never run -- a copied toolchain.
REM vswhere's answer goes through a FILE rather than through `for /f` backticks.
REM
REM Running it inline means handing cmd a command that both begins with a quote
REM and contains "(x86)". Whether cmd keeps the leading quote depends on how this
REM script was itself invoked, and when it drops it the directory goes with it:
REM the build log then carries "'vswhere.exe' is not recognized" from a line that
REM names the full path, and MSVC is silently resolved by the fallback sweep
REM instead. Reading a file has no such rule -- the exe is launched as an ordinary
REM command and for /f only parses text.
set "_VSTMP=%TEMP%\_moil_vswhere.txt"
set "_VSWHERE=!_PF86!\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!_VSWHERE!" (
  "!_VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "!_VSTMP!" 2>nul
  for /f "usebackq tokens=* delims=" %%I in ("!_VSTMP!") do (
    if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "_FOUND=%%I\VC\Auxiliary\Build\vcvars64.bat"
  )
  del "!_VSTMP!" 2>nul
)
if not defined _FOUND for %%Y in (2019 2022) do (
  for %%E in (BuildTools Community Professional Enterprise) do (
    for %%P in ("!_PF86!" "!_PF64!") do (
      if exist "%%~P\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" set "_FOUND=%%~P\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
    )
  )
)
REM endlocal and the set must be ONE line: the whole line is parsed, and %_FOUND%
REM substituted, before endlocal discards the scope that defines it.
endlocal & set "VCVARS=%_FOUND%"
REM Not found is NOT an error here -- see the --need-msvc note at the top. The
REM caller that actually compiles is the one that decides this is fatal.
goto :eof


REM ---------------------------------------------------------------------------
:resolve_ws
REM Where colcon puts build\, install\ and log\.
REM
REM NOT inside the workspace, and this is the one place where a short absolute
REM path is the right answer rather than a shortcut.
REM
REM Windows caps a path at 260 characters. rosidl nests deeply -- the generated
REM typesupport headers land about 120 characters below the build root on their
REM own -- so the workspace's own location eats the budget before the compiler
REM sees a file. This tree lives under a Desktop folder with a version number and
REM a space in it, which is 100 characters spent before build\ is even created.
REM When it overflows, the failure is "Cannot open source file", naming a header
REM that exists, which reads as a missing dependency and is not.
REM
REM A per-workspace subfolder keyed by the source tree keeps two checkouts from
REM overwriting each other's build; build_server.bat writes the key and checks it.
if not defined MOIL_WS_BASE set "MOIL_WS_BASE=%SystemDrive%\moil_ws"
if not defined MOIL_BUILD_BASE   set "MOIL_BUILD_BASE=%MOIL_WS_BASE%\build"
if not defined MOIL_INSTALL_BASE set "MOIL_INSTALL_BASE=%MOIL_WS_BASE%\install"
if not defined MOIL_LOG_BASE     set "MOIL_LOG_BASE=%MOIL_WS_BASE%\log"
set "MOIL_WS_STAMP=%MOIL_WS_BASE%\.moil_source"
goto :eof


REM ---------------------------------------------------------------------------
:fail
echo   NOT FOUND: %~1
echo              %~2
echo.
set "MOIL_ENV_ERROR=1"
goto :eof

:unresolved
echo Nothing has been built. Fix the above and run this again.
exit /b 1

:report
echo.
echo   workspace      %MOIL_ROOT%
echo   server         %MOIL_SERVER_DIR%
echo   overrides      %MOIL_SERVER_DIR%\deps.local.bat
if not exist "%MOIL_SERVER_DIR%\deps.local.bat" echo                  ^(absent -- everything below was discovered^)
echo.
echo   Qt             %QT_DIR%
echo   OpenCV         %OPENCV_DIR%
echo   OpenCV bin     %OPENCV_BIN%
if defined EIGEN_DIR echo   Eigen          %EIGEN_DIR%
if not defined EIGEN_DIR echo   Eigen          ^(from the ROS distro -- expected^)
if defined PIXI_MANIFEST echo   pixi           %PIXI_MANIFEST%
if not defined PIXI_MANIFEST echo   pixi           ^(not used -- ROS is already on PATH^)
echo   ROS setup      %ROS_LOCAL_SETUP%
if defined VCVARS echo   MSVC           %VCVARS%
if not defined VCVARS echo   MSVC           ^(not found -- only needed to BUILD^)
echo.
echo   build base     %MOIL_BUILD_BASE%
echo   install base   %MOIL_INSTALL_BASE%
echo   log base       %MOIL_LOG_BASE%
echo.
if defined MOIL_ENV_ERROR exit /b 1
echo   All resolved.
exit /b 0
