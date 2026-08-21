#include "ImageConversion.h"

#include <opencv2/imgproc.hpp>

QImage ImageConversion::matToQImage(const cv::Mat &mat)
{
    if (mat.empty())
        return {};

    switch (mat.type()) {
    case CV_8UC1: {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_Grayscale8)
            .copy();
    }
    case CV_8UC3: {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                       QImage::Format_RGB888)
            .copy();
    }
    case CV_8UC4: {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
                       QImage::Format_RGBA8888)
            .copy();
    }
    default:
        return {};
    }
}

cv::Mat ImageConversion::qImageToMat(const QImage &image)
{
    if (image.isNull())
        return {};

    const QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgb(converted.height(), converted.width(), CV_8UC3,
                const_cast<uchar *>(converted.bits()), static_cast<size_t>(converted.bytesPerLine()));

    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    return bgr; // cvtColor allocates a fresh buffer, so this doesn't alias `converted`.
}
