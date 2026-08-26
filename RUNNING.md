# Building and Running

This is a Qt 6 desktop app, built QML-first.
`main.cpp` starts a `QQmlApplicationEngine` and loads `Main` from the `FisheyeCaliJojo` module.
There is no `QMainWindow` and no `QQuickWidget` host.

The project is developed on two machines, and both are covered here.

| | MacBook | miniPC |
|---|---|---|
| OS | macOS (Darwin, arm64) | Ubuntu 24.04 LTS (x86_64) |
| Qt | 6.11.1, via Homebrew | 6.11.1, hand-installed under `~/Qt` |
| Qt prefix | `/opt/homebrew/opt/qt6` | `~/Qt/6.11.1/gcc_64` |
| CMake | Homebrew, `/opt/homebrew/bin/cmake` | apt, `/usr/bin/cmake` |
| Checkout | `~/Desktop/Fisheye_Calibration-Jojo_Version` | `~/Fisheye-Calibration-System` |
| Reached by | local | `ssh moilpc` |

Both machines push to `github.com/Jonathanhelga/Fisheye-Calibration-System`.

## The three stages

Every build is the same three stages, always in this order.
You only need to redo the ones that apply to what you changed.

1. **Configure.**
   CMake reads `CMakeLists.txt`, finds Qt 6, and generates the real build instructions into `build/`.
   This is the step that needs to know where Qt lives.
2. **Build.**
   Ninja compiles the C++ and compiles the QML into the binary.
3. **Run.**
   You execute the binary directly.
   There is no dev server, this is a real native app.

## macOS

### Configure

```bash
/opt/homebrew/bin/cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt6"
```

`-DCMAKE_PREFIX_PATH` is required because Homebrew keeps Qt outside CMake's default search path.
Without it you get `Could NOT find Qt6`.

`brew --prefix qt6` prints the same location if you would rather not hardcode it.
Note that it resolves to `/opt/homebrew/opt/qt`, which is a symlink to the same Cellar directory, so either spelling works.

### Build and run

```bash
/opt/homebrew/bin/cmake --build build -j6
./build/fisheye_cali_jojo
```

### First-time setup

```bash
brew install cmake ninja qt6
xcode-select --install
```

## miniPC (Ubuntu)

### Why the hand-installed Qt matters

`CMakeLists.txt` sets `qt_policy(SET QTP0004 NEW)`, which needs **Qt 6.8 or newer**.
Ubuntu 24.04's apt packages ship **Qt 6.4.2**, which is too old.

That is the whole reason Qt 6.11.1 was installed by hand into `~/Qt`.
`qmake6` on `PATH` still points at the apt 6.4.2, so it is not a reliable way to check which Qt a build actually used.
Always pass the prefix explicitly.

