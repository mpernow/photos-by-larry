#pragma once

#include <QWidget>

class QListView;
class ThumbnailModel;

// Left-hand panel: a scrollable grid of photo thumbnails for the currently
// open directory. Purely a view - selection state and thumbnail generation
// live in ThumbnailModel / PhotoLibrary.
class ThumbnailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ThumbnailPanel(QWidget *parent = nullptr);

    void setModel(ThumbnailModel *model);

signals:
    void photoSelected(int row);

private:
    QListView *m_listView;
};
