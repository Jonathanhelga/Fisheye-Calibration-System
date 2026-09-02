#include "moil_pattern_gennerator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <QJsonArray>
#include <QString>
#include <opencv2/imgproc.hpp>

// ==========================================================================
//  NEW rendering engine (struct-based) — unchanged port.
// ==========================================================================
namespace {
cv::Scalar bgrOf(const std::array<int, 3> &rgb) {
    return cv::Scalar(rgb[2], rgb[1], rgb[0]);  // RGB -> BGR
}
}  // namespace

namespace PatternGen {

cv::Mat drawCrossline(const cv::Mat &img) {
    cv::Mat out = img.clone();
    const int h = out.rows, w = out.cols;
    const int thickness = std::max(1, std::max(h, w) / 300);
    const cv::Scalar black(0, 0, 0);
    cv::line(out, {0, h / 2}, {w - 1, h / 2}, black, thickness, cv::LINE_8);
    cv::line(out, {w / 2, 0}, {w / 2, h - 1}, black, thickness, cv::LINE_8);
    return out;
}

cv::Mat renderConcentric(int h, int w, const std::vector<ConcentricLayer> &layers, bool crossline) {
    cv::Mat img(h, w, CV_8UC3, cv::Scalar(255, 255, 255));

    int radiusSum = 0;
    for (const auto &L : layers) radiusSum += L.radius;

    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        const ConcentricLayer &L = layers[i];
        const cv::Scalar bgr = bgrOf(L.rgb);
        if (L.shape == "square") {
            cv::rectangle(img, {w / 2 - radiusSum + L.cx, h / 2 - radiusSum + L.cy},
                          {w / 2 + radiusSum + L.cx, h / 2 + radiusSum + L.cy}, bgr, -1);
        } else {
            cv::circle(img, {w / 2 + L.cx, h / 2 + L.cy}, radiusSum, bgr, -1);
        }
        radiusSum -= L.radius;
    }
    return crossline ? drawCrossline(img) : img;
}

cv::Mat renderStripeline(int h, int w, const std::vector<StripelineLayer> &layers, bool crossline) {
    cv::Mat img(h, w, CV_8UC3, cv::Scalar(0, 0, 0));

    int y = 0;
    cv::Scalar last(0, 0, 0);
    for (const auto &L : layers) {
        const cv::Scalar bgr = bgrOf(L.rgb);
        const int y2 = std::min(h, y + L.interval);
        if (y2 > y) {
            img(cv::Range(y, y2), cv::Range::all()).setTo(bgr);
            last = bgr;
            y = y2;
        }
        if (y >= h) break;
    }
    if (y < h) img(cv::Range(y, h), cv::Range::all()).setTo(last);

    return crossline ? drawCrossline(img) : img;
}

cv::Mat renderChessboard(int width, int height, int squarePx, std::array<int, 3> fgRgb,
                         std::array<int, 3> bgRgb) {
    if (width < 1 || height < 1 || squarePx < 1) return {};
    if (1LL * width * height > 120'000'000LL) return {};

    cv::Mat img(height, width, CV_8UC3, bgrOf(bgRgb));
    const cv::Scalar fg = bgrOf(fgRgb);
    const int cx = width / 2, cy = height / 2;
    const int nCols = width / squarePx + 2;
    const int nRows = height / squarePx + 2;
    const cv::Rect canvas(0, 0, width, height);
    for (int row = -nRows; row <= nRows; ++row)
        for (int col = -nCols; col <= nCols; ++col) {
            if (((row + col) & 1) == 0) continue;
            const cv::Rect r =
                cv::Rect(cx + col * squarePx, cy + row * squarePx, squarePx, squarePx) & canvas;
            if (r.width > 0 && r.height > 0) img(r).setTo(fg);
        }
    return img;
}

}  // namespace PatternGen

// ==========================================================================
//  MoilCaliPatternGenerator — exact Python API on QJsonObject.
// ==========================================================================
namespace {
QJsonArray rgbArray(const std::array<int, 3> &c) {
    return QJsonArray{c[0], c[1], c[2]};
}
QJsonObject concentricLayerObj() {
    return QJsonObject{{"shape", "circle"},
                       {"radius", 0},
                       {"color", QJsonArray{255, 255, 255}},
                       {"cx", 0},
                       {"cy", 0}};
}
QJsonObject stripLayerObj() {
    return QJsonObject{{"interval", 100}, {"color", QJsonArray{255, 255, 255}}};
}
}  // namespace

MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::init_json_concentric(int height,
                                                                              int width) {
    Json j{{"pattern type", "concentric"}, {"height", 0},          {"width", 0},
           {"crossline", false},           {"pos_neg_color", true}, {"positive color", QJsonArray{0, 0, 0}},
           {"negative color", QJsonArray{0, 0, 0}}};
    for (int i = 1; i <= 25; ++i) j[QString::number(i)] = concentricLayerObj();
    j = set_height(j, height);
    j = set_width(j, width);
    return j;
}

MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::init_json_stripline(int height, int width) {
    Json j{{"pattern type", "stripeline"}, {"height", 0},          {"width", 0},
           {"crossline", false},           {"pos_neg_color", true}, {"positive color", QJsonArray{0, 0, 0}},
           {"negative color", QJsonArray{0, 0, 0}}};
    for (int i = 1; i <= 50; ++i) j[QString::number(i)] = stripLayerObj();
    j = set_height(j, height);
    j = set_width(j, width);
    return j;
}

MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::init_json_chessboard(int height,
                                                                              int width) {
    Json j{{"pattern type", "chessboard"},
           {"height", 1920},
           {"width", 1920},
           {"crossline", false},
           {"positive color", QJsonArray{0, 0, 0}},
           {"negative color", QJsonArray{255, 255, 255}},
           {"pixel size", 0.2478},
           {"grid height mm", 50},
           {"grid width mm", 50}};
    j = set_height(j, height);
    j = set_width(j, width);
    return j;
}

// ---- render ---------------------------------------------------------------
cv::Mat MoilCaliPatternGenerator::render_from_json(const Json &cfgIn) {
    Json cfg = cfgIn;
    const std::string ptype = detect_pattern_type(cfg);
    const bool use_pos_neg = cfg.value("pos_neg_color").toBool(false);

    cv::Mat img;
    if (ptype == "concentric") {
        if (use_pos_neg) cfg = apply_pos_neg_colors(cfg);
        img = _render_concentric(cfg);
    } else if (ptype == "stripeline") {
        if (use_pos_neg) cfg = apply_pos_neg_colors(cfg);
        img = _render_stripeline(cfg);
    } else if (ptype == "chessboard") {
        img = _render_chessboard(cfg);
    } else {
        throw std::runtime_error("Unsupported pattern type: " + ptype);
    }

    if (cfg.value("crossline").toBool(true)) img = _draw_crossline(img);
    return img;
}

cv::Mat MoilCaliPatternGenerator::_render_concentric(const Json &cfg) {
    const auto [h, w] = _get_hw(cfg);
    std::vector<PatternGen::ConcentricLayer> layers;
    for (auto &[idx, layer] : _iter_layers(cfg, 1, 25)) {
        PatternGen::ConcentricLayer L;
        L.shape = layer.value("shape").toString("circle").toLower().toStdString();
        L.radius = layer.value("radius").toInt(0);
        L.rgb = jsonToRgb(layer.value("color"), {255, 255, 255});
        L.cx = layer.value("cx").toInt(0);
        L.cy = layer.value("cy").toInt(0);
        layers.push_back(L);
    }
    return PatternGen::renderConcentric(h, w, layers, false);
}

cv::Mat MoilCaliPatternGenerator::_render_stripeline(const Json &cfg) {
    const auto [h, w] = _get_hw(cfg);
    const bool use_posneg = cfg.value("pos_neg_color").toBool(false);
    const auto pos_rgb = use_posneg ? _valid_rgb(cfg.value("positive color")) : std::nullopt;
    const auto neg_rgb = use_posneg ? _valid_rgb(cfg.value("negative color")) : std::nullopt;

    std::vector<PatternGen::StripelineLayer> layers;
    for (auto &[idx, layer] : _iter_layers(cfg, 1, 50)) {
        PatternGen::StripelineLayer L;
        L.interval = layer.value("interval").toInt(0);
        std::array<int, 3> rgb = jsonToRgb(layer.value("color"), {255, 255, 255});
        if (use_posneg) {
            if ((idx % 2 == 1) && pos_rgb) rgb = *pos_rgb;
            else if ((idx % 2 == 0) && neg_rgb) rgb = *neg_rgb;
        }
        L.rgb = rgb;
        layers.push_back(L);
    }
    return PatternGen::renderStripeline(h, w, layers, false);
}

