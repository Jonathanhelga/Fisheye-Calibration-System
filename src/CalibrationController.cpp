#include "CalibrationController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPair>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <thread>

#ifdef FISHEYE_ROS_ENABLED
#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "moil_interfaces/action/cali_job.hpp"
#include "moil_interfaces/srv/cali_op.hpp"
#include "moil_interfaces/srv/cali_series.hpp"
#include "moil_interfaces/srv/xlsx_io.hpp"
#endif

namespace {

constexpr char kCaliService[] = "/compute/cali";
constexpr char kSeriesService[] = "/compute/series";
constexpr char kXlsxService[] = "/compute/xlsx";
constexpr char kCaliJobAction[] = "/compute/cali_job";
constexpr char kCaliType[] = "moil_interfaces/srv/CaliOp";

constexpr int kServiceWaitMs = 5000;
constexpr int kWaitSliceMs = 200;
constexpr int kSpinSliceMs = 100;
constexpr int kOpTimeoutMs = 120000;

// Round table: two header rows, 75 layers.
constexpr int kRows = 77;
constexpr int kCols = 35;
constexpr int kFirstDataRow = 2;
constexpr int kLayers = kRows - kFirstDataRow;

// Wire-format column indices, mirrored from CaliCompute.cpp.
constexpr int kColRound = 0;
constexpr int kColPct = 2;
constexpr int kColIctAvg = 12;
constexpr int kColPctCal = 13;
constexpr int kColDistance = 14;
constexpr int kColAlphaAvg = 33;
constexpr int kColZflAvg = 34;

// The rig finds the side layer by this.
const QString kSideMark = QStringLiteral("*");

// The eight directions in table column order.
const QStringList &dirs8() {
    static const QStringList d{QStringLiteral("n"),  QStringLiteral("s"),  QStringLiteral("w"),
                               QStringLiteral("e"),  QStringLiteral("nw"), QStringLiteral("se"),
                               QStringLiteral("sw"), QStringLiteral("ne")};
    return d;
}

int ictCol(int direction) { return 3 + direction; }         // ict_n .. ict_ne, 3..10
int alphaCol(int direction) { return 16 + direction * 2; }  // alpha_n, alpha_s, ...
int zflCol(int direction) { return 17 + direction * 2; }    // zfl_n, zfl_s, ...

QString dump(const QJsonObject &o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QVariantList pointsToVariant(const QJsonArray &pts) {
    QVariantList out;
    out.reserve(pts.size());
    for (const QJsonValue &v : pts) {
        const QJsonArray pair = v.toArray();
        if (pair.size() < 2) continue;
        QVariantMap point;
        point[QStringLiteral("x")] = pair.at(0).toDouble();
        point[QStringLiteral("y")] = pair.at(1).toDouble();
        out.append(point);
    }
    return out;
}

// "round_3.xlsx" -> 3, or -1.
int roundFromName(const QString &fileName) {
    static const QRegularExpression digits(QStringLiteral("(\\d+)"));
    const QRegularExpressionMatch m = digits.match(QFileInfo(fileName).completeBaseName());
    if (!m.hasMatch()) return -1;
    const int n = m.captured(1).toInt();
    return (n >= 1 && n <= 10) ? n : -1;
}

#ifdef FISHEYE_ROS_ENABLED
QString describeMissingService(rclcpp::Node &node, int domainId) {
    const QString name = QString::fromLatin1(kCaliService);
    const auto services = node.get_service_names_and_types();
    const auto found = services.find(kCaliService);

    if (found != services.end()) {
        for (const std::string &type : found->second) {
            if (type != kCaliType) {
                return QObject::tr("%1 is served as %2, this client speaks %3")
                    .arg(name, QString::fromStdString(type), QString::fromLatin1(kCaliType));
            }
        }
        return QObject::tr("%1 was found on domain %2 but did not answer within %3 ms")
            .arg(name, QString::number(domainId), QString::number(kServiceWaitMs));
    }

    return QObject::tr("nothing is serving %1 on domain %2 "
                       "(wrong domain, wrong subnet, or the compute node is not running)")
        .arg(name, QString::number(domainId));
}
#endif

}  // namespace

struct CalibrationController::Impl {
    std::atomic<quint64> generation{0};
    std::thread worker;
    std::mutex clientMutex;
#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::CaliOp>::SharedPtr cali;
    rclcpp::Client<moil_interfaces::srv::CaliSeries>::SharedPtr series;
    rclcpp::Client<moil_interfaces::srv::XlsxIo>::SharedPtr xlsx;
    rclcpp_action::Client<moil_interfaces::action::CaliJob>::SharedPtr job;

    // Written on executor thread, read on GUI.
    rclcpp_action::ClientGoalHandle<moil_interfaces::action::CaliJob>::SharedPtr goal;
#endif
};

CalibrationController::CalibrationController(QObject *parent) : QObject(parent), d_(new Impl) {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { stopWorker(); });

    // An unfilled round is empty, not missing.
    for (int r = 0; r <= 10; ++r) ensureRound(r);

    coefficients_ = QVariantList{QStringLiteral("0"), QStringLiteral("0"), QString(),
                                 QString(),           QString(),           QString()};
    rebuildModel();
}

CalibrationController::~CalibrationController() { stopWorker(); }

void CalibrationController::stopWorker() {
    ++d_->generation;
    if (d_->worker.joinable()) d_->worker.join();
}

// ---- table storage ----
// Cells are strings; "" and "0" differ.

void CalibrationController::ensureRound(int round) {
    QJsonObject rounds = table_.value(QStringLiteral("rounds")).toObject();
    const QString key = QString::number(round);
    if (!rounds.contains(key)) {
        QJsonObject r;
        r[QStringLiteral("rows")] = kRows;
        r[QStringLiteral("cols")] = kCols;
        r[QStringLiteral("cells")] = QJsonObject();
        rounds[key] = r;
        table_[QStringLiteral("rounds")] = rounds;
    }
}

