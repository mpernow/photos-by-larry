#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPixmap>
#include <QSet>

class PhotoLibrary;

// Adapts PhotoLibrary to Qt's model/view framework for the thumbnail grid.
// Thumbnails are decoded off the UI thread on first request and cached
// in-memory; rows that don't have a cached thumbnail yet return a blank
// placeholder and repaint themselves once the background job completes.
class ThumbnailModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static constexpr int PhotoIndexRole = Qt::UserRole + 1;

    explicit ThumbnailModel(PhotoLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private slots:
    void onLibraryChanged();

private:
    void requestThumbnail(int row);
    void onThumbnailReady(int row, const QImage &thumbnail);

    PhotoLibrary *m_library;
    QHash<int, QPixmap> m_thumbnailCache;
    QSet<int> m_pendingRows;
};
