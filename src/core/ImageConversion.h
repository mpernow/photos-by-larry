#pragma once

#include <QImage>
#include <opencv2/core.hpp>

// Bridges between OpenCV's pixel buffers and Qt's, so the two halves of the
// pipeline (OpenCV for processing, Qt for display) can hand images back and
// forth. Every conversion here deep-copies, so callers never have to worry
// about one side's buffer being freed out from under the other.
namespace ImageConversion
{
QImage matToQImage(const cv::Mat &mat);
cv::Mat qImageToMat(const QImage &image);
} // namespace ImageConversion
