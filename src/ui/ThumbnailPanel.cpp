#include "ThumbnailPanel.h"

#include "ThumbnailGeometry.h"
#include "ThumbnailModel.h"

#include <QCheckBox>
#include <QItemSelectionModel>
#include <QListView>
#include <QVBoxLayout>

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QWidget(parent),
      m_listView(new QListView(this)),
      m_favoritesOnlyCheckBox(new QCheckBox(tr("★ Favorites only"), this))
{
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::TopToBottom);
    m_listView->setWrapping(false);
    m_listView->setResizeMode(QListView::Adjust);
    // The model provides a fixed Qt::SizeHintRole matching kCellSize, so every
    // row's layout size is known upfront without depending on (or triggering
    // decode of) the actual thumbnail - see ThumbnailGeometry.h.
    m_listView->setIconSize(ThumbnailGeometry::kIconSize);
    m_listView->setGridSize(ThumbnailGeometry::kCellSize);
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(4);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_favoritesOnlyCheckBox);
    layout->addWidget(m_listView);

    connect(m_favoritesOnlyCheckBox, &QCheckBox::toggled, this, &ThumbnailPanel::favoritesOnlyToggled);
}

void ThumbnailPanel::setModel(ThumbnailModel *model)
{
    m_listView->setModel(model);

    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this, model](const QModelIndex &current, const QModelIndex &) {
                if (!current.isValid())
                    return;
                // The photo's stable library index, not current.row() - those
                // diverge once the favorites-only filter hides rows, and every
                // listener of photoSelected already assumes a library index.
                emit photoSelected(model->data(current, ThumbnailModel::PhotoIndexRole).toInt());
            });
}
