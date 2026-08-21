#pragma once

#include <QString>

#include "EditParameters.h"

// One photo in the library: a path to the original file on disk, plus the
// current non-destructive edit parameters for it. The original pixels are
// never touched; edits live in a small JSON sidecar file next to the photo
// (e.g. "IMG_0001.jpg.larryedit.json") so they survive across sessions and
// can always be reverted or re-applied.
//
// Favorite status lives here too, alongside EditParameters rather than on
// it: it's browsing/curation metadata, not a value that feeds into
// rendering the photo, so it never touches ImageProcessor. It's persisted
// in the same sidecar file as a sibling JSON key rather than a separate
// file - real tools do the same (an XMP sidecar bundles develop settings
// and ratings/flags together) - but kept flat rather than nested under the
// edit-parameter fields, so sidecars written before this feature existed
// still parse identically (a missing "favorite" key just defaults to false).
class Photo
{
public:
    explicit Photo(QString filePath);

    const QString &filePath() const { return m_filePath; }
    QString fileName() const;
    QString sidecarPath() const;

    const EditParameters &editParameters() const { return m_params; }
    void setEditParameters(const EditParameters &params);

    bool isFavorite() const { return m_isFavorite; }
    void setFavorite(bool favorite);

    bool hasUnsavedEdits() const { return m_dirty; }

    void loadSidecarIfPresent();
    void saveSidecar();

private:
    QString m_filePath;
    EditParameters m_params;
    bool m_isFavorite = false;
    bool m_dirty = false;
};
