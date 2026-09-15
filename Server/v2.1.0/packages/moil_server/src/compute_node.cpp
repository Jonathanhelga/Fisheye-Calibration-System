#include "compute_node.h"

#include <thread>

#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "CaliCompute.h"
#include "CaliTableData.h"
#include "ComputeOps.h"
#include "XlsxIO.h"
#include "server_context.h"

namespace {

std::vector<cv::Mat> decodeAll(const std::vector<sensor_msgs::msg::CompressedImage> &imgs,
                               QString *err) {
    std::vector<cv::Mat> out;
    out.reserve(imgs.size());
    for (const auto &img : imgs) {
        const cv::Mat buf(1, static_cast<int>(img.data.size()), CV_8U,
                          const_cast<unsigned char *>(img.data.data()));
        cv::Mat m = cv::imdecode(buf, cv::IMREAD_COLOR);
        if (m.empty()) {
            *err = QStringLiteral("could not decode one of the images (format=%1, %2 bytes)")
                       .arg(QString::fromStdString(img.format))
                       .arg(img.data.size());
            return {};
        }
        out.push_back(std::move(m));
    }
    return out;
}

QByteArray encodePngMat(const cv::Mat &m, int maxSide) {
    cv::Mat out = m;
    const int side = std::max(m.cols, m.rows);
    if (maxSide > 0 && side > maxSide) {
        const double s = static_cast<double>(maxSide) / side;
        cv::resize(m, out, cv::Size(), s, s, cv::INTER_AREA);
    }
    std::vector<unsigned char> buf;
    cv::imencode(".png", out, buf);
    return QByteArray(reinterpret_cast<const char *>(buf.data()), static_cast<int>(buf.size()));
}

QJsonArray pointsToJson(const QVector<QPointF> &pts) {
    QJsonArray a;
    for (const QPointF &p : pts) a.append(QJsonArray{p.x(), p.y()});
    return a;
}

// The three searches are actions, not services, and asking for one here is
// rejected by name. Blocking a service call for a minute with no way to cancel is
// the failure this check exists to prevent, and it is a mistake a client makes
// once -- so it is named clearly rather than merely timing out.
bool isSearchOp(const QString &op) {
    return op == ComputeOps::cali::kFindMinAggrSingleRound ||
           op == ComputeOps::cali::kFindMinAggregationInWindow ||
           op == ComputeOps::cali::kFindDistanceForTargetAggregation;
}

// Apply the per-round enable flags the operator set. The server has no other way
// to know which rounds count, and every series and aggregation depends on it.
void applyEnabled(CaliCompute &cc, const QJsonObject &params) {
    const QJsonArray en = params.value(QStringLiteral("enabled")).toArray();
    for (int i = 0; i < en.size() && i <= 10; ++i) cc.setRoundEnabled(i, en.at(i).toBool(true));
}

}  // namespace

ComputeNode::ComputeNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_compute", ctx, options) {
    addService<DetectOp>("/compute/detect", &ComputeNode::onDetect);
    addService<CaliOp>("/compute/cali", &ComputeNode::onCali);
    addService<CaliSeries>("/compute/series", &ComputeNode::onSeries);
    addService<RenderPattern>("/compute/render_pattern", &ComputeNode::onRenderPattern);
    addService<XlsxIoSrv>("/compute/xlsx", &ComputeNode::onXlsx);
    addService<SelfTest>("/compute/self_test", &ComputeNode::onSelfTest);

    addDetachedAction<CaliJob>("/compute/cali_job", &ComputeNode::executeCaliJob);

    RCLCPP_INFO(get_logger(), "compute: calibration maths, plot series, pattern render, xlsx");
}

// ------------------------------------------------------------ image analysis ----