void CalibrationController::setCell(int round, int row, int col, const QString &text) {
    if (round < 0 || round > 10 || row < 0 || row >= kRows || col < 0 || col >= kCols) return;

    QJsonObject rounds = table_.value(QStringLiteral("rounds")).toObject();
    QJsonObject r = rounds.value(QString::number(round)).toObject();
    QJsonObject cells = r.value(QStringLiteral("cells")).toObject();
    QJsonObject rowObject = cells.value(QString::number(row)).toObject();

    if (text.isEmpty())
        rowObject.remove(QString::number(col));
    else
        rowObject[QString::number(col)] = text;

    if (rowObject.isEmpty())
        cells.remove(QString::number(row));
    else
        cells[QString::number(row)] = rowObject;

    r[QStringLiteral("cells")] = cells;
    if (!r.contains(QStringLiteral("rows"))) r[QStringLiteral("rows")] = kRows;
    if (!r.contains(QStringLiteral("cols"))) r[QStringLiteral("cols")] = kCols;
    rounds[QString::number(round)] = r;
    table_[QStringLiteral("rounds")] = rounds;
}

QString CalibrationController::cell(int round, int row, int col) const {
    return table_.value(QStringLiteral("rounds"))
        .toObject()
        .value(QString::number(round))
        .toObject()
        .value(QStringLiteral("cells"))
        .toObject()
        .value(QString::number(row))
        .toObject()
        .value(QString::number(col))
        .toString();
}

QString CalibrationController::tableJson() const { return dump(table_); }

void CalibrationController::bumpVersion(int round) {
    ++tableVersion_;
    if (round >= 0 && round <= 10 && rounds_.size() == 11)
        rebuildRound(round);
    else
        rebuildModel();
    emit tableChanged();
    // Cached series now answer a stale question.
    emit seriesChanged();
}

void CalibrationController::rebuildRound(int round) {
    QVariantList table;
    int sideLayer = kLayers;

    for (int layer = 0; layer < kLayers; ++layer) {
        const int row = kFirstDataRow + layer;

        QVariantMap entry;
        entry[QStringLiteral("pct")] = cell(round, row, kColPct);

        QVariantList ict, alpha, zfl;
        for (int d = 0; d < 8; ++d) {
            ict.append(cell(round, row, ictCol(d)));
            alpha.append(cell(round, row, alphaCol(d)));
            zfl.append(cell(round, row, zflCol(d)));
        }
        entry[QStringLiteral("ict")] = ict;
        entry[QStringLiteral("alpha")] = alpha;
        entry[QStringLiteral("zfl")] = zfl;

        entry[QStringLiteral("ictAvg")] = cell(round, row, kColIctAvg);
        entry[QStringLiteral("pctCal")] = cell(round, row, kColPctCal);
        entry[QStringLiteral("distance")] = cell(round, row, kColDistance);
        entry[QStringLiteral("alphaAvg")] = cell(round, row, kColAlphaAvg);
        entry[QStringLiteral("zflAvg")] = cell(round, row, kColZflAvg);

        // First marked row wins; one start only.
        if (cell(round, row, kColRound) == kSideMark && sideLayer == kLayers) sideLayer = layer;

        table.append(entry);
    }

    if (rounds_.size() == 11) {
        rounds_[round] = QVariant(table);
        sideLayers_[round] = sideLayer;
    } else {
        rounds_.append(QVariant(table));
        sideLayers_.append(sideLayer);
    }
}

void CalibrationController::rebuildModel() {
    rounds_.clear();
    sideLayers_.clear();

    if (roundEnabled_.isEmpty())
        for (int r = 0; r < 10; ++r) roundEnabled_.append(true);

    for (int round = 0; round <= 10; ++round) rebuildRound(round);

    QVariantMap fields;
    const QJsonObject f = table_.value(QStringLiteral("fields")).toObject();
    for (auto it = f.constBegin(); it != f.constEnd(); ++it)
        fields.insert(it.key(), it.value().toString());
    fields_ = fields;
}

// ---- plumbing ----

void CalibrationController::setStatus(ProbeStatus::Status status) {
    if (status_ == status) return;
    status_ = status;
    emit statusChanged();
}

void CalibrationController::setLastError(const QString &message) {
    if (!message.isEmpty()) emit errorRaised(message);
    if (lastError_ == message) return;
    lastError_ = message;
    emit lastErrorChanged();
}

void CalibrationController::beginCall(const QString &what) {
    ++busy_;
    activity_ = what;
    emit busyChanged();
}

void CalibrationController::endCall() {
    if (busy_ > 0) --busy_;
    if (busy_ == 0) activity_.clear();
    emit busyChanged();
}

bool CalibrationController::ready() {
    if (status_ == ProbeStatus::Ok) return true;
    setLastError(tr("not connected to the compute node, press Update in the Server panel"));
    return false;
}

void CalibrationController::stop() {
    // Cancel a running search first.
    if (searchRunning_) {
        cancelSearch();
        return;
    }

    if (liveTokens_.isEmpty()) return;

    const int dropped = static_cast<int>(liveTokens_.size());
    liveTokens_.clear();
    busy_ = 0;
    activity_.clear();
    pendingLoads_ = 0;
    seriesPending_ = 0;
    emit busyChanged();

    // Cali and xlsx are services with no cancel.
    emit notice(tr("Stopped waiting on %1 request(s). They are still running on the rig -- "
                   "their answers will be ignored.")
                    .arg(dropped));
}

void CalibrationController::setFolder(const QString &path) {
    const QString clean = path.startsWith(QLatin1String("file:"))
                              ? QUrl(path).toLocalFile()
                              : path;
    if (folder_ == clean) return;
    folder_ = clean;
    emit folderChanged();
}

void CalibrationController::setSingleDistance(bool on) {
    if (singleDistance_ == on) return;
    singleDistance_ = on;
    emit optionsChanged();
}

void CalibrationController::setBaseDistance(double distance) {
    if (qFuzzyCompare(baseDistance_, distance)) return;
    baseDistance_ = distance;
    emit optionsChanged();
}

void CalibrationController::setRegressionDegree(int degree) {
    const int clamped = qBound(1, degree, 5);
    if (degree_ == clamped) return;
    degree_ = clamped;
    emit optionsChanged();
}

