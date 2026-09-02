#include "axis_device.h"

// The motion stage: the thread that owns the serial ports, the gate that lets
// the process exit while a home is running, and the public API.
//
// The controller protocols themselves are not here -- see axis_backend.h for
// what is in which file and why they are apart.

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <utility>

#include <QMetaObject>
#include <QThread>

#include "axis_backend.h"

namespace {

// The object that owns the ports. It lives on the serial thread; nothing touches
// a QSerialPort from anywhere else.
class AxisWorker : public QObject {
public:
    explicit AxisWorker(const DeviceConfig &cfg) : cfg_(cfg) {}

    // The backend -- and therefore every QSerialPort -- is built HERE and not in
    // the constructor, and that placement is the whole point of this method.
    //
    // A QSerialPort does its Windows I/O through overlapped requests whose
    // completions are delivered by the event dispatcher of the thread the port
    // OBJECT lives on. It does not matter which thread calls write(): what has to
    // line up is the port's thread affinity and the thread whose event loop is
    // running.
    //
    // Building the backend in the constructor put those on different threads.
    // AxisWorker is constructed by AxisDevice's constructor, so it and its ports
    // were born on the caller's thread; the moveToThread() that follows moves the
    // AxisWorker and its CHILDREN, and these ports are neither -- they are members
    // of a plain C++ backend object held by unique_ptr, so nothing about them is
    // reachable through the QObject tree and they stayed behind. Every command
    // then ran on the serial thread against a port whose completions were being
    // waited on somewhere else.
    //
    // It fails differently depending on what the original thread is doing, which
    // is why it did not look like one bug:
    //
    //   --test-axis   no event loop on the main thread at all, so completions are
    //                 never delivered. The first write finishes synchronously, its
    //                 reply is never read, and every write after it dies on
    //                 "The wait operation timed out" -- the port still holding an
    //                 overlapped write that nothing will ever complete.
    //   the GUI       the main thread's event loop IS running, so completions are
    //                 delivered -- on the GUI thread, concurrently with the serial
    //                 thread blocking inside waitForBytesWritten() on the same
    //                 port. That is a data race, and it behaves like one: mostly
    //                 fine, occasionally a truncated status word or a command that
    //                 never lands.
    //
    // Constructing on the serial thread puts affinity and event loop on the same
    // thread and both go away. Nothing else here changes -- every call already
    // arrives through Impl::sync().
    void init() {
        if (backend_) return;
        if (cfg_.axisSystem.compare(QLatin1String("yinda"), Qt::CaseInsensitive) == 0)
            backend_ = makeYindaBackend(cfg_);
        else
            backend_ = makeYuanmanBackend(cfg_);
    }

    // Close AND destroy the ports, for the same reason init() builds them: a
    // QSerialPort has to be destroyed on the thread it lives on. Called on the
    // serial thread while its event loop is still running.
    void shutdown() {
        if (!backend_) return;
        backend_->close();
        backend_.reset();
    }

    AxisBackend *backend() { return backend_.get(); }
    const DeviceConfig &config() const { return cfg_; }

private:
    DeviceConfig cfg_;
    std::unique_ptr<AxisBackend> backend_;
};

}  // namespace

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

struct AxisDevice::Impl {
    DeviceConfig cfg;
    QThread thread;
    AxisWorker *worker = nullptr;
    QString error;  // written on the serial thread, read on the caller's

    // Shutdown gate.
    //
    // A single command can hold the serial thread for tens of seconds -- a home
    // is a physical movement -- and the GUI lets you quit during one. Without
    // this, the destructor stops the serial thread while a QtConcurrent worker is
    // still inside sync(): its BlockingQueuedConnection then waits on an event
    // loop that will never run again, and the app hangs on exit with no window
    // left to explain why. Worse, `worker` is deleted underneath it.
    //
    // So calls are counted, new ones are refused once shutdown starts, and the
    // destructor waits for the ones already running to come back.
    std::mutex gate;
    std::condition_variable drained;
    int inFlight = 0;
    bool stopping = false;

