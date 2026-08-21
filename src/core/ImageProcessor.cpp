#include "ImageProcessor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace
{
// Converts a normalized [0,1] crop rect into a pixel rect on an image of the
// given size, clamped so it always stays a valid, in-bounds region.
cv::Rect cropRectPixels(const QRectF &normalized, int width, int height)
{
    const int x = std::clamp(static_cast<int>(std::lround(normalized.x() * width)), 0, width - 1);
    const int y = std::clamp(static_cast<int>(std::lround(normalized.y() * height)), 0, height - 1);
    const int w = std::clamp(static_cast<int>(std::lround(normalized.width() * width)), 1, width - x);
    const int h = std::clamp(static_cast<int>(std::lround(normalized.height() * height)), 1, height - y);
    return cv::Rect(x, y, w, h);
}
} // namespace

cv::Mat ImageProcessor::apply(const cv::Mat &source, const EditParameters &params)
{
    if (source.empty())
        return source;

    cv::Mat working = source;

    if (const int turns = ((params.rotationQuarterTurns % 4) + 4) % 4; turns != 0) {
        cv::Mat rotated;
        switch (turns) {
        case 1:
            cv::rotate(working, rotated, cv::ROTATE_90_CLOCKWISE);
            break;
        case 2:
            cv::rotate(working, rotated, cv::ROTATE_180);
            break;
        case 3:
            cv::rotate(working, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        }
        working = rotated;
    }

    if (!params.isFullCrop())
        working = working(cropRectPixels(params.cropRect, working.cols, working.rows));

    if (params.brightness == 0.0 && params.contrast == 1.0)
        return working.clone(); // detach from source's buffer before handing back

    cv::Mat result;
    // Classic linear brightness/contrast adjustment: out = contrast * in + brightness.
    // convertTo with alpha/beta does this in one pass, saturating to the valid range.
    working.convertTo(result, -1, params.contrast, params.brightness);
    return result;
}
