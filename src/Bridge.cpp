#include "Bridge.h"

#include "controller_auto_3d_measurement.h"

Bridge::~Bridge() { delete measure3d_; }

void Bridge::openMeasure3d() {
    if (!measure3d_) {
        // The (axis, camera) arguments exist only for the old MainWindow's API.
        // This dialog stores them and never dereferences them -- its live-capture
        // methods are no-op bodies -- so passing nullptr is safe and keeps the
        // whole ROS stack out of this build.
        measure3d_ = new Controller3dMeasurement(nullptr, nullptr, nullptr);
    }

    // Keeping the pointer means a second click re-raises the same window with
    // its state intact, which is what the old MainWindow did. Closing it hides
    // rather than destroys, so the dialog's own closeEvent autosave still runs.
    measure3d_->show();
    measure3d_->raise();
    measure3d_->activateWindow();
}
