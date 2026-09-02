#include "Moildev.h"

#include <algorithm>
#include <cmath>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <opencv2/imgproc.hpp>

namespace {
// MoilCV.cpp hard-codes these; keep them identical so mapsAnypointMode2 is
// numerically equal to the native AnyPointM2.
constexpr double kMoilPI = 3.1415926;
constexpr double kPctUnitWidth = 1.27;
constexpr double kPctUnitHeight = 1.27;
constexpr double kFocalLengthForZoom = 250.0;

double poly6(double a, double p0, double p1, double p2, double p3, double p4, double p5) {
    return p0 * a * a * a * a * a * a + p1 * a * a * a * a * a + p2 * a * a * a * a +
           p3 * a * a * a + p4 * a * a + p5 * a;
}

// Pulls a field out of a camera-parameter QJsonObject (top-level or nested).
double num(const QJsonObject &o, const char *key, double def = 0.0) {
    return o.contains(key) ? o.value(key).toDouble(def) : def;
}
}  // namespace

Moildev::Moildev(const std::string &fileCameraParameter, const std::string &cameraType,
                 double resolutionRatio) {
    load(fileCameraParameter, cameraType, resolutionRatio);
}

bool Moildev::load(const std::string &fileCameraParameter, const std::string &cameraType,
                   double resolutionRatio) {
    valid_ = false;
    resolutionRatio_ = resolutionRatio;

    QFile f(QString::fromStdString(fileCameraParameter));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();

    // Select the parameter block: nested under camera_type, or top-level.
    QJsonObject o;
    const QString ct = QString::fromStdString(cameraType);
    if (!ct.isEmpty() && root.contains(ct) && root.value(ct).isObject()) {
        o = root.value(ct).toObject();
    } else if (root.contains("cameraName")) {
        o = root;
    } else {
        return false;
    }

    const double rr = resolutionRatio_;
    cameraName_ = o.value("cameraName").toString().toStdString();
    cameraFov_ = o.contains("cameraFov") ? num(o, "cameraFov", 220.0) : 220.0;
    sensorWidth_ = num(o, "cameraSensorWidth");
    sensorHeight_ = num(o, "cameraSensorHeight");
    icx_ = static_cast<int>(num(o, "iCx") * rr);
    icy_ = static_cast<int>(num(o, "iCy") * rr);
    ratio_ = num(o, "ratio", 1.0);
    imageWidth_ = static_cast<int>(num(o, "imageWidth") * rr);
    imageHeight_ = static_cast<int>(num(o, "imageHeight") * rr);
    calibrationRatio_ = num(o, "calibrationRatio", 1.0);
    p0_ = num(o, "parameter0") * rr;
    p1_ = num(o, "parameter1") * rr;
    p2_ = num(o, "parameter2") * rr;
    p3_ = num(o, "parameter3") * rr;
    p4_ = num(o, "parameter4") * rr;
    p5_ = num(o, "parameter5") * rr;

    if (cameraName_.empty() || imageWidth_ <= 0 || imageHeight_ <= 0) return false;

    initAlphaRhoTable();
    valid_ = true;
    return true;
}

// Ports Moildev.__init_alpha_rho_table (1800*3 / 3600*3 entries).
void Moildev::initAlphaRhoTable() {
    const int N = 1800 * 3;   // 5400
    const int M = 3600 * 3;   // 10800
    alphaToRho_.assign(N, 0.0);
    for (int i = 0; i < N; ++i) {
        const double alpha = i / 10.0 * M_PI / 180.0;
        alphaToRho_[i] = poly6(alpha, p0_, p1_, p2_, p3_, p4_, p5_) * calibrationRatio_;
    }

    rhoToAlpha_.clear();
    rhoToAlpha_.reserve(M);
    int i = 0;
    int index = 0;
    while (i < N) {
        while (index < alphaToRho_[i]) {
            rhoToAlpha_.push_back(i);
            ++index;
        }
        ++i;
    }
    while (index < M) {
        rhoToAlpha_.push_back(i);
        ++index;
    }
}

double Moildev::getAlphaFromRho(int rho) const {
    if (rhoToAlpha_.empty()) return 0.0;
    const int last = static_cast<int>(rhoToAlpha_.size()) - 1;
    if (rho >= 0) {
        const int r = rho > last ? last : rho;
        return rhoToAlpha_[r] / 10.0;
    }
    int r = -rho;
    if (r > last) r = last;
    return -rhoToAlpha_[r] / 10.0;
}

double Moildev::getRhoFromAlpha(double alpha) const {
    if (alphaToRho_.empty()) return 0.0;
    int idx = static_cast<int>(std::lround(alpha * 10.0));
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(alphaToRho_.size())) idx = static_cast<int>(alphaToRho_.size()) - 1;
    return alphaToRho_[idx];
}

