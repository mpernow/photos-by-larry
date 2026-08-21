#pragma once

#include <opencv2/core.hpp>

#include "EditParameters.h"

// Pure image-processing pipeline: takes an original (never-modified) image
// plus a set of edit parameters, and produces the rendered result. Stateless
// by design so it can be called from the UI thread for a live preview, or
// later from a background thread for export, with no shared state to guard.
namespace ImageProcessor
{
cv::Mat apply(const cv::Mat &source, const EditParameters &params);
} // namespace ImageProcessor
