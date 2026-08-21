#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QVector>

class PhotoLibrary;

// Adapts PhotoLibrary to Qt's model/view framework for the thumbnail grid.
// Thumbnails are decoded off the UI thread on first request and cached
// in-memory; rows that don't have a cached thumbnail yet return a blank
// placeholder and repaint themselves once the background job completes.
//
// Optionally filters down to favorited photos only. This introduces two
// distinct notions of "index": the *view row* (this model's row, 0..N-1
// over whatever subset is currently visible) and the *library index* (the
// photo's stable position in PhotoLibrary, unaffected by filtering). Every
// caller outside this class - MainWindow, ThumbnailPanel's selection
// handling - deals exclusively in library indices, via PhotoIndexRole; only
// this model's own internals (m_visibleIndices, rowCount(), data()) need to
// know view rows exist at all. The thumbnail cache is keyed by library
// index too, so toggling the filter never forces re-decoding anything -
// it only ever recomputes which already-cached rows are currently visible.
class ThumbnailModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static constexpr int PhotoIndexRole = Qt::UserRole + 1; // -> this row's stable library index

    explicit ThumbnailModel(PhotoLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Drops the cached thumbnail for a photo (by library index) and
    // re-fetches it (with its current EditParameters applied) - call after
    // an edit is committed, so its rotate/crop/etc. shows up in the
    // thumbnail strip too, not just the main viewer.
    void invalidateThumbnail(int libraryIndex);

    // Call after a photo's favorite status changes - unlike other edits,
    // this can change *which rows are visible* under the favorites-only
    // filter, not just a thumbnail's appearance, so it needs its own entry
    // point rather than reusing invalidateThumbnail.
    void notifyFavoriteChanged(int libraryIndex);

public slots:
    void setFavoritesOnlyFilter(bool favoritesOnly);

private slots:
    void onLibraryChanged();

private:
    void rebuildVisibleIndices();
    void requestThumbnail(int libraryIndex);
    void onThumbnailReady(int libraryIndex, const QImage &thumbnail);

    PhotoLibrary *m_library;
    QVector<int> m_visibleIndices; // view row -> library index
    bool m_favoritesOnly = false;
    QHash<int, QPixmap> m_thumbnailCache; // keyed by library index
    QSet<int> m_pendingRows;              // keyed by library index
};
