#include "session_node.h"

#include <thread>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>

#include "CaliTableData.h"
#include "server_context.h"

SessionNode::SessionNode(ServerContext &ctx, const rclcpp::NodeOptions &options)
    : ServerNode("moil_session", ctx, options) {
    // TRANSIENT_LOCAL: a client that connects after an edit still receives the
    // last event, so it knows to fetch rather than sitting on a stale table until
    // somebody happens to change something again.
    pubEvent_ = create_publisher<moil_interfaces::msg::SessionEvent>(
        "/session/events", rclcpp::QoS(rclcpp::KeepLast(10)).transient_local());

    addService<Command>("/session/command", &SessionNode::onCommand);
    addService<Edit>("/session/edit", &SessionNode::onEdit);
    addService<State>("/session/state", &SessionNode::onState);
    addService<CaptureSrv>("/session/capture", &SessionNode::onCapture);
    addService<ImageSrv>("/session/image", &SessionNode::onImage);

    addDetachedAction<RunCompute>("/session/run_compute", &SessionNode::executeRunCompute);

    // Every mutation the store makes, whoever caused it, becomes an event. Wired
    // to the store's own signal rather than emitted from each handler, so a path
    // that mutates without going through a handler cannot forget to announce it.
    QObject::connect(ctx_.sessions.get(), &SessionStore::sessionChanged,
                     [this](const QString &id, quint64 v, const QString &kind,
                            const QString &detail) { publishEvent(id, v, kind, detail); });

    RCLCPP_INFO(get_logger(), "session store: %s", qPrintable(ctx_.sessions->sessionsDir()));
}

void SessionNode::publishEvent(const QString &sessionId, quint64 version, const QString &kind,
                               const QString &detail) {
    moil_interfaces::msg::SessionEvent m;
    m.header.stamp = now();
    m.session_id = sessionId.toStdString();
    m.version = version;
    m.kind = kind.toStdString();
    m.detail = detail.toStdString();
    pubEvent_->publish(m);
}

void SessionNode::onCommand(const Command::Request &req, Command::Response &res) {
    const QString cmd = QString::fromStdString(req.command).trimmed().toLower();
    const QString id = QString::fromStdString(req.session_id);
    const QString payload = QString::fromStdString(req.payload);
    SessionStore &s = *ctx_.sessions;
    QString err;

    if (cmd == "list") {
        QJsonArray arr;
        for (const SessionStore::Summary &sum : s.list(&err)) {
            QJsonObject o;
            o["id"] = sum.id;
            o["name"] = sum.name;
            o["created_at"] = static_cast<double>(sum.createdAt);
            o["version"] = static_cast<double>(sum.version);
            QJsonArray caps;
            for (const QString &c : sum.captures) caps.append(c);
            o["captures"] = caps;
            arr.append(o);
        }
        res.result =
            QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)).toStdString();
        res.success = err.isEmpty();
    } else if (cmd == "create") {
        res.success = s.create(payload, &err);
    } else if (cmd == "open") {
        res.success = s.open(id, &err);
    } else if (cmd == "close") {
        res.success = s.close(&err);
    } else if (cmd == "rename") {
        res.success = s.rename(payload, &err);
    } else if (cmd == "save") {
        res.success = s.save(&err);
    } else if (cmd == "remove") {
        // Irreversible, and the server does not ask. The client must already have.
        res.success = s.remove(id, &err);
    } else if (cmd == "undo") {
        res.success = s.undo(&err);
    } else if (cmd == "redo") {
        res.success = s.redo(&err);
    } else {
        res.success = false;
        err = QStringLiteral("unknown command: %1").arg(cmd);
    }

    if (res.result.empty()) res.result = "{}";
    res.session_id = s.sessionId().toStdString();
    res.version = s.mirrorVersion();
    res.message = err.toStdString();
}

void SessionNode::onEdit(const Edit::Request &req, Edit::Response &res) {
    const QJsonObject o =
        QJsonDocument::fromJson(QByteArray::fromStdString(req.edits)).object();

    QVector<SessionStore::CellEdit> cells;
    for (const QJsonValue &v : o.value(QStringLiteral("cells")).toArray()) {
        const QJsonObject c = v.toObject();
        SessionStore::CellEdit e;
        e.round = c.value("round").toInt();
        e.row = c.value("row").toInt();
        e.col = c.value("col").toInt();
        e.text = c.value("text").toString();
        cells.push_back(e);
    }

    QHash<QString, QString> fields;
    const QJsonObject f = o.value(QStringLiteral("fields")).toObject();
    for (auto it = f.begin(); it != f.end(); ++it) fields.insert(it.key(), it.value().toString());

    // One call is one undo step -- see SessionEdit.srv. The batching is the
    // client's responsibility and this is where it pays off.
    QString err;
    res.success = ctx_.sessions->applyEdits(cells, fields, &err);
    res.session_id = ctx_.sessions->sessionId().toStdString();
    res.version = ctx_.sessions->mirrorVersion();
    res.message = err.toStdString();
}

void SessionNode::onState(const State::Request &req, State::Response &res) {
    const quint64 version = ctx_.sessions->mirrorVersion();
    res.version = version;
    res.success = true;

    if (req.since_version == version && version != 0) {
        // Nothing moved. The table is large; not sending it is the point of the
        // version field.
        res.changed = false;
        res.message = "";
        return;
    }
    res.changed = true;
    res.state_json = ctx_.sessions->mirror().toJson().toStdString();
    res.message = "";
}