std::pair<double, double> Moildev::getAlphaBeta(double x, double y, int mode, bool &ok) const {
    ok = false;
    const double delta_x = x - icx_;
    const double delta_y = -(y - icy_);
    const double rho110 = getRhoFromAlpha(110);
    const double range_left = x - (icx_ - rho110 - 4);
    const double range_right = imageWidth_ - (icx_ - rho110 + 4);

    if (range_left >= 0 && x <= range_right) {
        double alpha, beta;
        if (mode == 1) {
            const int r = static_cast<int>(std::lround(std::sqrt(delta_x * delta_x + delta_y * delta_y)));
            alpha = getAlphaFromRho(r);
            double angle;
            if (x == icy_)  // faithful to Moildev.py (compares x to icy)
                angle = 0.0;
            else
                angle = std::atan2(delta_y, delta_x) * 180.0 / M_PI;
            beta = 90.0 - angle;
        } else {
            alpha = getAlphaFromRho(static_cast<int>(delta_y));
            beta = getAlphaFromRho(static_cast<int>(delta_x));
        }
        ok = true;
        return {alpha, beta};
    }
    return {0.0, 0.0};
}

std::pair<int, int> Moildev::getCoordinateFromAlphaBeta(double alpha, double beta, int mode,
                                                        bool &ok) const {
    ok = false;
    const double maxDeg = std::floor(cameraFov_ / 2.0);
    if (std::abs(alpha) > maxDeg) return {0, 0};

    double dx, dy;
    if (mode == 1) {
        const double rho = getRhoFromAlpha(alpha);
        const double angleRad = (90.0 - beta) * M_PI / 180.0;
        dx = rho * std::cos(angleRad);
        dy = rho * std::sin(angleRad);
    } else {
        auto rhoSigned = [this](double a) {
            return std::copysign(getRhoFromAlpha(std::abs(a)), a);
        };
        dx = rhoSigned(beta);
        dy = rhoSigned(alpha);
    }
    const int xx = static_cast<int>(std::lround(icx_ + dx));
    const int yy = static_cast<int>(std::lround(icy_ - dy));
    const double cover = getRhoFromAlpha(maxDeg) + 4;
    if (std::abs(xx - icx_) > cover || std::abs(yy - icy_) > cover) return {0, 0};
    ok = true;
    return {xx, yy};
}

// Ports MoilCV::AnyPointM2 (moildev/dll/MoilCV.cpp). The Python wrapper clamps
// pitch/yaw to [-110, 110] (zeroing both when out of range) before the native
// call; that guard is reproduced here.
void Moildev::mapsAnypointMode2(double pitch, double yaw, double zoom, cv::Mat &mapX,
                                cv::Mat &mapY) const {
    mapsAnypointMode2(pitch, yaw, zoom, mapX, mapY, imageWidth_, imageHeight_);
}

