#pragma once

#include <QString>
#include <opencv2/core.hpp>

// Wraps LibRaw to decode RAW camera files (Fuji .RAF, Canon .CR2/.CR3,
// Nikon .NEF, etc.) into the same shape cv::imread produces for ordinary
// raster formats - BGR channel order - so ImageConversion::loadImage can
// hand either source into the rest of the pipeline interchangeably.
namespace RawDecoder
{
// Full-resolution decode: demosaics the whole sensor image using the
// camera's embedded white balance and sRGB output, at 16 bits per channel
// so ImageProcessor's float pipeline keeps the extra highlight/shadow
// headroom a RAW file offers over an 8-bit JPEG. Slow - this runs the real
// demosaic algorithm across the full sensor resolution. Returns an empty
// Mat if the file can't be opened or decoded.
cv::Mat decode(const QString &path);

// Fast path for thumbnails: pulls the RAW file's embedded preview JPEG
// instead of demosaicing the full sensor image (mirrors what other photo
// apps do for their own thumbnail caches) - orders of magnitude cheaper
// than decode() on a modern high-megapixel sensor. Returns an 8-bit image,
// or an empty Mat if the file has no usable embedded preview.
cv::Mat decodeThumbnail(const QString &path);
} // namespace RawDecoder
