#include "ImageProcessor.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

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

// Simple per-channel gain shift rather than true Kelvin-based color science -
// consistent with the rest of this pipeline's "plain linear ops" approach.
// temperature/tint are both roughly -100..100; 0 leaves the image untouched.
cv::Mat applyWhiteBalance(const cv::Mat &source, double temperature, double tint)
{
    if (temperature == 0.0 && tint == 0.0)
        return source;

    constexpr double kTemperatureStrength = 0.003; // ±100 temperature -> up to ±30% red/blue gain
    constexpr double kTintStrength = 0.003;         // ±100 tint -> up to ±30% green gain

    const double blueGain = 1.0 - temperature * kTemperatureStrength + tint * kTintStrength * 0.5;
    const double greenGain = 1.0 - tint * kTintStrength;
    const double redGain = 1.0 + temperature * kTemperatureStrength + tint * kTintStrength * 0.5;

    std::vector<cv::Mat> channels; // BGR order, matching cv::imread's default
    cv::split(source, channels);
    channels[0].convertTo(channels[0], -1, blueGain, 0.0);
    channels[1].convertTo(channels[1], -1, greenGain, 0.0);
    channels[2].convertTo(channels[2], -1, redGain, 0.0);

    cv::Mat result;
    cv::merge(channels, result);
    return result;
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

    working = applyWhiteBalance(working, params.temperature, params.tint);

    if (params.brightness == 0.0 && params.contrast == 1.0)
        return working.clone(); // detach from source's buffer before handing back

    cv::Mat result;
    // Classic linear brightness/contrast adjustment: out = contrast * in + brightness.
    // convertTo with alpha/beta does this in one pass, saturating to the valid range.
    working.convertTo(result, -1, params.contrast, params.brightness);
    return result;
}