QString CalibrationController::paramsJson(int round, const QJsonObject &extra) const {
    QJsonObject p = extra;
    p[QStringLiteral("round")] = round;
    p[QStringLiteral("use_single_round_distance")] = singleDistance_;

    QJsonArray enabled;
    for (const QVariant &v : roundEnabled_) enabled.append(v.toBool());
    p[QStringLiteral("round_enabled")] = enabled;

    return dump(p);
}

void CalibrationController::applyLink(int status, const QString &message, quint64 generation) {
    if (generation != d_->generation.load()) return;

    setStatus(static_cast<ProbeStatus::Status>(status));
    setLastError(message);

    if (status_ != ProbeStatus::Ok) {
        liveTokens_.clear();
        pendingLoads_ = 0;
        seriesPending_ = 0;
        if (busy_ > 0) {
            busy_ = 0;
            activity_.clear();
            emit busyChanged();
        }
    }
}

void CalibrationController::connectTo(int domainId) {
    stopWorker();

    setStatus(ProbeStatus::Checking);
    setLastError(QString());
    liveTokens_.clear();
    if (busy_ > 0) {
        busy_ = 0;
        activity_.clear();
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
            auto node = std::make_shared<rclcpp::Node>("fisheye_cali_jojo_cali", nodeOptions);

            rclcpp::ExecutorOptions executorOptions;
            executorOptions.context = context;
            rclcpp::executors::SingleThreadedExecutor executor(executorOptions);
            executor.add_node(node);

            auto cali = node->create_client<moil_interfaces::srv::CaliOp>(kCaliService);
            auto series = node->create_client<moil_interfaces::srv::CaliSeries>(kSeriesService);
            auto xlsx = node->create_client<moil_interfaces::srv::XlsxIo>(kXlsxService);
            auto job = rclcpp_action::create_client<moil_interfaces::action::CaliJob>(
                node, kCaliJobAction);

            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kServiceWaitMs);
            bool ready = false;
            while (alive() && std::chrono::steady_clock::now() < deadline) {
                if (cali->wait_for_service(std::chrono::milliseconds(kWaitSliceMs))) {
                    ready = true;
                    break;
                }
            }

            if (!ready) {
                post(ProbeStatus::Failed, describeMissingService(*node, domainId));
            } else {
                {
                    std::lock_guard<std::mutex> lock(d_->clientMutex);
                    d_->cali = cali;
                    d_->series = series;
                    d_->xlsx = xlsx;
                    d_->job = job;
                }
                post(ProbeStatus::Ok, QString());

                while (alive()) executor.spin_once(std::chrono::milliseconds(kSpinSliceMs));

                std::lock_guard<std::mutex> lock(d_->clientMutex);
                d_->cali.reset();
                d_->series.reset();
                d_->xlsx.reset();
                d_->job.reset();
                d_->goal.reset();
            }
        } catch (const std::exception &error) {
            post(ProbeStatus::Failed, QString::fromUtf8(error.what()));
        }

        context->shutdown("cali link closed");
    });
#endif
}

// ---- editing ----

void CalibrationController::setPct(int round, int layer, const QString &value) {
    if (layer < 0 || layer >= kLayers) return;
    setCell(round, kFirstDataRow + layer, kColPct, value.trimmed());
    bumpVersion(round);
}

void CalibrationController::setIct(int round, int layer, int direction, const QString &value) {
    if (layer < 0 || layer >= kLayers || direction < 0 || direction >= 8) return;
    setCell(round, kFirstDataRow + layer, ictCol(direction), value.trimmed());
    bumpVersion(round);
}

void CalibrationController::setSideLayer(int round, int layer) {
    if (layer < 0 || layer >= kLayers) return;

    const int row = kFirstDataRow + layer;
    const bool wasSet = cell(round, row, kColRound) == kSideMark;

    // One side marker per round.
    for (int l = 0; l < kLayers; ++l) setCell(round, kFirstDataRow + l, kColRound, QString());
    if (!wasSet) setCell(round, row, kColRound, kSideMark);

    bumpVersion(round);
}

void CalibrationController::setRoundEnabled(int round, bool enabled) {
    if (round < 1 || round > 10) return;
    if (roundEnabled_.size() < 10)
        while (roundEnabled_.size() < 10) roundEnabled_.append(true);
    roundEnabled_[round - 1] = enabled;
    emit tableChanged();
    emit seriesChanged();
}

void CalibrationController::setField(const QString &name, const QString &value) {
    QJsonObject fields = table_.value(QStringLiteral("fields")).toObject();
    if (value.isEmpty())
        fields.remove(name);
    else
        fields[name] = value;
    table_[QStringLiteral("fields")] = fields;
    bumpVersion();
}

void CalibrationController::clearTable(int round) {
    QJsonObject rounds = table_.value(QStringLiteral("rounds")).toObject();
    QJsonObject r = rounds.value(QString::number(round)).toObject();
    r[QStringLiteral("cells")] = QJsonObject();
    r[QStringLiteral("rows")] = kRows;
    r[QStringLiteral("cols")] = kCols;
    rounds[QString::number(round)] = r;
    table_[QStringLiteral("rounds")] = rounds;

    bumpVersion(round);
    emit notice(tr("Round %1 cleared").arg(round));
}

void CalibrationController::clearAllTables() {
    table_ = QJsonObject();
    for (int r = 0; r <= 10; ++r) ensureRound(r);

    loadStatus_ = ProbeStatus::Unknown;
    loadSummary_.clear();
    aggregationText_.clear();
    noiseText_.clear();
    alphaRounds_.clear();
    alphaFit_.clear();
    zflRounds_.clear();
    globalIctAlpha_.clear();
    roundPoints_.clear();
    searchSamples_.clear();
    searchSummary_.clear();
    maxIct_ = 0;
    hasRawIct_ = false;
    seriesVersion_ = -1;

    bumpVersion();
    emit resultChanged();
    emit notice(tr("All eleven rounds cleared"));
}

// ---- cali ops ----

