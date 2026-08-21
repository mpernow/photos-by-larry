#include "ImageViewer.h"

#include "CropOverlayItem.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QWheelEvent>

namespace
{
QRectF denormalize(const QRectF &normalized, const QRectF &imageRect)
{
    return QRectF(imageRect.x() + normalized.x() * imageRect.width(),
                   imageRect.y() + normalized.y() * imageRect.height(), normalized.width() * imageRect.width(),
                   normalized.height() * imageRect.height());
}
} // namespace

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
    m_scene->clear(); // also deletes m_cropOverlay, if it existed - don't leave that dangling
    m_pixmapItem = nullptr;
    m_cropOverlay = nullptr;
    m_hasImage = false;
}

void ImageViewer::beginCropping(const QRectF &initialNormalizedRect)
{
    if (!m_pixmapItem)
        return;

    if (!m_cropOverlay) {
        m_cropOverlay = new CropOverlayItem();
        m_cropOverlay->setZValue(1.0); // paint (and hit-test) above the pixmap item
        m_scene->addItem(m_cropOverlay);
    }

    const QRectF imageRect = m_pixmapItem->boundingRect();
    m_cropOverlay->setImageRect(imageRect);
    m_cropOverlay->setCropRect(denormalize(initialNormalizedRect, imageRect));
    m_cropOverlay->setVisible(true);

    // Cropping drags the overlay's own handles, not the view.
    setDragMode(QGraphicsView::NoDrag);
}

QRectF ImageViewer::currentCropNormalizedRect() const
{
    if (!m_cropOverlay)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF &img = m_cropOverlay->imageRect();
    const QRectF &crop = m_cropOverlay->cropRect();
    if (img.width() <= 0 || img.height() <= 0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return QRectF((crop.x() - img.x()) / img.width(), (crop.y() - img.y()) / img.height(),
                   crop.width() / img.width(), crop.height() / img.height());
}

void ImageViewer::endCropping()
{
    if (m_cropOverlay)
        m_cropOverlay->setVisible(false);
    setDragMode(QGraphicsView::ScrollHandDrag);
}

bool ImageViewer::isCropping() const
{
    return m_cropOverlay && m_cropOverlay->isVisible();
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
