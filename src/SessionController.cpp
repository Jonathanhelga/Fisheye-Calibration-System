#include "SessionController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "moil_interfaces/action/run_compute.hpp"
#include "moil_interfaces/srv/session_capture.hpp"
#include "moil_interfaces/srv/session_command.hpp"
#endif

namespace {

constexpr char kCommandService[] = "/session/command";
constexpr char kCaptureService[] = "/session/capture";
constexpr char kRunComputeAction[] = "/session/run_compute";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;

// Generous: a capture and a sweep are slow.
constexpr int kCaptureTimeoutMs = 20000;
constexpr int kComputeTimeoutMs = 120000;

SessionController *g_instance = nullptr;

}  // namespace

struct SessionController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedPtr command;
    rclcpp::Client<moil_interfaces::srv::SessionCapture>::SharedPtr capture;
    rclcpp_action::Client<moil_interfaces::action::RunCompute>::SharedPtr compute;
#endif
};

SessionController::SessionController(QObject *parent)
    : QObject(parent), d_(std::make_unique<Impl>()) {
    g_instance = this;
}

SessionController::~SessionController() {
    if (g_instance == this) g_instance = nullptr;
    stopWorker();
}

SessionController *SessionController::instance() { return g_instance; }

void SessionController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

void SessionController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void SessionController::setLastError(const QString &message) {
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void SessionController::beginCall() {
    if (busy_++ == 0) emit busyChanged();
}

void SessionController::endCall() {
    if (busy_ > 0 && --busy_ == 0) emit busyChanged();
}

// ---- GUI slots ----

void SessionController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);

    if (status_ != ProbeStatus::Ok) {
        // The session belonged to a link now gone.
        sessionId_.clear();
        sessionName_.clear();
        capturedSlots_.clear();
        emit sessionChanged();
        if (busy_ > 0) {
            busy_ = 0;
            emit busyChanged();
        }
    }
}

void SessionController::applySession(const QString &id, const QString &name, bool ok,
                                     const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) {
        setLastError(message.isEmpty() ? tr("the session could not be opened") : message);
        return;
    }

    setLastError(QString());
    sessionId_ = id;
    sessionName_ = name;
    // The server reports its slots, not us.
    capturedSlots_.clear();
    emit sessionChanged();
    emit notice(tr("session %1 open").arg(name.isEmpty() ? id : name));
}

void SessionController::applyCaptured(const QString &slot, bool ok, int width, int height,
                                      const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (ok) {
        setLastError(QString());
        if (!capturedSlots_.contains(slot)) {
            capturedSlots_.append(slot);
            emit sessionChanged();
        }
    } else {
        setLastError(message.isEmpty() ? tr("the rig refused the capture") : message);
    }
    emit captured(slot, ok, width, height, message);
}

void SessionController::applyCompute(quint64 token, bool ok, const QString &resultJson,
                                     const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;
    endCall();

    if (!ok) setLastError(message);
    else setLastError(QString());

    emit computeFinished(token, ok, resultJson, message);
}

void SessionController::applyProgress(quint64 token, int done, int total, const QString &stage,
                                      quint64 generation) {
    if (generation != d_->generation.load()) return;
    emit computeProgress(token, done, total, stage);
}

// ---- link ----