bool CalibrationController::sendCaliOp(const QString &op, int round, const QJsonObject &extra,
                                       const QString &activity) {
    if (!ready()) return false;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(op)
    Q_UNUSED(round)
    Q_UNUSED(extra)
    Q_UNUSED(activity)
    return false;
#else
    rclcpp::Client<moil_interfaces::srv::CaliOp>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->cali;
    }
    if (!client) return false;

    beginCall(activity);
    const quint64 generation = d_->generation.load();
    const quint64 token = ++nextToken_;
    liveTokens_.insert(token);

    auto request = std::make_shared<moil_interfaces::srv::CaliOp::Request>();
    request->op = op.toStdString();
    request->table_json = tableJson().toStdString();
    request->params_json = paramsJson(round, extra).toStdString();

    client->async_send_request(
        request, [this, generation, token, op,
                  round](rclcpp::Client<moil_interfaces::srv::CaliOp>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(
                this, "applyCaliOp", Qt::QueuedConnection, Q_ARG(QString, op), Q_ARG(int, round),
                Q_ARG(bool, response->success),
                Q_ARG(QString, QString::fromStdString(response->table_json)),
                Q_ARG(QString, QString::fromStdString(response->result_json)),
                Q_ARG(QString, QString::fromStdString(response->message)), Q_ARG(quint64, token),
                Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kOpTimeoutMs, this, [this, generation, token, op] {
        if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
        endCall();
        setLastError(tr("%1 did not answer within %2 s").arg(op).arg(kOpTimeoutMs / 1000));
    });
    return true;
#endif
}

void CalibrationController::applyCaliOp(const QString &op, int round, bool ok,
                                        const QString &tableJson, const QString &resultJson,
                                        const QString &message, quint64 token,
                                        quint64 generation) {
    if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
    endCall();

    // The mutated table comes back even on failure.
    if (!tableJson.isEmpty()) {
        const QJsonObject next = QJsonDocument::fromJson(tableJson.toUtf8()).object();
        if (!next.isEmpty()) {
            table_ = next;
            bumpVersion();
        }
    }

    if (!ok) {
        setLastError(message.isEmpty() ? tr("%1 failed").arg(op) : message);
        return;
    }
    setLastError(QString());

    const QJsonObject r = QJsonDocument::fromJson(resultJson.toUtf8()).object();

    if (op == QLatin1String("aggregation_by_distance") ||
        op == QLatin1String("aggregation_all_rounds_by_distance")) {
        const bool found = r.value(QStringLiteral("found")).toBool(false);
        aggregationText_ = found ? QString::number(r.value(QStringLiteral("value")).toDouble(), 'f', 4)
                                 : tr("no data");
        emit resultChanged();
        emit notice(found ? tr("Round %1 aggregation: %2").arg(round).arg(aggregationText_)
                          : tr("Round %1 has nothing to aggregate yet").arg(round));
        return;
    }

    if (op == QLatin1String("auto_detect_noise_bands")) {
        // One at a time; each reply replaces all.
        pendingBands_.clear();
        pendingBandRound_ = round;
        bandsRemoved_ = 0;

        for (const QJsonValue &v : r.value(QStringLiteral("bands")).toArray()) {
            const QJsonObject b = v.toObject();
            if (!b.value(QStringLiteral("found")).toBool(false)) continue;
            pendingBands_.append(b);
        }

        bandsFound_ = static_cast<int>(pendingBands_.size());
        noiseText_ = bandsFound_ > 0 ? tr("%1 band(s) found").arg(bandsFound_)
                                     : tr("no noise bands found");
        emit resultChanged();

        if (bandsFound_ == 0) {
            emit notice(tr("Round %1: no noise bands to remove").arg(round));
            return;
        }
        sendNextBand();
        return;
    }

    if (op == QLatin1String("remove_nodes_in_band")) {
        bandsRemoved_ += r.value(QStringLiteral("removed")).toInt();

        if (!pendingBands_.isEmpty()) {
            sendNextBand();
            return;
        }

        noiseText_ = tr("%1 node(s) removed from %2 band(s)")
                         .arg(bandsRemoved_)
                         .arg(bandsFound_);
        emit resultChanged();
        emit notice(tr("Round %1: %2 node(s) removed from %3 band(s)")
                        .arg(round)
                        .arg(bandsRemoved_)
                        .arg(bandsFound_));
        return;
    }

    if (op == QLatin1String("compute_all")) {
        emit notice(tr("All rounds recomputed"));
        updateSeries();
        return;
    }

    if (op == QLatin1String("calculate_result") ||
        op == QLatin1String("calculate_result_single_round") ||
        op == QLatin1String("calculate_result_with_base_distance")) {
        emit notice(tr("Round %1 computed").arg(round));
        updateSeries();
        return;
    }

    if (op == QLatin1String("update_table_from_capture")) {
        emit notice(tr("Round %1 filled from the capture").arg(round));
        // Then recompute, or derived columns stay stale.
        computeAll();
        return;
    }
}

void CalibrationController::computeAll() {
    sendCaliOp(QStringLiteral("compute_all"), 0, {}, tr("recomputing every round"));
}

void CalibrationController::calculateRound(int round) {
    if (singleDistance_) {
        QJsonObject extra;
        extra[QStringLiteral("distance")] = baseDistance_;
        sendCaliOp(QStringLiteral("calculate_result_single_round"), round, extra,
                   tr("computing round %1").arg(round));
        return;
    }

    QJsonObject extra;
    extra[QStringLiteral("base_distance")] = baseDistance_;
    sendCaliOp(QStringLiteral("calculate_result_with_base_distance"), round, extra,
               tr("computing round %1").arg(round));
}

void CalibrationController::aggregationForRound(int round) {
    QJsonObject extra;
    extra[QStringLiteral("distance")] = baseDistance_;
    sendCaliOp(QStringLiteral("aggregation_by_distance"), round, extra,
               tr("aggregating round %1").arg(round));
}

void CalibrationController::sendNextBand() {
    if (pendingBands_.isEmpty()) return;

    const QJsonObject band = pendingBands_.takeFirst();

    QJsonObject extra;
    extra[QStringLiteral("dirs")] = band.value(QStringLiteral("dirs"));
    extra[QStringLiteral("lo")] = band.value(QStringLiteral("lo"));
    extra[QStringLiteral("hi")] = band.value(QStringLiteral("hi"));

    const int index = bandsFound_ - static_cast<int>(pendingBands_.size());
    sendCaliOp(QStringLiteral("remove_nodes_in_band"), pendingBandRound_, extra,
               tr("removing noise band %1 of %2").arg(index).arg(bandsFound_));
}

void CalibrationController::aggregationAllRounds(bool useRange, double xLo, double xHi) {
    QJsonObject extra;
    extra[QStringLiteral("base_distance")] = baseDistance_;
    extra[QStringLiteral("use_range")] = useRange;
    extra[QStringLiteral("x_lo")] = xLo;
    extra[QStringLiteral("x_hi")] = xHi;
    // The op reads round_enabled, not round.
    sendCaliOp(QStringLiteral("aggregation_all_rounds_by_distance"), 0, extra,
               tr("aggregating every enabled round"));
}

void CalibrationController::cleanNoise(int round) {
    // Detect first, then remove the reported bands.
    sendCaliOp(QStringLiteral("auto_detect_noise_bands"), round, {},
               tr("finding noise bands in round %1").arg(round));
}

void CalibrationController::updateFromCapture(int round, const QVariantList &pct,
                                              const QVariantMap &nodes) {
    QJsonArray pctArray;
    for (const QVariant &v : pct) pctArray.append(v.toString());

    QJsonObject ict8;
    for (const QString &d : dirs8()) {
        QJsonArray values;
        for (const QVariant &v : nodes.value(d).toList()) values.append(v.toDouble());
        ict8[d] = values;
    }

    QJsonObject extra;
    extra[QStringLiteral("pct")] = pctArray;
    extra[QStringLiteral("ict8")] = ict8;

    sendCaliOp(QStringLiteral("update_table_from_capture"), round, extra,
               tr("filling round %1 from the capture").arg(round));
}

// ---- series ----

bool CalibrationController::sendSeries(const QString &kind, const QJsonObject &extra) {
    if (status_ != ProbeStatus::Ok) return false;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(kind)
    Q_UNUSED(extra)
    return false;
#else
    rclcpp::Client<moil_interfaces::srv::CaliSeries>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->series;
    }
    if (!client) return false;

    beginCall(tr("reading %1").arg(kind));
    ++seriesPending_;
    const quint64 generation = d_->generation.load();
    const quint64 token = ++nextToken_;
    const int version = tableVersion_;
    liveTokens_.insert(token);

    QJsonObject p = extra;
    p[QStringLiteral("degree")] = degree_;
    QJsonArray enabled;
    for (const QVariant &v : roundEnabled_) enabled.append(v.toBool());
    p[QStringLiteral("enabled")] = enabled;

    auto request = std::make_shared<moil_interfaces::srv::CaliSeries::Request>();
    request->kind = kind.toStdString();
    request->table_json = tableJson().toStdString();
    request->params_json = dump(p).toStdString();

    client->async_send_request(
        request, [this, generation, token, kind,
                  version](rclcpp::Client<moil_interfaces::srv::CaliSeries>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(
                this, "applySeries", Qt::QueuedConnection, Q_ARG(QString, kind),
                Q_ARG(bool, response->success),
                Q_ARG(QString, QString::fromStdString(response->series_json)),
                Q_ARG(QString, QString::fromStdString(response->message)), Q_ARG(int, version),
                Q_ARG(quint64, token), Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kOpTimeoutMs, this, [this, generation, token] {
        if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
        if (seriesPending_ > 0) --seriesPending_;
        endCall();
    });
    return true;
#endif
}

void CalibrationController::applySeries(const QString &kind, bool ok, const QString &seriesJson,
                                        const QString &message, int version, quint64 token,
                                        quint64 generation) {
    if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
    endCall();
    if (seriesPending_ > 0) --seriesPending_;

    // Stale answers are dropped, not drawn.
    if (version != tableVersion_) return;

    if (!ok) {
        setLastError(message.isEmpty() ? tr("could not read %1").arg(kind) : message);
        return;
    }

    const QJsonObject r = QJsonDocument::fromJson(seriesJson.toUtf8()).object();

    if (kind == QLatin1String("ih_alpha") || kind == QLatin1String("ict_zfl")) {
        QVariantList series;
        QVariantList colors;
        for (const QJsonValue &v : r.value(QStringLiteral("series")).toArray()) {
            const QJsonObject s = v.toObject();
            series.append(QVariant(pointsToVariant(s.value(QStringLiteral("pts")).toArray())));
            colors.append(s.value(QStringLiteral("color")).toString());
        }
        if (kind == QLatin1String("ih_alpha")) {
            alphaRounds_ = series;
            roundColors_ = colors;
        } else {
            zflRounds_ = series;
        }
    } else if (kind == QLatin1String("ih_alpha_regression")) {
        alphaFit_ = pointsToVariant(r.value(QStringLiteral("pts")).toArray());
    } else if (kind == QLatin1String("global_ict_alpha")) {
        globalIctAlpha_ = pointsToVariant(r.value(QStringLiteral("pts")).toArray());
    } else if (kind == QLatin1String("ict_zfl_points")) {
        roundPoints_ = pointsToVariant(r.value(QStringLiteral("pts")).toArray());
        emit roundPointsChanged();
    } else if (kind == QLatin1String("alpha_polynomial")) {
        QVariantList coefficients;
        for (const QJsonValue &v : r.value(QStringLiteral("coeffs")).toArray())
            coefficients.append(QString::number(v.toDouble(), 'g', 6));
        while (coefficients.size() < 6) coefficients.append(QString());
        coefficients_ = coefficients;
    } else if (kind == QLatin1String("max_ict_all_rounds")) {
        maxIct_ = r.value(QStringLiteral("value")).toDouble();
    } else if (kind == QLatin1String("has_any_raw_ict")) {
        hasRawIct_ = r.value(QStringLiteral("value")).toBool();
    }

    if (seriesPending_ == 0) seriesVersion_ = version;
    emit seriesChanged();
}

void CalibrationController::updateSeries() {
    if (!ready()) return;

    sendSeries(QStringLiteral("has_any_raw_ict"), {});
    sendSeries(QStringLiteral("max_ict_all_rounds"), {});
    sendSeries(QStringLiteral("ih_alpha"), {});
    sendSeries(QStringLiteral("ih_alpha_regression"), {});
    sendSeries(QStringLiteral("ict_zfl"), {});
    sendSeries(QStringLiteral("alpha_polynomial"), {});
    // Every enabled round's (ict, alpha) pooled.
    sendSeries(QStringLiteral("global_ict_alpha"), {});
}

void CalibrationController::fetchRoundPoints(int round) {
    if (!ready()) return;

    roundPointsRound_ = round;
    emit roundPointsChanged();

    QJsonObject extra;
    extra[QStringLiteral("round")] = round;
    sendSeries(QStringLiteral("ict_zfl_points"), extra);
}

// ---- distance searches (CaliJob) ----
// Cancellable; a cancelled search answers partially.

void CalibrationController::startSearch(const QString &op, const QJsonObject &extra, int round) {
    if (searchRunning_) {
        setLastError(tr("a search is already running -- stop it first"));
        return;
    }
    if (!ready()) return;

#ifndef FISHEYE_ROS_ENABLED
    Q_UNUSED(op)
    Q_UNUSED(extra)
    Q_UNUSED(round)
#else
    rclcpp_action::Client<moil_interfaces::action::CaliJob>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->job;
    }
    if (!client) return;

    if (!client->action_server_is_ready()) {
        setLastError(tr("nothing is serving %1 -- the compute node is running but has no "
                        "search action")
                         .arg(QString::fromLatin1(kCaliJobAction)));
        return;
    }

    searchRunning_ = true;
    searchStage_ = tr("starting");
    searchSummary_.clear();
    searchDone_ = 0;
    searchTotal_ = 0;
    searchSamples_.clear();
    emit searchChanged();
    beginCall(tr("searching: %1").arg(op));

    const quint64 generation = d_->generation.load();

    moil_interfaces::action::CaliJob::Goal goal;
    goal.op = op.toStdString();
    goal.table_json = tableJson().toStdString();
    goal.params_json = paramsJson(round, extra).toStdString();

    using GoalHandle = rclcpp_action::ClientGoalHandle<moil_interfaces::action::CaliJob>;
    rclcpp_action::Client<moil_interfaces::action::CaliJob>::SendGoalOptions options;

    options.goal_response_callback = [this, generation](GoalHandle::SharedPtr handle) {
        if (handle) {
            std::lock_guard<std::mutex> lock(d_->clientMutex);
            d_->goal = handle;
        }
        QMetaObject::invokeMethod(this, "applySearchAccepted", Qt::QueuedConnection,
                                  Q_ARG(bool, handle != nullptr),
                                  Q_ARG(QString, handle ? QString()
                                                        : tr("the rig rejected the search")),
                                  Q_ARG(quint64, generation));
    };

    options.feedback_callback =
        [this, generation](GoalHandle::SharedPtr,
                           const std::shared_ptr<const moil_interfaces::action::CaliJob::Feedback>
                               feedback) {
            QMetaObject::invokeMethod(this, "applySearchFeedback", Qt::QueuedConnection,
                                      Q_ARG(int, feedback->done), Q_ARG(int, feedback->total),
                                      Q_ARG(QString, QString::fromStdString(feedback->stage)),
                                      Q_ARG(quint64, generation));
        };

    options.result_callback = [this, generation](const GoalHandle::WrappedResult &result) {
        {
            std::lock_guard<std::mutex> lock(d_->clientMutex);
            d_->goal.reset();
        }

        const bool reached = result.code == rclcpp_action::ResultCode::SUCCEEDED ||
                             result.code == rclcpp_action::ResultCode::CANCELED;
        const bool ok = reached && result.result && result.result->success;
        const bool cancelled = result.result && result.result->cancelled;

        QMetaObject::invokeMethod(
            this, "applySearchResult", Qt::QueuedConnection, Q_ARG(bool, ok),
            Q_ARG(bool, cancelled),
            Q_ARG(QString, result.result ? QString::fromStdString(result.result->table_json)
                                         : QString()),
            Q_ARG(QString, result.result ? QString::fromStdString(result.result->result_json)
                                         : QString()),
            Q_ARG(QString, result.result ? QString::fromStdString(result.result->message)
                                         : tr("the search was aborted by the rig")),
            Q_ARG(quint64, generation));
    };

    client->async_send_goal(goal, options);
#endif
}

void CalibrationController::applySearchAccepted(bool accepted, const QString &message,
                                                quint64 generation) {
    if (generation != d_->generation.load() || !searchRunning_) return;

    if (!accepted) {
        searchRunning_ = false;
        searchStage_.clear();
        endCall();
        setLastError(message);
        emit searchChanged();
        return;
    }
    searchStage_ = tr("running");
    emit searchChanged();
}

void CalibrationController::applySearchFeedback(int done, int total, const QString &stage,
                                                quint64 generation) {
    if (generation != d_->generation.load() || !searchRunning_) return;

    searchDone_ = done;
    // 0 means indeterminate, not zero progress.
    searchTotal_ = total;
    searchStage_ = stage.isEmpty() ? tr("running") : stage;
    emit searchChanged();
}

void CalibrationController::applySearchResult(bool ok, bool cancelled, const QString &tableJson,
                                              const QString &resultJson, const QString &message,
                                              quint64 generation) {
    if (generation != d_->generation.load()) return;

    searchRunning_ = false;
    searchStage_.clear();
    endCall();

    if (!tableJson.isEmpty()) {
        const QJsonObject next = QJsonDocument::fromJson(tableJson.toUtf8()).object();
        if (!next.isEmpty()) {
            table_ = next;
            bumpVersion();
        }
    }

    if (!ok) {
        searchSummary_ = message.isEmpty() ? tr("the search failed") : message;
        setLastError(searchSummary_);
        emit searchChanged();
        return;
    }

    const QJsonObject r = QJsonDocument::fromJson(resultJson.toUtf8()).object();
    const bool found = r.value(QStringLiteral("found")).toBool(false);
    bestDistance_ = r.value(QStringLiteral("best_distance")).toDouble();
    bestAggregation_ = r.value(QStringLiteral("best_aggr")).toDouble();

    searchSamples_.clear();
    for (const QJsonValue &v : r.value(QStringLiteral("samples")).toArray()) {
        const QJsonArray pair = v.toArray();
        if (pair.size() < 2) continue;
        QVariantMap point;
        point[QStringLiteral("x")] = pair.at(0).toDouble();
        point[QStringLiteral("y")] = pair.at(1).toDouble();
        searchSamples_.append(point);
    }

    if (!found) {
        searchSummary_ = tr("no distance found -- nothing to aggregate over");
    } else if (cancelled) {
        // Say partial outright.
        searchSummary_ = tr("stopped early: best so far %1 at distance %2 (partial)")
                             .arg(bestAggregation_, 0, 'f', 4)
                             .arg(bestDistance_, 0, 'f', 2);
    } else {
        searchSummary_ = tr("best aggregation %1 at distance %2")
                             .arg(bestAggregation_, 0, 'f', 4)
                             .arg(bestDistance_, 0, 'f', 2);
    }

    setLastError(QString());
    emit searchChanged();
    emit notice(searchSummary_);
    if (found && !cancelled) setBaseDistance(bestDistance_);
}

void CalibrationController::cancelSearch() {
    if (!searchRunning_) return;

#ifdef FISHEYE_ROS_ENABLED
    rclcpp_action::Client<moil_interfaces::action::CaliJob>::SharedPtr client;
    rclcpp_action::ClientGoalHandle<moil_interfaces::action::CaliJob>::SharedPtr goal;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->job;
        goal = d_->goal;
    }
    if (client && goal) {
        client->async_cancel_goal(goal);
        searchStage_ = tr("stopping");
        emit searchChanged();
        emit notice(tr("Asked the rig to stop the search; it will answer with the table as "
                       "the last probe left it."));
        return;
    }
#endif

    // Nothing to cancel remotely.
    searchRunning_ = false;
    searchStage_.clear();
    endCall();
    emit searchChanged();
}

