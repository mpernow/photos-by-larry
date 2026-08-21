#include "ThumbnailPanel.h"

#include "ThumbnailModel.h"

#include <QItemSelectionModel>
#include <QListView>
#include <QVBoxLayout>

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QWidget(parent), m_listView(new QListView(this))
{
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::TopToBottom);
    m_listView->setWrapping(false);
    m_listView->setResizeMode(QListView::Adjust);
    // Not uniform: thumbnails decode asynchronously, so the first item's size
    // hint is initially just a null placeholder. uniformItemSizes(true) would
    // cache that as the size for every row, cropping real thumbnails once
    // they load in - only a window resize would force a relayout to fix it.
    m_listView->setIconSize(QSize(140, 140));
    m_listView->setGridSize(QSize(160, 172));
    m_listView->setSpacing(4);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listView);
}

void ThumbnailPanel::setModel(ThumbnailModel *model)
{
    m_listView->setModel(model);

    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
                if (current.isValid())
                    emit photoSelected(current.row());
            });
}