cv::Mat MoilCaliPatternGenerator::_render_chessboard(const Json &cfg) {
    const auto [h, w] = _get_hw(cfg);
    const double px_mm = cfg.value("pixel size").toDouble();
    // Read as doubles, not ints. QJsonValue::toInt() returns its default for a
    // non-integral number, so a 45.5 mm square arrived as 0 and the board was drawn
    // with 1 px cells. Whole-millimetre values are unaffected.
    const double cell_w_mm = cfg.value("grid width mm").toDouble();
    const double cell_h_mm = cfg.value("grid height mm").toDouble();
    const cv::Scalar pos_bgr = _rgb_to_bgr(jsonToRgb(cfg.value("positive color"), {0, 0, 0}));
    const cv::Scalar neg_bgr = _rgb_to_bgr(jsonToRgb(cfg.value("negative color"), {255, 255, 255}));

    const int cell_w = std::max(1, int(std::lround(cell_w_mm / px_mm)));
    const int cell_h = std::max(1, int(std::lround(cell_h_mm / px_mm)));

    cv::Mat img(h, w, CV_8UC3, cv::Scalar(128, 128, 128));
    const int cx = w / 2, cy = h / 2;
    const int cols = w / cell_w + 2;
    const int rows = h / cell_h + 2;
    for (int r = -rows; r <= rows; ++r)
        for (int c = -cols; c <= cols; ++c) {
            const cv::Scalar bgr = ((r + c) % 2 == 0) ? pos_bgr : neg_bgr;
            const int x1 = cx + c * cell_w, y1 = cy + r * cell_h;
            const cv::Rect rect = cv::Rect(x1, y1, cell_w, cell_h) & cv::Rect(0, 0, w, h);
            if (rect.width > 0 && rect.height > 0) img(rect).setTo(bgr);
        }
    return img;
}

cv::Mat MoilCaliPatternGenerator::_draw_crossline(const cv::Mat &base_image) {
    if (base_image.empty()) throw std::runtime_error("base_image is empty or invalid");
    return PatternGen::drawCrossline(base_image);
}

// ---- detect ---------------------------------------------------------------
std::string MoilCaliPatternGenerator::detect_pattern_type(const Json &config) {
    const auto normalize = [](QString k) {
        return k.trimmed().toLower().replace("_", " ");
    };
    QHash<QString, QString> norm2orig;
    for (auto it = config.begin(); it != config.end(); ++it) norm2orig.insert(normalize(it.key()), it.key());

    for (const QString cand : {"pattern type", "pattern-type", "patterntype", "pattern  type"}) {
        if (norm2orig.contains(cand)) {
            const QString v =
                config.value(norm2orig.value(cand)).toString().trimmed().toLower();
            if (v == "concentric" || v == "stripeline" || v == "chessboard") return v.toStdString();
        }
    }

    const bool has_pixel = norm2orig.contains("pixel size");
    const bool has_gh = norm2orig.contains("grid height mm");
    const bool has_gw = norm2orig.contains("grid width mm");
    const bool has_pos = norm2orig.contains("positive color");
    const bool has_neg = norm2orig.contains("negative color");
    if (has_pixel || ((has_gh || has_gw) && (has_pos || has_neg))) return "chessboard";

    int circle = 0, centerXy = 0, interval = 0, total = 0;
    for (auto it = config.begin(); it != config.end(); ++it) {
        bool isNum = false;
        it.key().toInt(&isNum);
        if (!isNum || !it.value().isObject()) continue;
        const QJsonObject item = it.value().toObject();
        ++total;
        if (item.value("shape").toString().trimmed().toLower() == "circle") ++circle;
        if (item.contains("cx") && item.contains("cy")) ++centerXy;
        if (item.contains("interval")) ++interval;
    }
    const auto reached = [](int c, int t) { return t > 0 && c >= std::max(1, t / 3); };
    if (reached(circle, total) && centerXy > 0) return "concentric";
    if (reached(interval, total)) return "stripeline";
    throw std::runtime_error("Unable to infer pattern type from the given JSON.");
}

// ---- check ----------------------------------------------------------------
namespace {
void assertRgb(const QJsonValue &v, const char *tag) {
    if (v.isNull()) return;
    if (!v.isArray() || v.toArray().size() != 3)
        throw std::runtime_error(std::string(tag) + " channel error: expected len=3");
    for (const auto &e : v.toArray()) {
        const int x = e.toInt(-1);
        if (x < 0 || x > 255)
            throw std::runtime_error(std::string(tag) + " value error: expected 0..255");
    }
}
}  // namespace

