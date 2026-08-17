cmake_minimum_required(VERSION 3.16)
project(FisheyeCalibrationJojo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# QuickControls2 is needed for QQuickStyle (selecting the "Basic" style in main.cpp).
# Network is needed for the reachability probes in ServerProbe.
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Network)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(fisheye_cali_jojo
    main.cpp
)

# Registers qml/Main.qml (and everything else added here later) as the
# "FisheyeCaliJojo" QML module, compiled into the binary instead of read
# from disk at runtime. Add new .qml files to QML_FILES as panels are built.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(fisheye_cali_jojo
    URI FisheyeCaliJojo
    VERSION 1.0
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/PanelPlaceholder.qml
        qml/HistogramPanel.qml
        qml/ServerUrlPanel.qml
        qml/LabeledField.qml
        qml/SectionFrame.qml
        qml/SegmentedControl.qml
        qml/StatusDot.qml
        qml/AxisControlPanel.qml
        qml/AxisReadout.qml
        qml/DpadDirectionControl.qml
        qml/ActionButton.qml
        qml/ImagePreview.qml
        qml/CameraPanel.qml
        qml/ValueField.qml
        qml/ColorSwatch.qml
        qml/CenteringPanel.qml
    SOURCES
        Bridge.cpp Bridge.h
        ServerProbe.cpp ServerProbe.h
)

target_link_libraries(fisheye_cali_jojo PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)