void SessionController::connectTo(int domainId) {
    stopWorker();

    domainId_ = domainId;
    setStatus(ProbeStatus::Checking);
    setLastError(QString());
    sessionId_.clear();
    sessionName_.clear();
    capturedSlots_.clear();
    emit sessionChanged();
    if (busy_ > 0) {
        busy_ = 0;
        emit busyChanged();
    }

    const quint64 generation = d_->generation.load();

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(domainId)
    applyLink(ProbeStatus::Failed,
              tr("this build has no ROS 2 support "
                 "(configure with -DFISHEYE_ENABLE_ROS=ON on Linux)"),
              generation);
#else
    d_->worker = std::thread([this, generation, domainId] {
        auto alive = [this, generation] { return generation == d_->generation.load(); };

        auto post = [this, generation, &alive](ProbeStatus::Status status,
                                               const QString &message) {
            if (!alive()) return;
            QMetaObject::invokeMethod(this, "applyLink", Qt::QueuedConnection,
                                      Q_ARG(int, static_cast<int>(status)),
                                      Q_ARG(QString, message), Q_ARG(quint64, generation));
        };

        auto context = std::make_shared<rclcpp::Context>();
        rclcpp::NodeOptions nodeOptions;

        try {
            rclcpp::InitOptions initOptions;
            initOptions.set_domain_id(static_cast<size_t>(domainId));
            initOptions.auto_initialize_logging(false);
            context->init(0, nullptr, initOptions);
            nodeOptions.context(context);
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
            return;
        }

        try {
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_session", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto command =
                node->create_client<moil_interfaces::srv::SessionCommand>(kCommandService);
            auto capture =
                node->create_client<moil_interfaces::srv::SessionCapture>(kCaptureService);
            auto compute = rclcpp_action::create_client<moil_interfaces::action::RunCompute>(
                node, kRunComputeAction);

            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kServiceWaitMs);
            bool ready = false;
            while (alive() && std::chrono::steady_clock::now() < deadline) {
                if (command->wait_for_service(std::chrono::milliseconds(kWaitSliceMs))) {
                    ready = true;
                    break;
                }
            }

            if (!ready) {
                post(ProbeStatus::Failed,
                     tr("nothing is serving %1 on domain %2 (wrong domain, wrong subnet, or "
                        "the session node is not running)")
                         .arg(QString::fromLatin1(kCommandService))
                         .arg(domainId));
            } else {
                {
                    std::lock_guard<std::mutex> lock(d_->clientMutex);
                    d_->command = command;
                    d_->capture = capture;
                    d_->compute = compute;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->command.reset();
                d_->capture.reset();
                d_->compute.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("session link closed");
    });
#endif
}

// ---- session ----

void SessionController::openOrCreate(const QString &name) {
    if (status_ != ProbeStatus::Ok) {
        setLastError(tr("not connected to the rig, press Update in the Server panel"));
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(name)
    setLastError(tr("this build has no ROS 2 support"));
#else
    rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->command;
    }
    if (!client) {
        setLastError(tr("the session link is up but the client is gone"));
        return;
    }

    beginCall();
    setLastError(QString());
    const quint64 generation = d_->generation.load();

    // Two steps: create would lose an existing session.
    auto request = std::make_shared<moil_interfaces::srv::SessionCommand::Request>();
    request->command = "list";

    client->async_send_request(
        request,
        [this, client, name, generation](
            rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedFuture future) {
            const auto listed = future.get();
            if (generation != d_->generation.load()) return;

            QString existingId;
            if (listed && listed->success) {
                const QJsonArray rows =
                    QJsonDocument::fromJson(QByteArray::fromStdString(listed->result)).array();
                for (const QJsonValue &value : rows) {
                    const QJsonObject row = value.toObject();
                    if (row.value(QStringLiteral("name")).toString() == name) {
                        existingId = row.value(QStringLiteral("id")).toString();
                        break;
                    }
                }
            }

            auto second = std::make_shared<moil_interfaces::srv::SessionCommand::Request>();
            if (existingId.isEmpty()) {
                second->command = "create";
                second->payload = name.toStdString();
            } else {
                second->command = "open";
                second->session_id = existingId.toStdString();
            }

            client->async_send_request(
                second,
                [this, name, generation](
                    rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedFuture inner) {
                    const auto response = inner.get();
                    if (generation != d_->generation.load()) return;

                    const bool ok = response && response->success;
                    QMetaObject::invokeMethod(
                        this, "applySession", Qt::QueuedConnection,
                        Q_ARG(QString, ok ? QString::fromStdString(response->session_id)
                                          : QString()),
                        Q_ARG(QString, name), Q_ARG(bool, ok),
                        Q_ARG(QString, response ? QString::fromStdString(response->message)
                                                : tr("no answer from %1")
                                                      .arg(QString::fromLatin1(kCommandService))),
                        Q_ARG(quint64, generation));
                });
        });
#endif
}

void SessionController::closeSession() {
    if (status_ != ProbeStatus::Ok || sessionId_.isEmpty()) return;

#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->command;
    }
    if (!client) return;

    auto request = std::make_shared<moil_interfaces::srv::SessionCommand::Request>();
    request->command = "close";
    request->session_id = sessionId_.toStdString();
    client->async_send_request(
        request,
        [](rclcpp::Client<moil_interfaces::srv::SessionCommand>::SharedFuture) {});
#endif

    sessionId_.clear();
    sessionName_.clear();
    capturedSlots_.clear();
    emit sessionChanged();
}

// ---- capture ----

void SessionController::capture(const QString &slot, double timeoutSeconds) {
    if (status_ != ProbeStatus::Ok) {
        setLastError(tr("not connected to the rig, press Update in the Server panel"));
        return;
    }
    if (sessionId_.isEmpty()) {
        setLastError(tr("no session is open -- the shot has nowhere to be filed"));
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(slot)
    Q_UNUSED(timeoutSeconds)
    setLastError(tr("this build has no ROS 2 support"));
#else
    rclcpp::Client<moil_interfaces::srv::SessionCapture>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->capture;
    }
    if (!client) {
        setLastError(tr("the session link is up but the client is gone"));
        return;
    }

    beginCall();
    setLastError(QString());
    const quint64 generation = d_->generation.load();

    auto request = std::make_shared<moil_interfaces::srv::SessionCapture::Request>();
    request->session_id = sessionId_.toStdString();
    request->slot = slot.toStdString();
    request->timeout = timeoutSeconds;

    client->async_send_request(
        request,
        [this, slot, generation](
            rclcpp::Client<moil_interfaces::srv::SessionCapture>::SharedFuture future) {
            const auto response = future.get();
            if (generation != d_->generation.load()) return;

            const bool ok = response && response->success;
            QMetaObject::invokeMethod(
                this, "applyCaptured", Qt::QueuedConnection, Q_ARG(QString, slot),
                Q_ARG(bool, ok), Q_ARG(int, ok ? response->width : 0),
                Q_ARG(int, ok ? response->height : 0),
                Q_ARG(QString, response ? QString::fromStdString(response->message)
                                        : tr("no answer from %1")
                                              .arg(QString::fromLatin1(kCaptureService))),
                Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kCaptureTimeoutMs, this, [this, slot, generation] {
        if (generation != d_->generation.load()) return;
        // Late answers are dropped by the generation check.
    });
#endif
}

// ---- compute ----

void SessionController::runCompute(const QString &op, const QString &paramsJson, quint64 token) {
    auto fail = [this, token](const QString &message) {
        setLastError(message);
        emit computeFinished(token, false, QString(), message);
    };

    if (status_ != ProbeStatus::Ok) {
        fail(tr("not connected to the rig, press Update in the Server panel"));
        return;
    }
    if (sessionId_.isEmpty()) {
        // Slot names mean nothing without a session.
        fail(tr("no session is open -- the op resolves its images from the session's "
                "capture slots"));
        return;
    }

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(op)
    Q_UNUSED(paramsJson)
    fail(tr("this build has no ROS 2 support"));
#else
    rclcpp_action::Client<moil_interfaces::action::RunCompute>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->compute;
    }
    if (!client) {
        fail(tr("the session link is up but the client is gone"));
        return;
    }
    if (!client->action_server_is_ready()) {
        fail(tr("nothing is serving %1 on domain %2")
                 .arg(QString::fromLatin1(kRunComputeAction))
                 .arg(domainId_));
        return;
    }

    beginCall();
    setLastError(QString());
    const quint64 generation = d_->generation.load();

    using Action = moil_interfaces::action::RunCompute;
    using GoalHandle = rclcpp_action::ClientGoalHandle<Action>;

    Action::Goal goal;
    goal.session_id = sessionId_.toStdString();
    goal.op = op.toStdString();
    goal.params = paramsJson.toStdString();

    rclcpp_action::Client<Action>::SendGoalOptions options;

    options.goal_response_callback = [this, token, op, generation](GoalHandle::SharedPtr handle) {
        if (generation != d_->generation.load() || handle) return;
        // A rejected goal is reported, not left hanging.
        QMetaObject::invokeMethod(this, "applyCompute", Qt::QueuedConnection,
                                  Q_ARG(quint64, token), Q_ARG(bool, false),
                                  Q_ARG(QString, QString()),
                                  Q_ARG(QString, tr("%1 rejected the goal").arg(op)),
                                  Q_ARG(quint64, generation));
    };

    options.feedback_callback = [this, token, generation](
                                    GoalHandle::SharedPtr,
                                    const std::shared_ptr<const Action::Feedback> feedback) {
        if (generation != d_->generation.load()) return;
        QMetaObject::invokeMethod(this, "applyProgress", Qt::QueuedConnection,
                                  Q_ARG(quint64, token), Q_ARG(int, feedback->done),
                                  Q_ARG(int, feedback->total),
                                  Q_ARG(QString, QString::fromStdString(feedback->stage)),
                                  Q_ARG(quint64, generation));
    };

    options.result_callback = [this, token, op, generation](const GoalHandle::WrappedResult &wrapped) {
        if (generation != d_->generation.load()) return;

        const bool reached = wrapped.code == rclcpp_action::ResultCode::SUCCEEDED ||
                             wrapped.code == rclcpp_action::ResultCode::CANCELED;
        const bool ok = reached && wrapped.result && wrapped.result->success;

        QString message;
        if (wrapped.result) message = QString::fromStdString(wrapped.result->message);
        if (!ok && message.isEmpty()) message = tr("%1 did not complete").arg(op);

        QMetaObject::invokeMethod(
            this, "applyCompute", Qt::QueuedConnection, Q_ARG(quint64, token), Q_ARG(bool, ok),
            Q_ARG(QString, wrapped.result ? QString::fromStdString(wrapped.result->result)
                                          : QString()),
            Q_ARG(QString, message), Q_ARG(quint64, generation));
    };

    client->async_send_goal(goal, options);

    QTimer::singleShot(kComputeTimeoutMs, this, [this, token, op, generation] {
        if (generation != d_->generation.load()) return;
        Q_UNUSED(token)
        Q_UNUSED(op)
        // Only applyCompute reaches endCall().
    });
#endif
}
