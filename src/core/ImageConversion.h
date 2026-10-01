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

// Single decode chokepoint for photo source files: dispatches by extension
// to cv::imread for ordinary raster formats or to RawDecoder for RAW camera
// files, so that branching lives in exactly one place rather than being
// duplicated at every call site that needs to open a photo. Returns an
// empty Mat if the file can't be decoded.
cv::Mat loadImage(const QString &path);

// Cheap approximate load for interactive use (thumbnails, an immediate
// preview while loadImage's full decode is still running in the
// background): for RAW files, returns the embedded preview JPEG rather than
// demosaicing the full sensor image (falling back to the full decode only
// if a file has no usable embedded preview); for ordinary raster formats,
// identical to loadImage() since those are already cheap to decode.
cv::Mat loadPreviewImage(const QString &path);

// True for file extensions ImageConversion routes through RawDecoder rather
// than cv::imread - exposed so callers (MainWindow's background-decode
// scheduling) can tell up front whether a photo is cheap or expensive to
// load, without duplicating the extension list themselves.
bool isRawFile(const QString &path);
} // namespace ImageConversion