void ComputeNode::onDetect(const DetectOp::Request &req, DetectOp::Response &res) {
    const QString op = QString::fromStdString(req.op);

    // Reject a wrong image count before decoding anything: decoding a 3040x3040
    // frame to discover the request was malformed is 40 ms wasted per mistake.
    const int want = ComputeOps::detectImageCount(op);
    if (want < 0) {
        res.success = false;
        res.message = "unknown detect op: " + req.op;
        return;
    }
    if (static_cast<int>(req.images.size()) != want) {
        res.success = false;
        res.message = "op " + req.op + " wants " + std::to_string(want) + " image(s), got " +
                       std::to_string(req.images.size());
        return;
    }

    QString err;
    const std::vector<cv::Mat> mats = decodeAll(req.images, &err);
    if (mats.size() != req.images.size()) {
        res.success = false;
        res.message = err.toStdString();
        return;
    }

    // noise_cleaning is a process-wide flag inside MoilCali, so two ops running at
    // once would set it for each other. Serialised here rather than inside the
    // engine, because the engine is also used single-threaded by the tests and
    // paying for a lock there would be paying for this server everywhere.
    std::lock_guard<std::mutex> lock(ctx_.computeMutex);
    const QString out =
        ComputeOps::runDetectOp(op, mats, QString::fromStdString(req.params_json), &err);
    res.success = err.isEmpty();
    res.result_json = out.toStdString();
    res.message = err.toStdString();
}

// -------------------------------------------------------- calibration pipeline ----

void ComputeNode::onCali(const CaliOp::Request &req, CaliOp::Response &res) {
    const QString op = QString::fromStdString(req.op);
    if (isSearchOp(op)) {
        res.success = false;
        res.message = "op " + req.op +
                       " is a search: use the /compute/cali_job action, which reports progress "
                       "and can be cancelled";
        return;
    }

    bool ok = false;
    CaliTableData table = CaliTableData::fromJson(QString::fromStdString(req.table_json), &ok);
    if (!ok) {
        res.success = false;
        res.message = "table_json did not parse";
        return;
    }

    // ---- B4 diagnostic: the blank Side column after Update Table -------------
    //
    // The operator reports the Side column empty after Update Table. Reading the
    // code rules out most of the obvious causes -- the client sends the widget's
    // real rowCount, the right op is dispatched, computeAll does not clear "side",
    // column 1 is inside every table, and the JSON round trip keeps non-empty
    // cells -- so what is left needs a look at the actual numbers.
    //
    // The HANDOFF's hypothesis (rows == 2, so maxLayer == 0 and the Side loop
    // writes nothing, "while the ict block still fills") cannot be right on its
    // own: every ict write in updateTableFromCapture is guarded by the SAME
    // maxLayer, so rows == 2 blanks ict too. This line prints both counts so the
    // two cases are told apart the first time the button is pressed, instead of
    // by a second round of guessing:
    //
    //   side=0 ict=0   -> the round is empty or maxLayer is 0: a table problem,
    //                     and `rows` below says which
    //   side=0 ict>0   -> genuinely Side-specific, which no current reading of
    //                     the engine explains -- look at the reply, not the op
    //   side>0         -> the server filled it; the loss is on the way back or in
    //                     the client's applyFrom, not here
    //
    // Cheap and only on this one op, so it can stay: one line per Update Table.
    const bool diagUpdate = (op == QLatin1String(ComputeOps::cali::kUpdateTableFromCapture));
    int diagRound = -1;
    if (diagUpdate) {
        diagRound = QJsonDocument::fromJson(QByteArray::fromStdString(req.params_json))
                        .object()
                        .value("table_index")
                        .toInt(-1);
    }

    QString result, err;
    std::lock_guard<std::mutex> lock(ctx_.computeMutex);
    const bool done =
        ComputeOps::runCaliOp(op, table, QString::fromStdString(req.params_json), &result, &err);

    if (diagUpdate && diagRound >= 0) {
        const int rows = table.rowCount(diagRound);
        // Layer rows only: row 0 and 1 are headers, and row 1 holds the side-LAYER
        // metadata rather than a layer index, so counting from row 2 is what
        // "the Side column" means to the operator looking at the screen.
        int side = 0, ict = 0;
        for (int r = 2; r < rows; ++r) {
            if (!table.cell(diagRound, r, 1).isEmpty()) ++side;   // "side"
            if (!table.cell(diagRound, r, 3).isEmpty()) ++ict;    // "ict_n"
        }
        RCLCPP_INFO(get_logger(),
                    "update_table_from_capture: round=%d present=%d rows=%d "
                    "maxLayer=%d side_filled=%d ict_n_filled=%d",
                    diagRound, table.hasRound(diagRound) ? 1 : 0, rows,
                    rows > 2 ? rows - 2 : 0, side, ict);
    }

    // The mutated table goes back even on failure: an op that got part way has
    // already written derived columns, and the client's display must match what
    // the server actually holds rather than silently keeping the pre-call state.
    res.table_json = table.toJson().toStdString();
    res.result_json = result.isEmpty() ? "{}" : result.toStdString();
    res.success = done;
    res.message = err.toStdString();
}

