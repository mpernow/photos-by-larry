#include "ImageViewer.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QWheelEvent>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent), m_scene(new QGraphicsScene(this)), m_pixmapItem(nullptr)
{
    setScene(m_scene);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setBackgroundBrush(QBrush(QColor(45, 45, 45)));
}

void ImageViewer::setImage(const QImage &image)
{
    const QPixmap pixmap = QPixmap::fromImage(image);
    if (!m_pixmapItem)
        m_pixmapItem = m_scene->addPixmap(pixmap);
    else
        m_pixmapItem->setPixmap(pixmap);

    m_scene->setSceneRect(pixmap.rect());
    m_hasImage = true;
    fitToView();
}

void ImageViewer::clear()
{
    m_scene->clear();
    m_pixmapItem = nullptr;
    m_hasImage = false;
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if (!m_hasImage) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_hasImage)
        fitToView();
}

void ImageViewer::fitToView()
{
    if (m_pixmapItem)
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}
