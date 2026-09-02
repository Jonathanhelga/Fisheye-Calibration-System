#include "jobs_node.h"

#include <thread>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QThread>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "CaliTableData.h"
#include "ComputeOps.h"
#include "XlsxIO.h"
#include "server_context.h"

namespace {

// Measured against this rig; ported unchanged from the v2.0 run.
constexpr int kChessSettleMs = 1500;   // panel settle after a pattern change
constexpr int kShotSettleMs = 800;     // after a pattern change, before the shutter
constexpr int kAxisPollMs = 500;
constexpr int kAxisMaxMs = 90000;
constexpr int kAxisSettleMs = 1500;
constexpr int kZSettleMs = 2000;
constexpr double kChessXStepMm = 20.0;

const char *kSideDirections[4] = {"n", "w", "s", "e"};

}  // namespace

JobsNode::JobsNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_jobs", ctx, options) {
    addDetachedAction<AutoCalibrate>("/jobs/auto_calibrate", &JobsNode::executeAutoCalibrate);

    RCLCPP_INFO(get_logger(), "jobs: auto-calibrate");
}

bool JobsNode::pushRoundPatterns(const QString &topSpec, const QString &sideSpec) {
    bool ok = true;
    if (!topSpec.isEmpty()) ok = ctx_.monitor->show_pattern_spec(QStringLiteral("top"), topSpec) && ok;
    if (!sideSpec.isEmpty())
        for (const char *d : kSideDirections)
            ok = ctx_.monitor->show_pattern_spec(QString::fromLatin1(d), sideSpec) && ok;
    return ok;
}

QByteArray JobsNode::captureTo(const QString &path) {
    const QByteArray bytes = ctx_.camera->single_image();
    if (bytes.isEmpty()) return {};
    // Remove first: a failed capture must not leave a stale file that the next
    // step copies as though it were this round's shot.
    QFile::remove(path);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(bytes);
    return bytes;
}

bool JobsNode::waitAxisIdle(const QString &axis, int maxMs, int settleMs,
                            const std::shared_ptr<GoalHandle> &gh) {
    const QString moveSensor = QStringLiteral("is_sensor_%1_move").arg(axis);
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < maxMs) {
        if (gh->is_canceling()) return false;
        const QString v = ctx_.axis->sensor(moveSensor).trimmed().toLower();
        // Unreadable counts as "not moving", the same way the v2.0 wait did. A
        // sensor that cannot be read must not hold a run open indefinitely, and
        // the timeout below is the real backstop either way.
        if (v != QLatin1String("true") && v != QLatin1String("1")) break;
        QThread::msleep(kAxisPollMs);
    }
    QThread::msleep(settleMs);
    return true;
}