void SessionNode::onCapture(const CaptureSrv::Request &req, CaptureSrv::Response &res) {
    QString err;
    const QString slot = QString::fromStdString(req.slot);
    const double timeout = req.timeout > 0.0 ? req.timeout : 5.0;

    if (!ctx_.sessions->capture(slot, timeout, &err)) {
        res.success = false;
        res.message = err.toStdString();
        return;
    }

    // Persist immediately. A capture goes into the session in MEMORY, and without
    // this it stays there until something calls save -- so a server restart, or a
    // crash, would lose a measurement the operator watched being taken and has
    // every reason to think is safe.
    //
    // The v2.0 flow wrote the PNG to disk the moment the shutter closed, and a
    // capture should not become less durable for having moved to the rig. Edits
    // are different and are still saved explicitly: an edit is something you might
    // undo, a shot is not.
    QString saveErr;
    if (!ctx_.sessions->save(&saveErr))
        RCLCPP_WARN(get_logger(),
                    "capture stored in memory but the session did not save: %s",
                    qPrintable(saveErr));
    // Report the stored size without shipping the pixels: the client asked to
    // take a picture, not to look at one. captureBytes rather than captureImage
    // because only the size is wanted -- decoding a 3040x3040 frame to read two
    // numbers is a second of work per shot for nothing.
    QSize stored;
    ctx_.sessions->captureBytes(slot, 0, &stored, &err);
    res.success = true;
    res.session_id = ctx_.sessions->sessionId().toStdString();
    res.version = ctx_.sessions->mirrorVersion();
    res.width = stored.width();
    res.height = stored.height();
    res.message = "";
}

void SessionNode::onImage(const ImageSrv::Request &req, ImageSrv::Response &res) {
    QString err;
    QSize original;
    // THE BYTES GO OUT AS THEY WERE STORED when max_side is 0. That is the whole
    // of "send the original": the store hands back the shutter's own encoding,
    // this fills the response with it, and nothing between the two decodes.
    //
    // It used to decode to a QImage and re-encode to PNG on every reply, max_side
    // or not. For a preview that only wasted a second; for max_side 0 it was the
    // reason the original never arrived -- a 3040x3040 re-encode turned the
    // ~1.5 MB stored JPEG into ~16 MB, which this service does not carry, so the
    // call hung with no error on either side and the client quietly settled for a
    // 2048 px copy. The picture the operator's "save to this PC" toggle wrote was
    // therefore never the frame the rig measures.
    //
    // A max_side that really is smaller still costs an encode, in the store --
    // but that one is encoding the small image, which is the point of asking.
    const QByteArray bytes = ctx_.sessions->captureBytes(QString::fromStdString(req.slot),
                                                         req.max_side, &original, &err);
    if (bytes.isEmpty()) {
        res.success = false;
        res.message = err.toStdString();
        return;
    }

    res.image.header.stamp = now();
    // What the bytes ACTUALLY are, read from the header rather than assumed. The
    // stored encoding follows the camera's capture_format, so a rig configured
    // for PNG and one for JPEG both answer honestly and the client writes the
    // right extension either way.
    res.image.format = bytes.startsWith("\x89PNG") ? "png" : "jpeg";
    res.image.data.assign(bytes.begin(), bytes.end());
    // The ORIGINAL size, which is not the size of the image above when max_side
    // shrank it -- the zoom viewer needs to know what it has a piece of. Reported
    // by the store, which already knows it; re-decoding the full frame a second
    // time just to measure it was the other half of the old cost.
    res.width = original.width();
    res.height = original.height();
    res.success = true;
    res.message = "";

    // Said once per fetch, because the size of the reply is the thing that decides
    // whether it arrives. If an original ever stops coming through, this line is
    // where the number that broke it is written down.
    RCLCPP_INFO(get_logger(), "image \"%s\": %dx%d stored, sending %s %.2f MB (max_side=%d)",
                req.slot.c_str(), original.width(), original.height(),
                res.image.format.c_str(), double(bytes.size()) / (1024.0 * 1024.0),
                req.max_side);
}

void SessionNode::executeRunCompute(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<RunCompute>> gh) {
    const auto goal = gh->get_goal();
    auto result = std::make_shared<RunCompute::Result>();

    const SessionStore::ProgressFn progress = [gh](int done, int total, const QString &stage) {
        auto fb = std::make_shared<RunCompute::Feedback>();
        fb->done = done;
        fb->total = total;
        fb->stage = stage.toStdString();
        gh->publish_feedback(fb);
        return !gh->is_canceling();
    };

    QString out, err;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lock(ctx_.computeMutex);
        ok = ctx_.sessions->runCompute(QString::fromStdString(goal->op),
                                       QString::fromStdString(goal->params), &out, &err, progress);
    }

    result->session_id = ctx_.sessions->sessionId().toStdString();
    result->version = ctx_.sessions->mirrorVersion();
    result->result = out.isEmpty() ? "{}" : out.toStdString();
    result->cancelled = gh->is_canceling();
    result->success = ok || result->cancelled;
    result->message = err.toStdString();

    // The table is not returned. The SessionEvent the store just published has
    // already told every client -- including this one -- to fetch it.
    if (result->cancelled) gh->canceled(result);
    else gh->succeed(result);
}