void CalibrationController::findMinForRound(int round, double distMin, double distMax) {
    QJsonObject extra;
    extra[QStringLiteral("dist_min")] = distMin;
    extra[QStringLiteral("dist_max")] = distMax;
    startSearch(QStringLiteral("find_min_aggr_single_round"), extra, round);
}

void CalibrationController::findMinInWindow(bool useWindow, double xLo, double xHi) {
    QJsonObject extra;
    extra[QStringLiteral("use_window")] = useWindow;
    extra[QStringLiteral("x_lo")] = xLo;
    extra[QStringLiteral("x_hi")] = xHi;
    startSearch(QStringLiteral("find_min_aggregation_in_window"), extra, 0);
}

void CalibrationController::findDistanceForTarget(double target, bool useRange, double xLo,
                                                  double xHi) {
    QJsonObject extra;
    extra[QStringLiteral("target")] = target;
    extra[QStringLiteral("use_range")] = useRange;
    extra[QStringLiteral("x_lo")] = xLo;
    extra[QStringLiteral("x_hi")] = xHi;
    startSearch(QStringLiteral("find_distance_for_target_aggregation"), extra, 0);
}

// ---- Excel ----

void CalibrationController::loadExcel(int round, const QUrl &fileUrl) {
    if (!ready()) return;

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(tr("cannot read %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();

#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::XlsxIo>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->xlsx;
    }
    if (!client) return;

    const QString name = QFileInfo(path).fileName();
    beginCall(tr("reading %1").arg(name));
    const quint64 generation = d_->generation.load();
    const quint64 token = ++nextToken_;
    liveTokens_.insert(token);

    // Path is the operator's; parsing is the rig's.
    auto request = std::make_shared<moil_interfaces::srv::XlsxIo::Request>();
    request->mode = "read";
    request->sheet = "";
    request->grid_json = "";
    request->data.assign(bytes.constBegin(), bytes.constEnd());

    client->async_send_request(
        request, [this, generation, token, round,
                  name](rclcpp::Client<moil_interfaces::srv::XlsxIo>::SharedFuture future) {
            const auto response = future.get();
            QMetaObject::invokeMethod(
                this, "applyXlsxRead", Qt::QueuedConnection, Q_ARG(int, round),
                Q_ARG(bool, response->success),
                Q_ARG(QString, QString::fromStdString(response->grid_json)),
                Q_ARG(QString, QString::fromStdString(response->message)), Q_ARG(QString, name),
                Q_ARG(quint64, token), Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kOpTimeoutMs, this, [this, generation, token, name] {
        if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
        endCall();
        if (pendingLoads_ > 0) --pendingLoads_;
        setLastError(tr("reading %1 timed out").arg(name));
    });
#else
    Q_UNUSED(round)
    Q_UNUSED(bytes)
#endif
}

void CalibrationController::applyXlsxRead(int round, bool ok, const QString &gridJson,
                                          const QString &message, const QString &name,
                                          quint64 token, quint64 generation) {
    if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
    endCall();

    const bool partOfBatch = pendingLoads_ > 0;
    if (partOfBatch) --pendingLoads_;

    if (!ok) {
        const QString why = message.isEmpty() ? tr("%1 did not parse").arg(name) : message;
        setLastError(why);
        loadFailures_ << name;
        if (partOfBatch && pendingLoads_ == 0) {
            loadStatus_ = loadedOk_ > 0 ? ProbeStatus::Partial : ProbeStatus::Failed;
            loadSummary_ = tr("%1 loaded, %2 failed").arg(loadedOk_).arg(loadFailures_.size());
            emit tableChanged();
        } else if (!partOfBatch) {
            loadStatus_ = ProbeStatus::Failed;
            loadSummary_ = why;
            emit tableChanged();
        }
        return;
    }

    // grid_json is the sheet as text rows.
    const QJsonArray grid = QJsonDocument::fromJson(gridJson.toUtf8()).array();

    QJsonObject rounds = table_.value(QStringLiteral("rounds")).toObject();
    QJsonObject r = rounds.value(QString::number(round)).toObject();
    QJsonObject cells;

    int filled = 0;
    for (int row = 0; row < grid.size() && row < kRows; ++row) {
        const QJsonArray line = grid.at(row).toArray();
        QJsonObject rowObject;
        for (int col = 0; col < line.size() && col < kCols; ++col) {
            const QString text = line.at(col).toString().trimmed();
            if (text.isEmpty()) continue;
            rowObject[QString::number(col)] = text;
            ++filled;
        }
        if (!rowObject.isEmpty()) cells[QString::number(row)] = rowObject;
    }

    r[QStringLiteral("rows")] = kRows;
    r[QStringLiteral("cols")] = kCols;
    r[QStringLiteral("cells")] = cells;
    rounds[QString::number(round)] = r;
    table_[QStringLiteral("rounds")] = rounds;

    ++loadedOk_;
    setLastError(QString());
    // Only this round changed.
    bumpVersion(round);

    if (!partOfBatch || pendingLoads_ == 0) {
        loadStatus_ = loadFailures_.isEmpty() ? ProbeStatus::Ok : ProbeStatus::Partial;
        loadSummary_ = loadFailures_.isEmpty()
                           ? tr("%1 round(s) loaded").arg(loadedOk_)
                           : tr("%1 loaded, %2 failed").arg(loadedOk_).arg(loadFailures_.size());
        emit tableChanged();
        emit notice(loadSummary_);
    }

    if (!partOfBatch)
        emit notice(tr("%1 -> round %2, %3 cell(s)").arg(name).arg(round).arg(filled));
}

void CalibrationController::loadAllExcel(const QUrl &folderUrl) {
    if (!ready()) return;

    const QString path = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    setFolder(path);

    QDir dir(path);
    QStringList files = dir.entryList({QStringLiteral("*.xlsx")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        loadStatus_ = ProbeStatus::Failed;
        loadSummary_ = tr("no .xlsx files in %1").arg(QDir(path).dirName());
        setLastError(loadSummary_);
        emit tableChanged();
        return;
    }

    loadedOk_ = 0;
    loadFailures_.clear();
    pendingLoads_ = 0;

    // Numbered names win; the rest fill free slots.
    QSet<int> taken;
    QList<QPair<int, QString>> plan;
    QStringList unnumbered;

    for (const QString &file : files) {
        const int r = roundFromName(file);
        if (r > 0 && !taken.contains(r)) {
            taken.insert(r);
            plan.append({r, file});
        } else {
            unnumbered << file;
        }
    }
    for (const QString &file : unnumbered) {
        for (int r = 1; r <= 10; ++r) {
            if (taken.contains(r)) continue;
            taken.insert(r);
            plan.append({r, file});
            break;
        }
    }

    if (plan.isEmpty()) {
        loadStatus_ = ProbeStatus::Failed;
        loadSummary_ = tr("nothing in %1 could be mapped to a round").arg(QDir(path).dirName());
        setLastError(loadSummary_);
        emit tableChanged();
        return;
    }

    pendingLoads_ = plan.size();
    loadStatus_ = ProbeStatus::Checking;
    loadSummary_ = tr("loading %1 file(s)...").arg(plan.size());
    emit tableChanged();

    for (const auto &entry : plan)
        loadExcel(entry.first, QUrl::fromLocalFile(dir.filePath(entry.second)));
}

void CalibrationController::saveExcel(int round, const QUrl &fileUrl) {
    if (!ready()) return;

    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();

#ifdef FISHEYE_ROS_ENABLED
    rclcpp::Client<moil_interfaces::srv::XlsxIo>::SharedPtr client;
    {
        std::lock_guard<std::mutex> lock(d_->clientMutex);
        client = d_->xlsx;
    }
    if (!client) return;

    // The Excel grid is dense, unlike storage.
    QJsonArray grid;
    for (int row = 0; row < kRows; ++row) {
        QJsonArray line;
        for (int col = 0; col < kCols; ++col) line.append(cell(round, row, col));
        grid.append(line);
    }

    beginCall(tr("writing %1").arg(QFileInfo(path).fileName()));
    const quint64 generation = d_->generation.load();
    const quint64 token = ++nextToken_;
    liveTokens_.insert(token);

    auto request = std::make_shared<moil_interfaces::srv::XlsxIo::Request>();
    request->mode = "write";
    request->sheet = "";
    request->grid_json =
        QString::fromUtf8(QJsonDocument(grid).toJson(QJsonDocument::Compact)).toStdString();

    client->async_send_request(
        request, [this, generation, token,
                  path](rclcpp::Client<moil_interfaces::srv::XlsxIo>::SharedFuture future) {
            const auto response = future.get();
            const QByteArray data(reinterpret_cast<const char *>(response->data.data()),
                                  qsizetype(response->data.size()));
            QMetaObject::invokeMethod(this, "applyXlsxWrite", Qt::QueuedConnection,
                                      Q_ARG(bool, response->success), Q_ARG(QByteArray, data),
                                      Q_ARG(QString, QString::fromStdString(response->message)),
                                      Q_ARG(QString, path), Q_ARG(quint64, token),
                                      Q_ARG(quint64, generation));
        });

    QTimer::singleShot(kOpTimeoutMs, this, [this, generation, token, path] {
        if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
        endCall();
        setLastError(tr("writing %1 timed out").arg(QFileInfo(path).fileName()));
    });
#else
    Q_UNUSED(round)
    Q_UNUSED(path)
#endif
}

void CalibrationController::applyXlsxWrite(bool ok, const QByteArray &data, const QString &message,
                                           const QString &path, quint64 token,
                                           quint64 generation) {
    if (generation != d_->generation.load() || !liveTokens_.remove(token)) return;
    endCall();

    const QString name = QFileInfo(path).fileName();

    if (!ok || data.isEmpty()) {
        setLastError(message.isEmpty() ? tr("the rig produced no workbook for %1").arg(name)
                                       : message);
        return;
    }

    // QSaveFile: a failed write keeps the old file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(tr("cannot write %1: %2").arg(name, file.errorString()));
        return;
    }
    file.write(data);
    if (!file.commit()) {
        setLastError(tr("cannot write %1: %2").arg(name, file.errorString()));
        return;
    }

    setLastError(QString());
    emit notice(tr("Saved %1").arg(name));
}