bool MoilCaliPatternGenerator::check_json_concentric(const Json &j) {
    static const QStringList tags = QStringList{"pattern type", "height", "width", "crossline",
                                                "pos_neg_color", "positive color", "negative color"};
    for (auto it = j.begin(); it != j.end(); ++it) {
        const QString tag = it.key();
        bool isNum = false;
        const int n = tag.toInt(&isNum);
        if (!(tags.contains(tag) || (isNum && n >= 1 && n <= 25))) return false;
        if (tag == "pattern type" && it.value().toString() != "concentric")
            throw std::runtime_error("pattern type error: expected concentric");
        if (tag == "positive color" || tag == "negative color") assertRgb(it.value(), "color");
        if (isNum) {
            const QJsonObject L = it.value().toObject();
            const QString shape = L.value("shape").toString();
            if (shape != "circle" && shape != "square")
                throw std::runtime_error("shape error");
            assertRgb(L.value("color"), "color");
        }
    }
    return true;
}

bool MoilCaliPatternGenerator::check_json_stripeline(const Json &j) {
    static const QStringList tags = QStringList{"pattern type", "height", "width", "crossline",
                                                "pos_neg_color", "positive color", "negative color"};
    for (auto it = j.begin(); it != j.end(); ++it) {
        const QString tag = it.key();
        bool isNum = false;
        const int n = tag.toInt(&isNum);
        if (!(tags.contains(tag) || (isNum && n >= 1 && n <= 50))) return false;
        if (tag == "pattern type" && it.value().toString() != "stripeline")
            throw std::runtime_error("pattern type error: expected stripeline");
        if (tag == "positive color" || tag == "negative color") assertRgb(it.value(), "color");
        if (isNum) assertRgb(it.value().toObject().value("color"), "color");
    }
    return true;
}

bool MoilCaliPatternGenerator::check_json_chessboard(const Json &j) {
    static const QStringList required = QStringList{"pattern type",    "height",
                                                    "width",           "crossline",
                                                    "pixel size",      "grid height mm",
                                                    "grid width mm",   "positive color",
                                                    "negative color"};
    for (const QString &k : required)
        if (!j.contains(k)) throw std::runtime_error("Missing required key: " + k.toStdString());
    if (j.value("pattern type").toString() != "chessboard")
        throw std::runtime_error("pattern type error: expected chessboard");
    if (j.value("pixel size").toDouble() <= 0)
        throw std::runtime_error("pixel size error: expected > 0");
    assertRgb(j.value("positive color"), "positive color");
    assertRgb(j.value("negative color"), "negative color");
    return true;
}

