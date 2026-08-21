#include "ImageProcessor.h"

cv::Mat ImageProcessor::apply(const cv::Mat &source, const EditParameters &params)
{
    if (source.empty())
        return source;
    if (params.isIdentity())
        return source.clone();

    cv::Mat result;
    // Classic linear brightness/contrast adjustment: out = contrast * in + brightness.
    // convertTo with alpha/beta does this in one pass, saturating to the valid range.
    source.convertTo(result, -1, params.contrast, params.brightness);
    return result;
}