    // Run `f` on the serial thread and wait for it. Called from the GUI thread and
    // from QtConcurrent workers alike; the wait is what serialises the port, and
    // it is the same blocking the ROS client's per-call mutex did.
    template <typename F>
    auto sync(F &&f) -> decltype(f()) {
        using R = decltype(f());
        if (QThread::currentThread() == &thread) return f();

        {
            std::lock_guard<std::mutex> lock(gate);
            // A default-constructed answer is what every caller already treats as
            // "the axis did not respond" -- an empty QString is the ERROR case for
            // the sensors, and false for the booleans.
            if (stopping) return R{};
            ++inFlight;
        }

        R out{};
        QMetaObject::invokeMethod(worker, std::forward<F>(f), Qt::BlockingQueuedConnection,
                                  &out);

        {
            std::lock_guard<std::mutex> lock(gate);
            if (--inFlight == 0) drained.notify_all();
        }
        return out;
    }

    // Refuse further calls and wait for the ones in flight. The timeout is longer
    // than the longest single command so a normal shutdown never trips it; if it
    // does trip, exiting with a wedged serial thread still beats never exiting.
    void closeGate() {
        std::unique_lock<std::mutex> lock(gate);
        stopping = true;
        drained.wait_for(lock, std::chrono::seconds(35), [this] { return inFlight == 0; });
    }
};

AxisDevice::AxisDevice(DeviceConfig cfg) : d_(new Impl) {
    d_->cfg = std::move(cfg);
    d_->worker = new AxisWorker(d_->cfg);
    d_->worker->moveToThread(&d_->thread);
    d_->thread.start();

    // Build the ports AND open them on the serial thread, so they belong to it
    // from before the first byte -- see AxisWorker::init().
    d_->error = d_->sync([this]() -> QString {
        d_->worker->init();
        QString err;
        if (!d_->worker->backend()->open(&err)) return err;
        return QString();
    });

    if (!d_->error.isEmpty())
        std::cerr << "[axis] " << qUtf8Printable(d_->error) << "\n";
    else
        std::cerr << "[axis] open: " << qUtf8Printable(url()) << "\n";
}

AxisDevice::~AxisDevice() {
    // Stop accepting work and let whatever is mid-command finish, BEFORE the
    // thread it is running on is stopped.
    d_->closeGate();

    if (d_->worker) {
        // Not sync(): the gate is shut, so sync() would refuse this too. The
        // thread is still running its event loop at this point, which is the
        // whole reason the close can still be done properly.
        QMetaObject::invokeMethod(
            d_->worker, [this] { d_->worker->shutdown(); },
            Qt::BlockingQueuedConnection);
    }

    d_->thread.quit();
    d_->thread.wait();
    // Only now, with the thread stopped: deleteLater() would need the event loop
    // that quit() has just ended, and the worker would leak with its ports still
    // held -- which on Windows keeps the next run from opening them.
    delete d_->worker;
    d_->worker = nullptr;
}

bool AxisDevice::isOpen() const {
    return d_->sync([this] { return d_->worker->backend()->isOpen(); });
}

QString AxisDevice::lastError() const { return d_->error; }

QString AxisDevice::reconnect() {
    d_->error = d_->sync([this]() -> QString {
        d_->worker->backend()->close();
        QString err;
        if (!d_->worker->backend()->open(&err)) return err;
        return QString();
    });
    return d_->error.isEmpty() ? QStringLiteral("hardware reopened") : d_->error;
}

// ------------------------------------------------------------------ motion

QString AxisDevice::callMove(const QString &direction, double distance, const QString &speed) {
    return d_->sync([this, direction, distance, speed] {
        return d_->worker->backend()->move(direction, distance, speed);
    });
}

