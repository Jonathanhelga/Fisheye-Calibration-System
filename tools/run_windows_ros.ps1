# Launch moil_fisheye_cali on Windows with the full ROS 2 Lyrical environment.
#
# Why this exists: environment variables are per-PowerShell-window. Sourcing the
# two local_setup.ps1 files and prepending PATH works, but it is lost the moment
# you close the window. This script does all of it in one command, every time.
#
# It does NOT need `pixi shell`. That is only required for `colcon build`, whose
# commands live inside the environment. Running the app only needs the pixi
# environment's DLLs, which the PATH below points at directly.
#
#   .\tools\run_windows_ros.ps1                 # ROS build, RelWithDebInfo
#   .\tools\run_windows_ros.ps1 -NoRos          # plain build-win Release, no ROS
#   .\tools\run_windows_ros.ps1 -Qt "D:\Qt\6.9.0\msvc2022_64"
#
# Pass -Ros2 / -Qt / -OpenCvBin / -Config / -BuildDir if your paths differ from
# the defaults documented in README Part A.

[CmdletBinding()]
param(
    [string] $Ros2      = "C:\dev\ros2-lyrical",
    [string] $Qt        = "C:\Qt\6.8.1\msvc2022_64",
    [string] $OpenCvBin = "C:\opencv\build\x64\vc16\bin",
    [string] $Config,
    [string] $BuildDir,
    [switch] $NoRos,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $AppArgs
)

$ErrorActionPreference = "Stop"

# The repo root is the parent of tools\, so the script works from any directory.
$repo = Split-Path -Parent $PSScriptRoot

if (-not $BuildDir) { $BuildDir = if ($NoRos) { "build-win"   } else { "build-win-ros"   } }
if (-not $Config)   { $Config   = if ($NoRos) { "Release"     } else { "RelWithDebInfo"  } }

$exe = Join-Path $repo "$BuildDir\$Config\moil_fisheye_cali.exe"

if (-not (Test-Path $exe)) {
    Write-Error "Not built: $exe`nBuild it first -- see README Part A, step $(if ($NoRos) { 'A6b' } else { 'A7f' })."
}

# ---- Qt ---------------------------------------------------------------------
# Without the Qt bin directory the app dies with "Qt6Quick.dll was not found";
# without the plugin path it dies with "could not load the Qt platform plugin".
if (-not (Test-Path "$Qt\bin\Qt6Quick.dll")) {
    Write-Error "Qt not found at $Qt -- pass -Qt with the right path. See README step A3."
}
$paths = @("$Qt\bin")
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$Qt\plugins\platforms"

# ---- OpenCV -----------------------------------------------------------------
# Not Qt's problem, so windeployqt never fetches it.
if (Test-Path $OpenCvBin) {
    $paths += $OpenCvBin
} else {
    Write-Warning "OpenCV bin not found at $OpenCvBin -- the 3D Verification window may fail to open."
}

# ---- ROS 2 ------------------------------------------------------------------
if (-not $NoRos) {
    $lyricalSetup = Join-Path $Ros2 "local_setup.ps1"
    $ifaceSetup   = Join-Path $repo "ros\install\local_setup.ps1"

    if (-not (Test-Path $lyricalSetup)) {
        Write-Error "ROS 2 Lyrical not found at $Ros2 -- see README step A7c, or pass -NoRos."
    }
    if (-not (Test-Path $ifaceSetup)) {
        Write-Error "moil_interfaces is not built at $repo\ros\install -- see README step A7d."
    }

    # These set AMENT_PREFIX_PATH and the ROS half of PATH. Both are needed:
    # skip either and the process exits instantly with no message at all.
    . $lyricalSetup
    . $ifaceSetup

    # rclcpp.dll's own dependencies -- spdlog, tinyxml2, yaml-cpp, openssl, icu --
    # live in the pixi environment, not in the ROS archive. This is the pixi
    # environment; activating a pixi shell would only add the same directory.
    $pixiBin = Join-Path $Ros2 ".pixi\envs\default\Library\bin"
    if (-not (Test-Path $pixiBin)) {
        Write-Error "pixi environment missing at $pixiBin -- run 'pixi install' in $Ros2. See README step A7c."
    }
    $paths += $pixiBin
}

$env:PATH = ($paths -join ";") + ";" + $env:PATH

# Start from the repo root so image_cali/output_3D and the other relative paths
# the 3D sub-app writes to resolve where you expect them.
Push-Location $repo
try {
    Write-Host "Running $exe" -ForegroundColor Cyan
    if (-not $NoRos) { Write-Host "ROS 2: $Ros2 (domain is set in the app's Server panel, default 42)" -ForegroundColor DarkGray }

    & $exe @AppArgs
    $code = $LASTEXITCODE

    # The exe is a CONSOLE subsystem binary, so PowerShell waits for it and this
    # line only prints once it has really exited. A non-zero code here is the
    # single most useful fact when the app disappears without a message.
    $meaning = switch ($code) {
        0            { "clean exit" }
        -1           { "QML failed to load -- objectCreationFailed in main.cpp" }
        3            { "abort() -- usually an exception escaping a worker thread (std::terminate)" }
        -1073741819  { "0xC0000005 access violation" }
        -1073740791  { "0xC0000409 stack buffer overrun / fail-fast" }
        -1073741510  { "0xC000013A Ctrl-C" }
        default      { "" }
    }
    $colour = if ($code -eq 0) { "DarkGray" } else { "Yellow" }
    Write-Host ("Exit code $code" + $(if ($meaning) { "  --  $meaning" })) -ForegroundColor $colour
} finally {
    Pop-Location
}
