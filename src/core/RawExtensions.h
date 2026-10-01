#pragma once

#include <QStringList>

// RAW file extensions (lowercase, no leading dot) this app decodes via
// LibRaw - shared between PhotoLibrary's directory filter and
// ImageConversion's decode-format dispatch so the two lists can't drift
// apart. LibRaw handles all of these through roughly the same
// open/unpack/dcraw_process path.
inline const QStringList &rawFileExtensions()
{
    static const QStringList extensions = {
        "raf", "cr2", "cr3", "nef", "arw", "orf", "rw2", "dng",
    };
    return extensions;
}
