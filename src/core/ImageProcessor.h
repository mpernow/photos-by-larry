#pragma once

#include <opencv2/core.hpp>

#include "EditParameters.h"

// Pure image-processing pipeline: takes an original (never-modified) image
// plus a set of edit parameters, and produces the rendered result. Stateless
// by design so it can be called from the UI thread for a live preview, or
// later from a background thread for export, with no shared state to guard.
// Accepts an 8-bit or 16-bit BGR source (internally normalized to a shared
// float working space, so a higher-bit-depth source keeps its extra
// highlight/shadow headroom through every edit rather than losing it
// immediately); always returns 8-bit BGR, ready for display or export.
namespace ImageProcessor
{
cv::Mat apply(const cv::Mat &source, const EditParameters &params);
} // namespace ImageProcessor
