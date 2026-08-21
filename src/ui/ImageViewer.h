#pragma once

#include <QGraphicsView>

class QGraphicsPixmapItem;
class CropOverlayItem;

// Central panel: displays the currently rendered (original + edits) image,
// with pan/zoom. Knows nothing about photos, edits, or OpenCV - it just
// shows whatever QImage it's handed. Also hosts the interactive crop
// rectangle (CropOverlayItem) on request; ImageViewer only deals in
// normalized [0,1] crop fractions so callers never need to know the
// displayed image's pixel dimensions.
class ImageViewer : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void clear();

    // Shows the crop overlay over the currently displayed image, seeded with
    // a normalized [0,1] starting rect. Suspends pan-drag while active.
    void beginCropping(const QRectF &initialNormalizedRect);
    QRectF currentCropNormalizedRect() const;
    void endCropping();
    bool isCropping() const;

public slots:
    // Whether resizing the crop overlay preserves its current aspect ratio.
    // Stored here (not just forwarded) so it survives across crop sessions
    // even if toggled while no overlay exists yet.
    void setCropAspectRatioLocked(bool locked);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void fitToView();

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    CropOverlayItem *m_cropOverlay = nullptr;
    bool m_cropAspectRatioLocked = false;
    bool m_hasImage = false;
    bool m_userAdjustedZoom = false; // once true, auto-fit no longer overrides the user's zoom
};