void ComputeNode::onSeries(const CaliSeries::Request &req, CaliSeries::Response &res) {
    bool ok = false;
    CaliTableData table = CaliTableData::fromJson(QString::fromStdString(req.table_json), &ok);
    if (!ok) {
        res.success = false;
        res.message = "table_json did not parse";
        return;
    }

    const QJsonObject params =
        QJsonDocument::fromJson(QByteArray::fromStdString(req.params_json)).object();
    const QString kind = QString::fromStdString(req.kind);

    CaliCompute cc(&table);
    applyEnabled(cc, params);

    const auto seriesArray = [](const QVector<CaliCompute::Series> &ss) {
        QJsonArray a;
        for (const CaliCompute::Series &s : ss) {
            QJsonObject o;
            o["round"] = s.round;
            o["color"] = s.color.name();
            o["pts"] = pointsToJson(s.pts);
            a.append(o);
        }
        return a;
    };

    QJsonObject out;
    std::lock_guard<std::mutex> lock(ctx_.computeMutex);

    if (kind == "ict_zfl") {
        out["series"] = seriesArray(cc.ictZflSeries());
    } else if (kind == "ih_alpha") {
        out["series"] = seriesArray(cc.ihAlphaSeries());
    } else if (kind == "ict_zfl_points") {
        out["pts"] = pointsToJson(cc.ictZflPoints(params.value("round").toInt(0)));
    } else if (kind == "ih_alpha_regression") {
        out["pts"] = pointsToJson(cc.ihAlphaRegression(params.value("degree").toInt(4)));
    } else if (kind == "global_ict_alpha") {
        out["pts"] = pointsToJson(cc.globalIctAlpha());
    } else if (kind == "alpha_polynomial") {
        QJsonArray c;
        for (double d : cc.alphaPolynomial(params.value("degree").toInt(4))) c.append(d);
        out["coeffs"] = c;
    } else if (kind == "max_ict_all_rounds") {
        out["value"] = cc.maxIctAllRounds();
    } else if (kind == "has_any_raw_ict") {
        out["value"] = cc.hasAnyRawIct();
    } else {
        res.success = false;
        res.message = "unknown series kind: " + req.kind;
        return;
    }

    res.success = true;
    res.series_json = QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact))
                           .toStdString();
    res.message = "";
}

// ------------------------------------------------------------------- searches ----

void ComputeNode::executeCaliJob(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<CaliJob>> gh) {
    const auto goal = gh->get_goal();
    auto result = std::make_shared<CaliJob::Result>();

    bool ok = false;
    CaliTableData table = CaliTableData::fromJson(QString::fromStdString(goal->table_json), &ok);
    if (!ok) {
        result->success = false;
        result->message = "table_json did not parse";
        gh->abort(result);
        return;
    }

    // Return false to cancel. The engine unwinds the search wherever it is and
    // leaves the table at whatever the last probe wrote -- which is exactly what
    // the v2.0 progress dialog's Cancel did, and why the result carries the table
    // even when cancelled.
    const ComputeOps::ProgressFn progress = [gh](int done, int total, const QString &stage) {
        auto fb = std::make_shared<CaliJob::Feedback>();
        fb->done = done;
        fb->total = total;
        fb->stage = stage.toStdString();
        gh->publish_feedback(fb);
        return !gh->is_canceling();
    };

    QString resultJson, err;
    {
        std::lock_guard<std::mutex> lock(ctx_.computeMutex);
        ComputeOps::runCaliOp(QString::fromStdString(goal->op), table,
                              QString::fromStdString(goal->params_json), &resultJson, &err,
                              progress);
    }

    result->table_json = table.toJson().toStdString();
    result->result_json = resultJson.isEmpty() ? "{}" : resultJson.toStdString();
    result->cancelled = gh->is_canceling();
    // A cancelled search produced a partial answer; it did not fail.
    result->success = err.isEmpty() || result->cancelled;
    result->message = err.toStdString();

    if (result->cancelled) gh->canceled(result);
    else gh->succeed(result);
}

