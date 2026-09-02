#include "CaliTableData.h"

#include <QJsonDocument>
#include <QJsonObject>

void CaliTableData::addRound(int round, int rows, int cols) {
    if (!validRound(round)) return;
    Round &r = rounds_[round];
    r.present = true;
    r.rows = rows;
    r.cols = cols;
}

bool CaliTableData::hasRound(int round) const {
    return validRound(round) && rounds_[round].present;
}

int CaliTableData::rowCount(int round) const {
    return hasRound(round) ? rounds_[round].rows : 0;
}

int CaliTableData::columnCount(int round) const {
    return hasRound(round) ? rounds_[round].cols : 0;
}

QString CaliTableData::cell(int round, int row, int col) const {
    if (!hasRound(round)) return {};
    const Round &r = rounds_[round];
    if (row < 0 || row >= r.rows || col < 0 || col >= r.cols) return {};
    const auto it = r.cells.constFind(row);
    return it == r.cells.constEnd() ? QString() : it->value(col);
}

void CaliTableData::setCell(int round, int row, int col, const QString &text) {
    if (!hasRound(round)) return;
    Round &r = rounds_[round];
    if (row < 0 || row >= r.rows || col < 0 || col >= r.cols) return;
    // Drop empties instead of storing them: a cleared cell and a never-written
    // cell must read back the same, and keeping them would grow the JSON by the
    // ~15k blanks a full clearColumn sweep touches.
    if (text.isEmpty()) {
        const auto it = r.cells.find(row);
        if (it != r.cells.end()) {
            it->remove(col);
            if (it->isEmpty()) r.cells.erase(it);
        }
        return;
    }
    r.cells[row][col] = text;
}

double CaliTableData::field(const QString &name, double def) const {
    const auto it = fields_.constFind(name);
    if (it == fields_.constEnd()) return def;
    bool ok = false;
    const double v = it->trimmed().toDouble(&ok);
    return ok ? v : def;
}

// ---------------------------------------------------------------- wire format

QString CaliTableData::toJson() const {
    QJsonObject roundsObj;
    for (int i = 0; i <= 10; ++i) {
        const Round &r = rounds_[i];
        if (!r.present) continue;
        QJsonObject cellsObj;
        for (auto rowIt = r.cells.constBegin(); rowIt != r.cells.constEnd(); ++rowIt) {
            QJsonObject colsObj;
            for (auto colIt = rowIt->constBegin(); colIt != rowIt->constEnd(); ++colIt)
                colsObj[QString::number(colIt.key())] = colIt.value();
            if (!colsObj.isEmpty()) cellsObj[QString::number(rowIt.key())] = colsObj;
        }
        QJsonObject ro;
        ro["rows"] = r.rows;
        ro["cols"] = r.cols;
        ro["cells"] = cellsObj;
        roundsObj[QString::number(i)] = ro;
    }

    QJsonObject fieldsObj;
    for (auto it = fields_.constBegin(); it != fields_.constEnd(); ++it)
        fieldsObj[it.key()] = it.value();

    QJsonObject root;
    root["rounds"] = roundsObj;
    root["fields"] = fieldsObj;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

CaliTableData CaliTableData::fromJson(const QString &json, bool *ok) {
    CaliTableData out;
    if (ok) *ok = false;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return out;
    const QJsonObject root = doc.object();

    const QJsonObject roundsObj = root.value("rounds").toObject();
    for (auto it = roundsObj.constBegin(); it != roundsObj.constEnd(); ++it) {
        bool idxOk = false;
        const int idx = it.key().toInt(&idxOk);
        if (!idxOk || !validRound(idx)) continue;
        const QJsonObject ro = it->toObject();
        out.addRound(idx, ro.value("rows").toInt(kRows), ro.value("cols").toInt(kCols));

        const QJsonObject cellsObj = ro.value("cells").toObject();
        for (auto rowIt = cellsObj.constBegin(); rowIt != cellsObj.constEnd(); ++rowIt) {
            bool rowOk = false;
            const int row = rowIt.key().toInt(&rowOk);
            if (!rowOk) continue;
            const QJsonObject colsObj = rowIt->toObject();
            for (auto colIt = colsObj.constBegin(); colIt != colsObj.constEnd(); ++colIt) {
                bool colOk = false;
                const int col = colIt.key().toInt(&colOk);
                if (!colOk) continue;
                // toString() is empty for a JSON number, which would silently
                // blank the cell -- accept both so a hand-written or
                // number-emitting client still round-trips.
                const QJsonValue v = colIt.value();
                out.setCell(idx, row, col,
                            v.isString() ? v.toString()
                                         : (v.isDouble() ? QString::number(v.toDouble(), 'g', 15)
                                                         : QString()));
            }
        }
    }

    const QJsonObject fieldsObj = root.value("fields").toObject();
    for (auto it = fieldsObj.constBegin(); it != fieldsObj.constEnd(); ++it) {
        const QJsonValue v = it.value();
        out.setField(it.key(), v.isString() ? v.toString()
                                            : (v.isDouble() ? QString::number(v.toDouble(), 'g', 15)
                                                            : QString()));
    }

    if (ok) *ok = true;
    return out;
}