void Moildev::mapsAnypointMode2(double pitch, double yaw, double zoom, cv::Mat &mapX,
                                cv::Mat &mapY, int outW, int outH) const {
    if (pitch < -110 || pitch > 110 || yaw < -110 || yaw > 110) {
        pitch = 0;
        yaw = 0;
    }

    const int h = imageHeight_;
    const int w = imageWidth_;
    if (outW <= 0) outW = w;
    if (outH <= 0) outH = h;
    mapX.create(outH, outW, CV_32F);
    mapY.create(outH, outW, CV_32F);
    // Map each output pixel back to a full-resolution destination coordinate so
    // the field of view matches the full-res view, only sampled more coarsely.
    const double sx = static_cast<double>(w) / outW;
    const double sy = static_cast<double>(h) / outH;

    const double icx = icx_ * ratio_;
    const double icy = icy_ * ratio_;
    const double dcx = imageWidth_ / 2.0 * ratio_;
    const double dcy = imageHeight_ / 2.0 * ratio_;

    const double thetaX = pitch * (kMoilPI / 180.0);
    const double thetaY = yaw * (kMoilPI / 180.0);

    const double widthCosB = kPctUnitWidth * std::cos(thetaY);
    const double heightSinASinB = kPctUnitHeight * std::sin(thetaX) * std::sin(thetaY);
    const double flZoomCosASinB = kFocalLengthForZoom * zoom * std::cos(thetaX) * std::sin(thetaY);
    const double heightCosA = kPctUnitHeight * std::cos(thetaX);
    const double flZoomSinA = kFocalLengthForZoom * zoom * std::sin(thetaX);
    const double widthSinB = kPctUnitWidth * std::sin(thetaY);
    const double heightSinACosB = kPctUnitHeight * std::sin(thetaX) * std::cos(thetaY);
    const double flZoomCosACosB = kFocalLengthForZoom * zoom * std::cos(thetaX) * std::cos(thetaY);

    for (int outY = 0; outY < outH; ++outY) {
        float *mx = mapX.ptr<float>(outY);
        float *my = mapY.ptr<float>(outY);
        const double positionY = outY * sy;
        for (int outX = 0; outX < outW; ++outX) {
            const double positionX = outX * sx;
            const int dst = outX;  // output column index (positionX is the full-res coord)
            double tempX = (positionX - dcx) * widthCosB + (positionY - dcy) * heightSinASinB + flZoomCosASinB;
            double tempY = (positionY - dcy) * heightCosA - flZoomSinA;
            double tempZ = -(positionX - dcx) * widthSinB + (positionY - dcy) * heightSinACosB + flZoomCosACosB;

            tempX = -tempX;
            tempY = -tempY;
            const double alpha = std::atan2(std::sqrt(tempX * tempX + tempY * tempY), tempZ);

            double beta;
            if (tempX != 0)
                beta = std::atan2(tempY, tempX);
            else if (tempY >= 0)
                beta = kMoilPI / 2;
            else
                beta = -(kMoilPI / 2);

            const double ih = poly6(alpha, p0_, p1_, p2_, p3_, p4_, p5_) * calibrationRatio_ *
                              sensorHeight_ * ratio_;
            const double senH = icx * sensorWidth_ * ratio_ - ih * std::cos(beta);
            const double senV = icy * sensorHeight_ - ih * std::sin(beta);

            const double origX = std::round(senH / (sensorWidth_ * ratio_));
            const double origY = std::round(senV / sensorHeight_);

            if (origX >= 0 && origX < w && origY >= 0 && origY < h) {
                mx[dst] = static_cast<float>(origX);
                my[dst] = static_cast<float>(origY);
            } else {
                mx[dst] = 0;
                my[dst] = 0;
            }
        }
    }
}

cv::Mat Moildev::anypointMode2(const cv::Mat &image, double pitch, double yaw, double zoom) const {
    cv::Mat mapX, mapY, out;
    mapsAnypointMode2(pitch, yaw, zoom, mapX, mapY);
    cv::remap(image, out, mapX, mapY, cv::INTER_LINEAR);
    return out;
}

// Ports MoilCV::AnyPointM (tube-mode). The Python wrapper wraps beta into [0,360]
// and clamps alpha/beta (zeroing both when out of range); reproduced here.
void Moildev::mapsAnypointMode1(double alpha, double beta, double zoom, cv::Mat &mapX,
                                cv::Mat &mapY) const {
    if (beta < 0) beta += 360;
    if (alpha < -110 || alpha > 110 || beta < 0 || beta > 360) {
        alpha = 0;
        beta = 0;
    } else {
        alpha = std::max(-110.0, std::min(110.0, alpha));
        beta = std::max(0.0, std::min(360.0, beta));
    }

    const int h = imageHeight_, w = imageWidth_;
    mapX.create(h, w, CV_32F);
    mapY.create(h, w, CV_32F);

    const double icx = icx_ * ratio_;
    const double icy = icy_ * ratio_;
    const double dcx = imageWidth_ / 2.0 * ratio_;
    const double dcy = imageHeight_ / 2.0 * ratio_;

    const double mAlpha = alpha * (kMoilPI / 180.0);
    const double mBeta = (beta + 180.0) * (kMoilPI / 180.0);

    const double widthCosB = kPctUnitWidth * std::cos(mBeta);
    const double heightCosASinB = kPctUnitHeight * std::cos(mAlpha) * std::sin(mBeta);
    const double flZoomSinASinB = kFocalLengthForZoom * zoom * std::sin(mAlpha) * std::sin(mBeta);
    const double widthSinB = kPctUnitWidth * std::sin(mBeta);
    const double heightCosACosB = kPctUnitHeight * std::cos(mAlpha) * std::cos(mBeta);
    const double flZoomSinACosB = kFocalLengthForZoom * zoom * std::sin(mAlpha) * std::cos(mBeta);
    const double heightSinA = kPctUnitHeight * std::sin(mAlpha);
    const double flZoomCosA = kFocalLengthForZoom * zoom * std::cos(mAlpha);

    for (int positionY = 0; positionY < h; ++positionY) {
        float *mx = mapX.ptr<float>(positionY);
        float *my = mapY.ptr<float>(positionY);
        for (int positionX = 0; positionX < w; ++positionX) {
            const double tempX = (positionX - dcx) * widthCosB - (positionY - dcy) * heightCosASinB + flZoomSinASinB;
            const double tempY = (positionX - dcx) * widthSinB + (positionY - dcy) * heightCosACosB - flZoomSinACosB;
            const double tempZ = (positionY - dcy) * heightSinA + flZoomCosA;
            const double a = std::atan2(std::sqrt(tempX * tempX + tempY * tempY), tempZ);

            double beta2;
            if (tempX != 0)
                beta2 = std::atan2(tempY, tempX);
            else if (tempY >= 0)
                beta2 = kMoilPI / 2;
            else
                beta2 = -(kMoilPI / 2);

            const double ih = poly6(a, p0_, p1_, p2_, p3_, p4_, p5_) * calibrationRatio_ *
                              sensorHeight_ * ratio_;
            const double senH = icx * sensorWidth_ * ratio_ - ih * std::cos(beta2);
            const double senV = icy * sensorHeight_ - ih * std::sin(beta2);
            const double origX = std::round(senH / (sensorWidth_ * ratio_));
            const double origY = std::round(senV / sensorHeight_);
            if (origX >= 0 && origX < w && origY >= 0 && origY < h) {
                mx[positionX] = static_cast<float>(origX);
                my[positionX] = static_cast<float>(origY);
            } else {
                mx[positionX] = 0;
                my[positionX] = 0;
            }
        }
    }
}

