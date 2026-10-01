#include "RawDecoder.h"

#include <libraw/libraw.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <thread>

namespace
{

// decode() runs on a background thread (MainWindow kicks it off via
// QtConcurrent so the UI thread isn't blocked for the several seconds a
// full demosaic takes - see MainWindow::loadPhoto). But LibRaw's demosaic is
// itself OpenMP-parallelized across every available core by default, which
// starves the UI thread of CPU time for the whole decode regardless of which
// thread *launched* it - measured on an 8-logical-core machine (4 physical
// cores, hyperthreaded), dragging a slider (ImageProcessor::apply calls) went
// from ~9ms/~12ms-worst-case to ~21ms average/53ms worst-case while a decode
// was in flight. A naive "leave N logical cores free" cap barely helped
// (measured: cores-2 = 6 threads was nearly as bad as uncapped) because
// hyperthread siblings don't add independent throughput - 6 busy logical
// threads already saturate all 4 physical cores. A small fixed cap instead
// of a core-count-derived one sidesteps needing to detect physical core
// count (not portably available via std::thread): measured sweep on that
// same machine put 2 threads at ~12ms average/~18ms worst-case (close to the
// ~9ms idle baseline) for about 2x the decode wall time (12s vs 6.4s
// uncapped) - a clearly better tradeoff than the alternatives tried (1
// thread: same UI smoothness but ~3x decode time instead of ~2x; 3-4
// threads: decode only marginally faster than 2 but UI smoothness mostly
// gone again). LIBRAW_USE_OPENMP (and the omp_set_num_threads() it makes
// available) is only defined when LibRaw itself was actually built with
// OpenMP support - see libraw_types.h - so this is a no-op on a build
// without it.
void limitDecodeThreadsForUiResponsiveness()
{
#ifdef LIBRAW_USE_OPENMP
    const int cores = static_cast<int>(std::thread::hardware_concurrency());
    omp_set_num_threads(std::max(1, std::min(2, cores - 1)));
#endif
}


// LibRaw emits interleaved RGB; this codebase's convention throughout
// (ImageConversion, ImageProcessor) is BGR, matching cv::imread's default.
// cvtColor always allocates a fresh output buffer rather than aliasing its
// input, so the returned Mat is a genuine deep copy - safe to use after
// `image`'s buffer is freed via LibRaw::dcraw_clear_mem.
cv::Mat toBgrMat(libraw_processed_image_t *image)
{
    const int matType = image->bits == 16 ? CV_16UC3 : CV_8UC3;
    const cv::Mat rgb(image->height, image->width, matType, image->data);
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

} // namespace

cv::Mat RawDecoder::decode(const QString &path)
{
    LibRaw processor;

    if (processor.open_file(path.toLocal8Bit().constData()) != LIBRAW_SUCCESS)
        return {};
    if (processor.unpack() != LIBRAW_SUCCESS)
        return {};

    limitDecodeThreadsForUiResponsiveness();

    processor.imgdata.params.output_bps = 16;
    processor.imgdata.params.use_camera_wb = 1;
    processor.imgdata.params.output_color = 1; // sRGB

    if (processor.dcraw_process() != LIBRAW_SUCCESS)
        return {};

    int errorCode = LIBRAW_SUCCESS;
    libraw_processed_image_t *image = processor.dcraw_make_mem_image(&errorCode);
    if (!image || errorCode != LIBRAW_SUCCESS || image->colors != 3) {
        if (image)
            LibRaw::dcraw_clear_mem(image);
        return {};
    }

    const cv::Mat result = toBgrMat(image);
    LibRaw::dcraw_clear_mem(image);
    return result;
}

cv::Mat RawDecoder::decodeThumbnail(const QString &path)
{
    LibRaw processor;

    if (processor.open_file(path.toLocal8Bit().constData()) != LIBRAW_SUCCESS)
        return {};
    if (processor.unpack_thumb() != LIBRAW_SUCCESS)
        return {};

    int errorCode = LIBRAW_SUCCESS;
    libraw_processed_image_t *thumb = processor.dcraw_make_mem_thumb(&errorCode);
    if (!thumb || errorCode != LIBRAW_SUCCESS)
        return {};

    cv::Mat result;
    if (thumb->type == LIBRAW_IMAGE_JPEG) {
        const cv::Mat jpegBytes(1, static_cast<int>(thumb->data_size), CV_8UC1, thumb->data);
        result = cv::imdecode(jpegBytes, cv::IMREAD_COLOR); // already BGR
    } else if (thumb->colors == 3) {
        result = toBgrMat(thumb);
    }

    LibRaw::dcraw_clear_mem(thumb);
    return result;
}