void JobsNode::executeAutoCalibrate(const std::shared_ptr<GoalHandle> gh) {
    const auto goal = gh->get_goal();
    auto result = std::make_shared<AutoCalibrate::Result>();

    const QString root = ctx_.sessions->sessionsDir() + "/" +
                         QString::fromStdString(goal->run_name);
    QDir().mkpath(root);

    QStringList rounds;
    for (const std::string &r : goal->rounds) rounds << QString::fromStdString(r);
    if (rounds.isEmpty()) rounds = {"1", "3", "5", "7", "9"};
    for (const QString &r : rounds) QDir(root).mkpath(r);

    const int shots = goal->shots_per_polarity > 0 ? goal->shots_per_polarity : 5;
    const double gapS = goal->shot_interval > 0.0 ? goal->shot_interval : 10.0;
    const double zStep = goal->z_step_mm > 0.0 ? goal->z_step_mm : 20.0;
    const double chessStep =
        goal->chessboard_offset_mm > 0.0 ? goal->chessboard_offset_mm : kChessXStepMm;

    // How far the chessboard step has pushed X from where the run started. That
    // excursion is temporary, so an abort in the middle of it has to undo it; the
    // per-round Z moves are real progress and are left alone.
    double xOffsetMm = 0.0;
    bool cancelled = false;

    const auto feedback = [&](int roundIdx, int done, int total, const QString &stage,
                              const QByteArray &preview) {
        auto fb = std::make_shared<AutoCalibrate::Feedback>();
        fb->round_index = roundIdx;
        fb->round_total = rounds.size();
        fb->done = done;
        fb->total = total;
        fb->stage = stage.toStdString();
        if (!preview.isEmpty()) {
            // Small: the operator is watching the run, not measuring from it.
            const cv::Mat buf(1, preview.size(), CV_8U,
                              const_cast<char *>(preview.constData()));
            cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (!img.empty()) {
                cv::Mat small;
                const double s = 480.0 / std::max(img.cols, img.rows);
                cv::resize(img, small, cv::Size(), s, s, cv::INTER_AREA);
                std::vector<unsigned char> out;
                cv::imencode(".jpg", small, out, {cv::IMWRITE_JPEG_QUALITY, 70});
                fb->preview.format = "jpeg";
                fb->preview.data = std::move(out);
            }
        }
        gh->publish_feedback(fb);
    };

    // ---- chessboard step ---------------------------------------------------
    //
    // It has to happen before the rounds: every pattern shot pushes calibration
    // patterns to all five screens, which would paint over the chessboard.
    if (goal->chessboard_step && !goal->spec_chessboard_top.empty()) {
        const QString camDir =
            goal->camera_name.empty() ? root : root + "/" + QString::fromStdString(goal->camera_name);
        QDir().mkpath(camDir);

        feedback(0, 0, 3, "chessboard", {});
        pushRoundPatterns(QString::fromStdString(goal->spec_chessboard_top),
                          QString::fromStdString(goal->spec_chessboard_side));
        QThread::msleep(kChessSettleMs);

        // 1_org at wherever X started, 2 after +20 mm, 3 after 40 mm the other way.
        struct Shot { const char *file; double preMoveMm; bool right; };
        const Shot kShots[3] = {{"1_org.png", 0.0, true},
                                {"2_x+20mm.png", chessStep, true},
                                {"3_x-20mm.png", 2 * chessStep, false}};

        for (int i = 0; i < 3 && !cancelled; ++i) {
            if (gh->is_canceling()) { cancelled = true; break; }
            if (kShots[i].preMoveMm > 0.0) {
                feedback(0, i, 3, QStringLiteral("chessboard %1/3 - X").arg(i + 1), {});
                if (kShots[i].right) ctx_.axis->x_right(kShots[i].preMoveMm, "low");
                else ctx_.axis->x_left(kShots[i].preMoveMm, "low");
                xOffsetMm += kShots[i].right ? kShots[i].preMoveMm : -kShots[i].preMoveMm;
                if (!waitAxisIdle("x", kAxisMaxMs, kAxisSettleMs, gh)) { cancelled = true; break; }
            }
            const QByteArray bytes =
                captureTo(camDir + "/" + QString::fromLatin1(kShots[i].file));
            feedback(0, i + 1, 3, QStringLiteral("chessboard %1/3").arg(i + 1), bytes);
        }

        // Put X back where the operator left it. Every round's shots have to be
        // taken from the same place, or the calibration runs displaced.
        if (!cancelled) {
            feedback(0, 3, 3, "X back", {});
            ctx_.axis->x_right(chessStep, "low");
            xOffsetMm += chessStep;
            waitAxisIdle("x", kAxisMaxMs, kAxisSettleMs, gh);
        }
    }

    // ---- the rounds ---------------------------------------------------------

    CaliTableData table;
    const QJsonObject fields =
        QJsonDocument::fromJson(QByteArray::fromStdString(goal->fields_json)).object();
    for (auto it = fields.begin(); it != fields.end(); ++it)
        table.setField(it.key(), it.value().toString());

    QStringList pct;
    for (const std::string &p : goal->pct_values) pct << QString::fromStdString(p);

    for (int r = 0; r < rounds.size() && !cancelled; ++r) {
        const QString tag = rounds.at(r);
        const QString dir = root + "/" + tag;
        const int total = shots * 2 + 1;
        QByteArray lastPos, lastNeg;

        for (int polarity = 0; polarity < 2 && !cancelled; ++polarity) {
            const bool positive = polarity == 0;
            if (!pushRoundPatterns(
                    QString::fromStdString(positive ? goal->spec_top_positive
                                                    : goal->spec_top_negative),
                    QString::fromStdString(positive ? goal->spec_side_positive
                                                    : goal->spec_side_negative))) {
                result->success = false;
                result->message = "a panel refused the pattern -- run stopped rather than "
                                  "shooting against the previous one";
                gh->abort(result);
                return;
            }
            QThread::msleep(kShotSettleMs);

            for (int s = 0; s < shots; ++s) {
                if (gh->is_canceling()) { cancelled = true; break; }
                const QString name = positive ? QStringLiteral("capture_positive_shot.png")
                                              : QStringLiteral("capture_negative_shot.png");
                const QByteArray bytes = captureTo(dir + "/" + name);
                if (positive) lastPos = bytes; else lastNeg = bytes;

                feedback(r, polarity * shots + s + 1, total,
                         QStringLiteral("[%1] %2 %3/%4")
                             .arg(tag, positive ? "Pos" : "Neg")
                             .arg(s + 1)
                             .arg(shots),
                         bytes);
                // The interval exists so the panel and the stage settle between
                // shots; the last shot of a polarity does not need to wait for a
                // shot that is not coming.
                if (s + 1 < shots) QThread::msleep(static_cast<unsigned long>(gapS * 1000));
            }
        }
        if (cancelled) break;

        // ---- the round's numbers -------------------------------------------
        //
        // Detect the eight directions from this round's positive/negative pair and
        // write them into the round's table. This is the step that used to send two
        // 3040x3040 frames to the client and get a table back.
        //
        // Noise cleaning is deliberately NOT forced on here. It used to be, which
        // meant an automated run silently discarded nodes the Clean Noise button
        // said it was keeping: the spacing filter drops a crossing whose gap is
        // unusually small, and a real ring can trip that. Whether to clean is the
        // operator's call.
        feedback(r, total, total, QStringLiteral("[%1] computing").arg(tag), {});

        if (!lastPos.isEmpty() && !lastNeg.isEmpty()) {
            const cv::Mat pb(1, lastPos.size(), CV_8U, const_cast<char *>(lastPos.constData()));
            const cv::Mat nb(1, lastNeg.size(), CV_8U, const_cast<char *>(lastNeg.constData()));
            std::vector<cv::Mat> imgs{cv::imdecode(pb, cv::IMREAD_COLOR),
                                      cv::imdecode(nb, cv::IMREAD_COLOR)};
            QString err;
            std::lock_guard<std::mutex> lock(ctx_.computeMutex);
            const QString nodes = ComputeOps::runDetectOp(
                ComputeOps::detect::kNodes8Dir, imgs,
                QStringLiteral("{\"noise_cleaning\": false}"), &err);

            if (!nodes.isEmpty()) {
                const int roundIndex = tag.toInt();
                table.addRound(roundIndex);
                QJsonObject params;
                params["table_index"] = roundIndex;
                params["pct"] = QJsonArray::fromStringList(pct);
                params["ict8"] =
                    QJsonDocument::fromJson(nodes.toUtf8()).object().value("directions");
                QString out;
                ComputeOps::runCaliOp(ComputeOps::cali::kUpdateTableFromCapture, table,
                                      QString::fromUtf8(QJsonDocument(params).toJson(
                                          QJsonDocument::Compact)),
                                      &out, &err);
                ComputeOps::runCaliOp(ComputeOps::cali::kComputeAll, table, QStringLiteral("{}"),
                                      &out, &err);
            } else {
                RCLCPP_WARN(get_logger(), "[%s] node detection failed: %s", qPrintable(tag),
                            qPrintable(err));
            }
        }

        // Save this round's table beside its captures, in the format the Cali
        // Result window reads back.
        XlsxIO::Grid grid;
        for (int row = 0; row < CaliTableData::kRows; ++row) {
            QVector<QString> line;
            for (int col = 0; col < CaliTableData::kCols; ++col)
                line.push_back(table.cell(tag.toInt(), row, col));
            grid.push_back(line);
        }
        XlsxIO::write(dir + "/moil_cali_result.xlsx", grid);
        result->completed.push_back(tag.toStdString());

        // Next Z position.
        if (r + 1 < rounds.size()) {
            feedback(r + 1, 0, total, QStringLiteral("[%1] Z back").arg(rounds.at(r + 1)), {});
            ctx_.axis->z_back(zStep, "low");
            if (!waitAxisIdle("z", kAxisMaxMs, kZSettleMs, gh)) { cancelled = true; break; }
        }
    }

    // ---- ending -------------------------------------------------------------
    //
    // On abort, the X excursion from the chessboard step is undone because it was
    // only ever meant to be temporary. Z is NOT put back: those moves are the
    // run's actual progress, and driving the rig back through them on a cancel
    // would be a motion the operator never asked for.
    if (cancelled && std::abs(xOffsetMm) > 1e-6) {
        if (xOffsetMm > 0) ctx_.axis->x_left(std::abs(xOffsetMm), "low");
        else ctx_.axis->x_right(std::abs(xOffsetMm), "low");
        waitAxisIdle("x", kAxisMaxMs, kAxisSettleMs, gh);
    }

    QString pose;
    for (const char *a : {"x", "y", "z", "yaw", "pitch"})
        pose += QStringLiteral("%1=%2 ").arg(a, ctx_.axis->read_position(QString::fromLatin1(a)));

    result->run_path = root.toStdString();
    // Every round the run got through, in one document. Sent on a cancel too:
    // the rounds already completed are real results, and the operator should see
    // the ones that finished rather than an empty window.
    result->table_json = table.toJson().toStdString();
    result->cancelled = cancelled;
    result->success = !cancelled;
    result->final_pose = pose.trimmed().toStdString();
    result->message = cancelled ? "stopped by the operator" : "";

    if (cancelled) gh->canceled(result);
    else gh->succeed(result);
}
