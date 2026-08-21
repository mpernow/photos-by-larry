#pragma once

#include <QWidget>

class QListView;
class QCheckBox;
class ThumbnailModel;

// Left-hand panel: a scrollable grid of photo thumbnails for the currently
// open directory, plus a checkbox to filter it down to favorites only.
// Purely a view - selection state, thumbnail generation, and the actual
// filtering live in ThumbnailModel / PhotoLibrary.
class ThumbnailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ThumbnailPanel(QWidget *parent = nullptr);

    void setModel(ThumbnailModel *model);

signals:
    // Always the photo's stable PhotoLibrary index (ThumbnailModel::PhotoIndexRole),
    // never the view row - those diverge once the favorites-only filter hides rows,
    // and every caller of this signal already assumes "row" means a library index.
    void photoSelected(int row);
    void favoritesOnlyToggled(bool favoritesOnly);

private:
    QListView *m_listView;
    QCheckBox *m_favoritesOnlyCheckBox;
};
