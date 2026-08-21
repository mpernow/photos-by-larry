#pragma once

#include <QString>

#include "EditParameters.h"

// One photo in the library: a path to the original file on disk, plus the
// current non-destructive edit parameters for it. The original pixels are
// never touched; edits live in a small JSON sidecar file next to the photo
// (e.g. "IMG_0001.jpg.larryedit.json") so they survive across sessions and
// can always be reverted or re-applied.
class Photo
{
public:
    explicit Photo(QString filePath);

    const QString &filePath() const { return m_filePath; }
    QString fileName() const;
    QString sidecarPath() const;

    const EditParameters &editParameters() const { return m_params; }
    void setEditParameters(const EditParameters &params);
    bool hasUnsavedEdits() const { return m_dirty; }

    void loadSidecarIfPresent();
    void saveSidecar();

private:
    QString m_filePath;
    EditParameters m_params;
    bool m_dirty = false;
};