### Configure

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/gcc_64"
```

Leave `CMAKE_PREFIX_PATH` off and CMake silently finds the apt Qt 6.4.2 instead, then fails on the `QTP0004` policy.
The error names the policy, not the Qt version, so it reads as a CMake problem when it is really a wrong-Qt problem.

### Build and run

```bash
cmake --build build -j6
./build/fisheye_cali_jojo
```

Running the binary needs a real display session, so do it at the machine or over an X/Wayland forwarding connection, not a plain `ssh moilpc`.

### First-time setup

The apt side:

```bash
sudo apt install cmake ninja-build build-essential
```

Qt 6.11.1 itself comes from the Qt online installer, not apt, and lands in `~/Qt/6.11.1/gcc_64`.

## You must rebuild after editing QML

`CMakeLists.txt` registers the `.qml` files through `qt_add_qml_module`, which compiles them into the binary as a resource.
The app does not read `.qml` files from disk at runtime.

So editing a `.qml` file and relaunching shows you the **old** UI until you rebuild.

Any **new** `.qml` file must also be listed under `QML_FILES` in `CMakeLists.txt`.
A file missing from that list does not fail the build.
It fails at runtime, when the engine cannot find the type.

## What to re-run after what

| You changed | Stages to run |
|---|---|
| `CMakeLists.txt`, or added a new `.qml` file | configure, build, run |
| `main.cpp`, `Bridge.*`, `ServerProbe.*` | build, run |
| any existing `.qml` file | build, run |

The one-liner that always works, once `build/` is configured:

```bash
cmake --build build -j6 && ./build/fisheye_cali_jojo
```

## Working across both machines

`build/` is in `.gitignore`, so each machine keeps its own.
That is deliberate: the two are different architectures and different Qt installs, and sharing one would not work.

The usual loop is commit and push on one machine, then pull and rebuild on the other:

```bash
ssh moilpc
cd ~/Fisheye-Calibration-System
git pull
cmake --build build -j6
```

If a pull brings in `CMakeLists.txt` changes, reconfigure on that machine before building.

## What each piece of the build does

### `CMakeLists.txt`

The recipe for the whole project, the rough equivalent of `package.json` plus a bundler config.

```cmake
set(CMAKE_AUTOMOC ON)
```

Qt classes using `Q_OBJECT`, `Q_PROPERTY`, or `Q_INVOKABLE` need a code-generation pass called `moc` before they compile.
This runs it automatically, so you never call it by hand.
Skip it and you get link errors for symbols you never wrote.

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

Writes `build/compile_commands.json`, which is what the editor's language server reads to type-check the C++.
Before the first successful configure, expect "file not found" squiggles on every Qt include.

```cmake
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network)
```

`QuickControls2` is needed for the `QQuickStyle::setStyle("Basic")` call in `main.cpp`.
The default macOS style is native, meaning Qt does not draw the controls itself, so a custom `background` on a Button is silently discarded.
`Basic` assembles every control out of plain `Rectangle` and `Text`, which is what makes it restylable.

`Network` backs the reachability probes in `ServerProbe`.

```cmake
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)
```

`qt_standard_project_setup` wires up a batch of Qt defaults in one call.
`QTP0004` opts into the newer behaviour for QML files kept in subdirectories, which is what lets `qml/windows/`, `qml/panels/`, and `qml/controls/` work as a layout.
This is the line that requires Qt 6.8 or newer.

```cmake
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
```

Marks `Theme.qml` as a QML singleton, so every file can read `Theme.accent` without instantiating anything.

```cmake
qt_add_qml_module(fisheye_cali_jojo
    URI FisheyeCaliJojo
    VERSION 1.0
    QML_FILES ...
    RESOURCES assets/sample_shot.png
    SOURCES Bridge.cpp Bridge.h ServerProbe.cpp ServerProbe.h
)
```

`URI FisheyeCaliJojo` names the module, which is why `main.cpp` loads the app with `engine.loadFromModule("FisheyeCaliJojo", "Main")` rather than a file path.

`RESOURCES` compiles the sample image into the binary too.
That is why `Main.qml` can reference it as `qrc:/qt/qml/FisheyeCaliJojo/assets/sample_shot.png` and have it work on both machines, instead of depending on an absolute path that only exists on one of them.

### `main.cpp`

The entry point.
It does four things and nothing else:

- `QQuickStyle::setStyle("Basic")` picks a non-native, fully restylable control style.
- `setColorScheme(Qt::ColorScheme::Light)` pins the app to light mode.
  Without it, Qt follows the OS scheme and `base` turns black in dark mode, punching black input fields into our light panels.
  This is a stopgap that holds only while colours live in `qml/Theme.qml` as fixed light values.
- The `objectCreationFailed` connection exits with `-1` rather than leaving the app running with no window.
- `loadFromModule("FisheyeCaliJojo", "Main")` builds the UI.

Note that there is no `setContextProperty` call.
C++ types reach QML through `QML_ELEMENT` registration instead, which is what lets `qmllint` see them statically.

## When configure fails

| Error | What it means | Fix |
|---|---|---|
| `command not found: cmake` | CMake is not on `PATH` | `brew install cmake` / `sudo apt install cmake` |
| `command not found: ninja` | Ninja is not installed | `brew install ninja` / `sudo apt install ninja-build` |
| `Could NOT find Qt6` | CMake cannot see Qt | pass `-DCMAKE_PREFIX_PATH` as shown above |
| `QTP0004` policy error | it found Qt 6.4.2 from apt | point `CMAKE_PREFIX_PATH` at `~/Qt/6.11.1/gcc_64` |
| `does not match the generator used previously` | `build/` was configured with a different generator | `rm -rf build`, then reconfigure |
| `No CMAKE_CXX_COMPILER could be found` | no compiler installed | `xcode-select --install` / `sudo apt install build-essential` |

If you get something not on this list, copy the whole error rather than the last line.
The useful part of a CMake error is usually in the middle.

## See also

- `README.md` for what lives where in the source tree.
- `tools/DUMMY_SERVER.md` for testing the connection dots with no hardware attached.
