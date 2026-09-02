#include "ComputeOps.h"

#include <stdexcept>

#include <QJsonArray>
#include <QJsonObject>

#include "ComputeJson.h"
#include "moil_pattern_gennerator.h"

namespace ComputeOps {
namespace {

using namespace ComputeOps::json;

QJsonArray rgbArray(const QJsonValue &v, int r, int g, int b) {
    const QJsonArray a = v.toArray();
    if (a.size() == 3) return a;
    QJsonArray def;
    def.append(r);
    def.append(g);
    def.append(b);
    return def;
}

// Two JSON shapes for the same pattern are already in use:
//
//   * the pattern FILE, e.g. eizo_ev2730q_1920x1920_15circle.json, with layers
//     under the string keys "1", "2", ... -- what the Json button reads and
//     writes and what MoilCaliPatternGenerator::render_from_json expects;
//   * the SPEC of ShowPatternSpec.srv, with layers in a "layers" array and
//     snake_case colour keys.
//
// Rather than a second renderer for the spec, translate it into the file shape and
// render once. The two shapes then cannot disagree about what a pattern looks
// like, because only one of them is ever drawn.
QJsonObject specToPatternFile(const QJsonObject &spec) {
    const QString type = spec.value("type").toString().trimmed().toLower();
    QJsonObject out;
    out["pattern type"] = type;
    out["width"] = spec.value("width").toInt(0);
    out["height"] = spec.value("height").toInt(0);
    // render_from_json defaults crossline to TRUE when the key is absent, while the
    // spec's documented default is false. Write it explicitly so the absent case
    // cannot flip a crossline onto the glass.
    out["crossline"] = getBool(spec, "crossline", false);
    out["pos_neg_color"] = false;

    if (type == "chessboard") {
        // Sent as millimetres and left as millimetres: round(square_mm /
        // pixel_size_mm) is only right against the panel that displays it, and the
        // renderer is where the panel's pixel size is known.
        out["pixel size"] = getDouble(spec, "pixel_size_mm", 0.155);
        const double squareMm = getDouble(spec, "square_mm", 45.0);
        out["grid width mm"] = squareMm;
        out["grid height mm"] = squareMm;
        out["positive color"] = rgbArray(spec.value("fg_rgb"), 0, 0, 0);
        out["negative color"] = rgbArray(spec.value("bg_rgb"), 255, 255, 255);
        return out;
    }

    const QJsonArray layers = spec.value("layers").toArray();
    for (int i = 0; i < layers.size(); ++i) {
        const QJsonObject L = layers.at(i).toObject();
        QJsonObject o;
        if (type == "stripeline") {
            o["interval"] = L.value("interval").toInt(0);
        } else {
            o["shape"] = L.value("shape").toString("circle");
            o["radius"] = L.value("radius").toInt(0);
            o["cx"] = L.value("cx").toInt(0);
            o["cy"] = L.value("cy").toInt(0);
        }
        o["color"] = rgbArray(L.value("rgb"), 255, 255, 255);
        out[QString::number(i + 1)] = o;  // layers are keyed "1", "2", ... not indexed
    }
    return out;
}

// Which of the two shapes this is.
//
// The discriminator is the bare "type" key: the spec uses it, the pattern file
// spells the same thing "pattern type" (with a space) and otherwise carries only
// numbered layer keys. Testing for "layers" instead does not work -- a chessboard
// spec has no layers at all, so it would be handed to the file-format renderer,
// which cannot infer a type from {"type": "chessboard", "square_mm": ...} and
// rejects it.
bool looksLikeSpec(const QJsonObject &o) {
    return o.contains("type") || o.value("layers").isArray() || o.contains("square_mm") ||
           o.contains("pixel_size_mm");
}

}  // namespace

cv::Mat renderPattern(const QString &patternJson, int width, int height, QString *err) {
    const auto fail = [&](const QString &m) {
        if (err) *err = m;
        return cv::Mat();
    };

    bool ok = false;
    QJsonObject cfg = parseObject(patternJson, &ok);
    if (!ok || cfg.isEmpty()) return fail("pattern json is not a JSON object");
    if (looksLikeSpec(cfg)) cfg = specToPatternFile(cfg);

    // 0 means "keep what the pattern itself says". The override exists so the node
    // can draw at the panel's real resolution, which is the one thing the client
    // cannot know.
    if (width > 0) cfg["width"] = width;
    if (height > 0) cfg["height"] = height;
    if (cfg.value("width").toInt(0) <= 0 || cfg.value("height").toInt(0) <= 0)
        return fail("pattern has no usable width/height and none was supplied");

    try {
        cv::Mat img = MoilCaliPatternGenerator::render_from_json(cfg);
        if (img.empty()) return fail("renderer produced an empty image");
        return img;
    } catch (const std::exception &e) {
        return fail(QString::fromUtf8(e.what()));
    }
}

}  // namespace ComputeOps