// Ports MoilCV::PanoramaCar. Rows map to alpha in [0, alphaMax], cols to beta in
// [0, 2pi]; the panorama is centred on (icAlphaDeg, icBetaDeg). Map size equals the
// fisheye (imageHeight x imageWidth).
void Moildev::mapsPanoramaCar(double alphaMax, double icAlphaDeg, double icBetaDeg, bool flip,
                              cv::Mat &mapX, cv::Mat &mapY) const {
    const int rows = imageHeight_, cols = imageWidth_;
    mapX.create(rows, cols, CV_32F);
    mapY.create(rows, cols, CV_32F);

    const double icx = icx_ * ratio_;
    const double icy = icy_ * ratio_;
    const double icAlphaPivot = icAlphaDeg * kMoilPI / 180.0;
    const double icBetaPivot = -icBetaDeg * kMoilPI / 180.0;

    const double kx = std::sin(icAlphaPivot) * std::cos(icBetaPivot);
    const double ky = std::sin(icAlphaPivot) * std::sin(icBetaPivot);
    const double kz = std::cos(icAlphaPivot);

    for (int r = 0; r < rows; ++r) {
        float *mx = mapX.ptr<float>(r);
        float *my = mapY.ptr<float>(r);
        const double ingAlpha = static_cast<double>(r) / rows * alphaMax * kMoilPI / 180.0;
        const double targetAlpha = icAlphaPivot + ingAlpha;
        const double Vx = std::sin(targetAlpha) * std::cos(icBetaPivot);
        const double Vy = std::sin(targetAlpha) * std::sin(icBetaPivot);
        const double az = std::cos(targetAlpha);
        const double kxa_x = ky * az - kz * Vy;
        const double kxa_y = kz * Vx - kx * az;
        const double kxa_z = kx * Vy - ky * Vx;
        const double k_a = kx * Vx + ky * Vy + kz * az;
        for (int c = 0; c < cols; ++c) {
            const double ingBeta = static_cast<double>(c) / cols * 2 * kMoilPI;
            const double sB = std::sin(ingBeta), cB = std::cos(ingBeta);
            const double VrotX = cB * Vx + kxa_x * sB + kx * k_a * (1 - cB);
            const double VrotY = cB * Vy + kxa_y * sB + ky * k_a * (1 - cB);
            const double VrotZ = cB * az + kxa_z * sB + kz * k_a * (1 - cB);
            double fishBeta = std::atan2(VrotY, VrotX);
            const double fishAlpha = std::atan2(std::sqrt(VrotX * VrotX + VrotY * VrotY), VrotZ);
            fishBeta = kMoilPI / 2 - fishBeta;
            const double ih = poly6(fishAlpha, p0_, p1_, p2_, p3_, p4_, p5_) * calibrationRatio_ * ratio_;
            const double origX = flip ? std::round(icx + ih * std::cos(fishBeta))
                                      : std::round(icx - ih * std::cos(fishBeta));
            const double origY = std::round(icy - ih * std::sin(fishBeta));
            if (origX >= 0 && origX < cols && origY >= 0 && origY < rows) {
                mx[c] = static_cast<float>(origX);
                my[c] = static_cast<float>(origY);
            } else {
                mx[c] = 0;
                my[c] = 0;
            }
        }
    }
}