// ------------------------------------------------------------ pattern + xlsx ----

void ComputeNode::onRenderPattern(const RenderPattern::Request &req, RenderPattern::Response &res) {
    QString err;
    const cv::Mat m = ComputeOps::renderPattern(QString::fromStdString(req.pattern_json),
                                                req.width, req.height, &err);
    if (m.empty()) {
        res.success = false;
        res.message = err.isEmpty() ? "renderer produced nothing" : err.toStdString();
        return;
    }
    res.width = m.cols;
    res.height = m.rows;
    const QByteArray png = encodePngMat(m, req.max_side);
    res.image.header.stamp = now();
    res.image.format = "png";
    res.image.data.assign(png.begin(), png.end());
    res.success = true;
    res.message = "";
}

void ComputeNode::onXlsx(const XlsxIoSrv::Request &req, XlsxIoSrv::Response &res) {
    // XlsxIO works on paths, not buffers -- it shells out to the system zip tools.
    // Rather than change it (it is the same code the v2.0 app is verified against),
    // the bytes are staged through a temporary directory that is removed when this
    // scope ends. The operator's chosen path stays on the client, which is where
    // the operator is.
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        res.success = false;
        res.message = "could not create a temporary directory for the xlsx";
        return;
    }
    const QString path = tmp.filePath(QStringLiteral("sheet.xlsx"));
    const QString mode = QString::fromStdString(req.mode).trimmed().toLower();

    if (mode == "read") {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            res.success = false;
            res.message = "could not stage the uploaded xlsx";
            return;
        }
        f.write(reinterpret_cast<const char *>(req.data.data()),
                static_cast<qint64>(req.data.size()));
        f.close();

        QString sheet = QString::fromStdString(req.sheet);
        XlsxIO::Grid grid = XlsxIO::read(path, sheet.isEmpty() ? QStringLiteral("Sheet") : sheet);
        // Files saved by older versions of this app do not all name their sheet,
        // so a miss falls back to the first one. Same order the v2.0 loader used.
        if (grid.isEmpty()) grid = XlsxIO::read(path);

        QJsonArray rows;
        for (const QVector<QString> &r : grid) {
            QJsonArray cols;
            for (const QString &c : r) cols.append(c);
            rows.append(cols);
        }
        res.grid_json =
            QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact)).toStdString();
        res.success = !grid.isEmpty();
        res.message = grid.isEmpty() ? "no readable sheet in that file" : "";
        return;
    }

    if (mode == "write") {
        const QJsonArray rows =
            QJsonDocument::fromJson(QByteArray::fromStdString(req.grid_json)).array();
        XlsxIO::Grid grid;
        grid.reserve(rows.size());
        for (const QJsonValue &r : rows) {
            QVector<QString> row;
            for (const QJsonValue &c : r.toArray()) row.push_back(c.toString());
            grid.push_back(row);
        }
        if (!XlsxIO::write(path, grid)) {
            res.success = false;
            res.message = "xlsx write failed";
            return;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            res.success = false;
            res.message = "xlsx was written but could not be read back";
            return;
        }
        const QByteArray bytes = f.readAll();
        res.data.assign(bytes.begin(), bytes.end());
        res.success = true;
        res.message = "";
        return;
    }

    res.success = false;
    res.message = "mode must be \"read\" or \"write\"";
}

void ComputeNode::onSelfTest(const SelfTest::Request &req, SelfTest::Response &res) {
    double seconds = 0.0;
    const auto suites = ComputeSelfTest::run(QString::fromStdString(req.suite), &seconds);

    res.passed = true;
    for (const auto &s : suites) {
        res.suites.push_back(s.name.toStdString());
        res.suite_passed.push_back(s.passed);
        res.max_deviation.push_back(s.maxDeviation);
        for (const QString &n : s.notes) res.notes.push_back(n.toStdString());
        if (!s.passed) res.passed = false;
    }
    res.duration = seconds;
    res.success = !suites.isEmpty();
    res.message = suites.isEmpty() ? "unknown suite" : "";
}
