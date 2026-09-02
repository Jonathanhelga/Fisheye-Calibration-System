#include "compute.h"
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "ComputeOps.h"

QString Compute::detect(const QString &op, const std::vector<cv::Mat> &images,
                        const QString &params, QString *err) {
    return ComputeOps::runDetectOp(op, images, params, err);
}

QString Compute::detectEncoded(const QString &op, const QVector<QByteArray> &pngs,
                               const QString &params, QString *err) {
    // Decode here so ComputeOps sees Mats. Over ROS this was the one place the
    // bytes could be forwarded untouched; in-process the decode has to happen
    // somewhere, and doing it here keeps every caller's "I have file bytes" case
    // one call long.
    std::vector<cv::Mat> images;
    images.reserve(static_cast<std::size_t>(pngs.size()));

    for (int i = 0; i < pngs.size(); ++i) {
        const QByteArray &bytes = pngs.at(i);
        const std::vector<uchar> buffer(bytes.begin(), bytes.end());
        cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (image.empty()) {
            if (err)
                *err = QStringLiteral("image %1 of %2 could not be decoded")
                           .arg(i + 1)
                           .arg(pngs.size());
            return {};
        }
        images.push_back(std::move(image));
    }

    return ComputeOps::runDetectOp(op, images, params, err);
}

bool Compute::cali(const QString &op, CaliTableData &table, const QString &params,
                   QString *result, QString *err) {
    return ComputeOps::runCaliOp(op, table, params, result, err);
}

cv::Mat Compute::renderPattern(const QString &patternJson, int width, int height, QString *err) {
    return ComputeOps::renderPattern(patternJson, width, height, err);
}