QString AxisDevice::home(const QString &axis) {
    return d_->sync([this, axis] { return d_->worker->backend()->home(axis.toLower()); });
}
QString AxisDevice::stop(const QString &axis) {
    return d_->sync([this, axis] { return d_->worker->backend()->stop(axis.toLower()); });
}
QString AxisDevice::x_left(double d, const QString &s) { return callMove("x_left", d, s); }
QString AxisDevice::x_right(double d, const QString &s) { return callMove("x_right", d, s); }
QString AxisDevice::y_up(double d, const QString &s) { return callMove("y_up", d, s); }
QString AxisDevice::y_down(double d, const QString &s) { return callMove("y_down", d, s); }
QString AxisDevice::z_forward(double d, const QString &s) { return callMove("z_forward", d, s); }
QString AxisDevice::z_back(double d, const QString &s) { return callMove("z_back", d, s); }
QString AxisDevice::yaw_left(double d, const QString &s) { return callMove("yaw_left", d, s); }
QString AxisDevice::yaw_right(double d, const QString &s) { return callMove("yaw_right", d, s); }
QString AxisDevice::pitch_up(double d, const QString &s) { return callMove("pitch_up", d, s); }
QString AxisDevice::pitch_down(double d, const QString &s) { return callMove("pitch_down", d, s); }

QString AxisDevice::read_position(const QString &axis) {
    return d_->sync(
        [this, axis] { return d_->worker->backend()->readPosition(axis.toLower()); });
}

QString AxisDevice::write_position(const QString &axis, double position) {
    if (!d_->cfg.enableWritePosition) return QString();
    return d_->sync([this, axis, position] {
        return d_->worker->backend()->writePosition(axis.toLower(), position);
    });
}

double AxisDevice::unitsPerCount(const QString &axis) const {
    // Config, not serial: no round trip and safe to call from any thread.
    return d_->cfg.unitsPerCount(axis);
}

double AxisDevice::travelToLimit(const QString &axis) const {
    return d_->cfg.travelToLimit(axis);
}

// ------------------------------------------------------------------ sensors

QString AxisDevice::callSensor(const QString &name) {
    return d_->sync([this, name] { return d_->worker->backend()->sensor(name); });
}

QString AxisDevice::sensor(const QString &endpoint) {
    QString name = endpoint;
    if (name.startsWith(QLatin1String("is_sensor_")))
        name = name.mid(QStringLiteral("is_sensor_").size());
    return callSensor(name);
}

QString AxisDevice::is_sensor_x_left() { return callSensor("x_left"); }
QString AxisDevice::is_sensor_x_org() { return callSensor("x_org"); }
QString AxisDevice::is_sensor_x_right() { return callSensor("x_right"); }
QString AxisDevice::is_sensor_x_move() { return callSensor("x_move"); }
QString AxisDevice::is_sensor_y_down() { return callSensor("y_down"); }
QString AxisDevice::is_sensor_y_org() { return callSensor("y_org"); }
QString AxisDevice::is_sensor_y_up() { return callSensor("y_up"); }
QString AxisDevice::is_sensor_y_move() { return callSensor("y_move"); }
QString AxisDevice::is_sensor_z_back() { return callSensor("z_back"); }
QString AxisDevice::is_sensor_z_org() { return callSensor("z_org"); }
QString AxisDevice::is_sensor_z_forward() { return callSensor("z_forward"); }
QString AxisDevice::is_sensor_z_move() { return callSensor("z_move"); }
QString AxisDevice::is_sensor_yaw_left() { return callSensor("yaw_left"); }
QString AxisDevice::is_sensor_yaw_org() { return callSensor("yaw_org"); }
QString AxisDevice::is_sensor_yaw_right() { return callSensor("yaw_right"); }
QString AxisDevice::is_sensor_yaw_move() { return callSensor("yaw_move"); }
QString AxisDevice::is_sensor_pitch_down() { return callSensor("pitch_down"); }
QString AxisDevice::is_sensor_pitch_org() { return callSensor("pitch_org"); }
QString AxisDevice::is_sensor_pitch_up() { return callSensor("pitch_up"); }
QString AxisDevice::is_sensor_pitch_move() { return callSensor("pitch_move"); }

// ------------------------------------------------------------------ naming

void AxisDevice::setUrl(const QString &) {
    // There is no address any more. The field is left in the UI because removing
    // it is a change to the form, not to the wiring.
}

QString AxisDevice::url() const {
    return d_->sync([this] { return d_->worker->backend()->describePorts(); });
}
