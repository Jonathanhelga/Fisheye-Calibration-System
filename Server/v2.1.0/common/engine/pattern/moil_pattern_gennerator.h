#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <QJsonObject>
#include <opencv2/core.hpp>

// Port of mvc_model/moil_pattern/moil_pattern_gennerator.py.
//
// Python keeps the pattern as a JSON dict and drives everything through the
// MoilCaliPatternGenerator class. That class is reproduced below 1:1 (snake_case
// method names) on top of QJsonObject. The struct-based PatternGen renderer that
// the C++ port grew is preserved as the NEW rendering engine the class delegates
// to.

// ---- NEW (no Python counterpart): struct-based rendering engine -----------
namespace PatternGen {

struct ConcentricLayer {
    std::string shape = "circle";  // "circle" or "square"
    int radius = 0;
    std::array<int, 3> rgb = {255, 255, 255};
    int cx = 0;
    int cy = 0;
};

struct StripelineLayer {
    int interval = 0;
    std::array<int, 3> rgb = {255, 255, 255};
};

cv::Mat drawCrossline(const cv::Mat &img);
cv::Mat renderConcentric(int height, int width, const std::vector<ConcentricLayer> &layers,
                         bool crossline);
cv::Mat renderStripeline(int height, int width, const std::vector<StripelineLayer> &layers,
                         bool crossline);
cv::Mat renderChessboard(int width, int height, int squarePx, std::array<int, 3> fgRgb,
                         std::array<int, 3> bgRgb);

}  // namespace PatternGen

// ---- Exact Python class (moil_pattern_gennerator.py), snake_case 1:1 -------
class MoilCaliPatternGenerator {
public:
    using Json = QJsonObject;

    // init_json_*
    static Json init_json_concentric(int height, int width);
    static Json init_json_stripline(int height, int width);
    static Json init_json_chessboard(int height, int width);

    // render
    static cv::Mat render_from_json(const Json &cfg);

    // detect
    static std::string detect_pattern_type(const Json &config);

    // check
    static bool check_json_concentric(const Json &json_concentric);
    static bool check_json_stripeline(const Json &json_stripeline);
    static bool check_json_chessboard(const Json &json_chessboard);

    // universal setters
    static Json set_height(Json pattern_json, int height);
    static Json set_width(Json pattern_json, int width);
    static Json set_crossline(Json pattern_json, bool crossline);
    static Json set_positive_color(Json pattern_json, const std::optional<std::array<int, 3>> &color);
    static Json set_negative_color(Json pattern_json, const std::optional<std::array<int, 3>> &color);
    static Json set_pos_neg_color(Json pattern_json, bool enable);

    // concentric setters
    static Json set_concentric_shape(Json pattern_json, int layer, const std::string &shape);
    static Json set_concentric_radius(Json pattern_json, int layer, int radius);
    static Json set_concentric_color(Json pattern_json, int layer, std::array<int, 3> color);
    static Json set_concentric_cx(Json pattern_json, int layer, int cx);
    static Json set_concentric_cy(Json pattern_json, int layer, int cy);

    // stripeline setters
    static Json set_stripeline_interval(Json pattern_json, int layer, int interval);
    static Json set_stripeline_color(Json pattern_json, int layer, std::array<int, 3> color);

    // chessboard setters
    static Json set_chessboard_pixel_size(Json pattern_json, double pixel_size);
    static Json set_chessboard_grid_h_mm(Json pattern_json, int grid_h_mm);
    static Json set_chessboard_grid_w_mm(Json pattern_json, int grid_w_mm);

    static Json apply_pos_neg_colors(Json pattern_json);

private:
    static cv::Mat _render_concentric(const Json &cfg);
    static cv::Mat _render_stripeline(const Json &cfg);
    static cv::Mat _render_chessboard(const Json &cfg);
    static cv::Mat _draw_crossline(const cv::Mat &base_image);

    static cv::Scalar _rgb_to_bgr(const std::array<int, 3> &rgb);
    static std::pair<int, int> _get_hw(const Json &cfg);  // (height, width)
    static std::vector<std::pair<int, QJsonObject>> _iter_layers(const Json &cfg, int start = 1,
                                                                 int end = 25);
    static std::optional<std::array<int, 3>> _valid_rgb(const QJsonValue &v);
    static std::array<int, 3> jsonToRgb(const QJsonValue &v, std::array<int, 3> fallback);
};