// ---- universal setters ----------------------------------------------------
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_height(Json j, int height) {
    if (height <= 0) throw std::runtime_error("height must be positive int");
    j["height"] = height;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_width(Json j, int width) {
    if (width <= 0) throw std::runtime_error("width must be positive int");
    j["width"] = width;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_crossline(Json j, bool crossline) {
    j["crossline"] = crossline;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_positive_color(
    Json j, const std::optional<std::array<int, 3>> &color) {
    if (!color) j["positive color"] = QJsonValue(QJsonValue::Null);
    else j["positive color"] = rgbArray(*color);
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_negative_color(
    Json j, const std::optional<std::array<int, 3>> &color) {
    if (!color) j["negative color"] = QJsonValue(QJsonValue::Null);
    else j["negative color"] = rgbArray(*color);
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_pos_neg_color(Json j, bool enable) {
    j["pos_neg_color"] = enable;
    return j;
}

// ---- concentric setters ---------------------------------------------------
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_concentric_shape(
    Json j, int layer, const std::string &shape) {
    if (detect_pattern_type(j) != "concentric")
        throw std::runtime_error("Pattern Type is not Concentric");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["shape"] = QString::fromStdString(shape);
    j[QString::number(layer)] = L;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_concentric_radius(Json j, int layer,
                                                                               int radius) {
    if (detect_pattern_type(j) != "concentric")
        throw std::runtime_error("Pattern Type is not Concentric");
    if (radius < 0) throw std::runtime_error("radius must be non-negative int");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["radius"] = radius;
    j[QString::number(layer)] = L;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_concentric_color(
    Json j, int layer, std::array<int, 3> color) {
    if (detect_pattern_type(j) != "concentric")
        throw std::runtime_error("Pattern Type is not Concentric");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["color"] = rgbArray(color);
    j[QString::number(layer)] = L;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_concentric_cx(Json j, int layer,
                                                                           int cx) {
    if (detect_pattern_type(j) != "concentric")
        throw std::runtime_error("Pattern Type is not Concentric");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["cx"] = cx;
    j[QString::number(layer)] = L;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_concentric_cy(Json j, int layer,
                                                                           int cy) {
    if (detect_pattern_type(j) != "concentric")
        throw std::runtime_error("Pattern Type is not Concentric");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["cy"] = cy;
    j[QString::number(layer)] = L;
    return j;
}

// ---- stripeline setters ---------------------------------------------------
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_stripeline_interval(Json j, int layer,
                                                                                 int interval) {
    if (detect_pattern_type(j) != "stripeline")
        throw std::runtime_error("Pattern Type is not StripLine");
    if (interval <= 0) throw std::runtime_error("interval must be positive int");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["interval"] = interval;
    j[QString::number(layer)] = L;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_stripeline_color(
    Json j, int layer, std::array<int, 3> color) {
    if (detect_pattern_type(j) != "stripeline")
        throw std::runtime_error("Pattern Type is not StripLine");
    QJsonObject L = j.value(QString::number(layer)).toObject();
    L["color"] = rgbArray(color);
    j[QString::number(layer)] = L;
    return j;
}

// ---- chessboard setters ---------------------------------------------------
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_chessboard_pixel_size(
    Json j, double pixel_size) {
    if (detect_pattern_type(j) != "chessboard")
        throw std::runtime_error("Pattern Type is not Chessboard");
    if (pixel_size <= 0) throw std::runtime_error("pixel_size must be positive");
    j["pixel size"] = pixel_size;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_chessboard_grid_h_mm(Json j,
                                                                                  int grid_h_mm) {
    if (detect_pattern_type(j) != "chessboard")
        throw std::runtime_error("Pattern Type is not Chessboard");
    if (grid_h_mm <= 0) throw std::runtime_error("grid_h_mm must be positive int");
    j["grid height mm"] = grid_h_mm;
    return j;
}
MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::set_chessboard_grid_w_mm(Json j,
                                                                                  int grid_w_mm) {
    if (detect_pattern_type(j) != "chessboard")
        throw std::runtime_error("Pattern Type is not Chessboard");
    if (grid_w_mm <= 0) throw std::runtime_error("grid_w_mm must be positive int");
    j["grid width mm"] = grid_w_mm;
    return j;
}

MoilCaliPatternGenerator::Json MoilCaliPatternGenerator::apply_pos_neg_colors(Json j) {
    if (j.value("pattern type").toString().toLower() != "concentric") return j;
    const auto pos = _valid_rgb(j.value("positive color"));
    const auto neg = _valid_rgb(j.value("negative color"));
    for (int i = 1; i <= 25; ++i) {
        const QString k = QString::number(i);
        if (!j.value(k).isObject()) continue;
        QJsonObject L = j.value(k).toObject();
        if ((i % 2 == 1) && pos) L["color"] = rgbArray(*pos);
        else if ((i % 2 == 0) && neg) L["color"] = rgbArray(*neg);
        j[k] = L;
    }
    return j;
}

// ---- small tools ----------------------------------------------------------
cv::Scalar MoilCaliPatternGenerator::_rgb_to_bgr(const std::array<int, 3> &rgb) {
    return cv::Scalar(rgb[2], rgb[1], rgb[0]);
}
std::pair<int, int> MoilCaliPatternGenerator::_get_hw(const Json &cfg) {
    return {cfg.value("height").toInt(), cfg.value("width").toInt()};
}
std::vector<std::pair<int, QJsonObject>> MoilCaliPatternGenerator::_iter_layers(const Json &cfg,
                                                                                int start, int end) {
    std::vector<std::pair<int, QJsonObject>> out;
    for (int i = start; i <= end; ++i) {
        const QJsonValue v = cfg.value(QString::number(i));
        if (v.isObject()) out.emplace_back(i, v.toObject());
    }
    return out;
}
std::optional<std::array<int, 3>> MoilCaliPatternGenerator::_valid_rgb(const QJsonValue &v) {
    if (!v.isArray() || v.toArray().size() != 3) return std::nullopt;
    std::array<int, 3> out{};
    const QJsonArray a = v.toArray();
    for (int i = 0; i < 3; ++i) {
        const int x = a[i].toInt(-1);
        if (x < 0 || x > 255) return std::nullopt;
        out[i] = x;
    }
    return out;
}
std::array<int, 3> MoilCaliPatternGenerator::jsonToRgb(const QJsonValue &v,
                                                       std::array<int, 3> fallback) {
    if (const auto r = _valid_rgb(v)) return *r;
    return fallback;
}
