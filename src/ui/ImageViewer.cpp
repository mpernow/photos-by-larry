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
    // A slider adjustment re-renders the same photo at the same dimensions
    // (only pixel values change), so only reset the fit/zoom when the image
    // is actually new - otherwise every slider tick would both redo needless
    // work and keep yanking the view back to fit, undoing any zoom/pan the
    // user had set.
    const bool isNewImage = !m_pixmapItem || m_pixmapItem->pixmap().size() != pixmap.size();

    if (!m_pixmapItem)
        m_pixmapItem = m_scene->addPixmap(pixmap);
    else
        m_pixmapItem->setPixmap(pixmap);

    if (isNewImage) {
        m_scene->setSceneRect(pixmap.rect());
        m_hasImage = true;
        m_userAdjustedZoom = false; // a freshly selected photo should start fit-to-view
        fitToView();
    }
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
    m_userAdjustedZoom = true;
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // Zooming in past the fit level makes a scrollbar appear, which resizes
    // the viewport and re-enters this handler - if we always re-fit here,
    // that immediately snaps the image straight back to "fit", so zooming in
    // looks like it does nothing (zooming out never shows a scrollbar, so it
    // never hit this). Only auto-fit until the user actually picks a zoom.
    if (m_hasImage && !m_userAdjustedZoom)
        fitToView();
}

void ImageViewer::fitToView()
{
    if (m_pixmapItem)
        fitInView(m_pixmapItem, Qt::KeepAspectRatio);
}
