#pragma once

#include <QGraphicsItem>

// Interactive crop rectangle drawn on top of the currently displayed image:
// darkens everything outside the rect, and lets the user drag its body to
// move it or its edges/corners to resize it. ImageViewer keeps the item's
// coordinate space aligned 1:1 with the displayed pixmap's pixels (the item
// sits at the scene origin), so imageRect()/cropRect() are plain pixel rects
// - ImageViewer is responsible for normalizing to/from [0,1] fractions.
class CropOverlayItem : public QGraphicsItem
{
public:
    explicit CropOverlayItem(QGraphicsItem *parent = nullptr);

    // The full extent of the image being cropped.
    void setImageRect(const QRectF &imageRect);
    const QRectF &imageRect() const { return m_imageRect; }

    // The current crop selection, in the same coordinate space as imageRect().
    void setCropRect(const QRectF &rect);
    const QRectF &cropRect() const { return m_cropRect; }

    // When set, resizing (any handle but Move) preserves whatever
    // width/height ratio the crop rect had at the start of that drag,
    // instead of resizing freely.
    void setKeepAspectRatio(bool keep) { m_keepAspectRatio = keep; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum class Handle { None, Move, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

    Handle handleAt(const QPointF &pos) const;
    QRectF constrainRect(QRectF rect) const;
    // Corrects a resized rect to preserve m_dragStartRect's aspect ratio,
    // uniformly scaled up/down from whichever axis moved more, anchored at
    // the fixed point implied by `handle` (the opposite corner for a corner
    // handle, m_dragStartRect's center for an edge handle - a single edge
    // has no natural opposite point to anchor on).
    QRectF applyAspectLock(QRectF rect, Handle handle) const;

    static constexpr qreal kHandleSize = 24.0; // was 12 - too small a click/drag target
    static constexpr qreal kMinCropSize = 48.0; // pixels, in image space; scaled up alongside kHandleSize
                                                 // so the corner handles don't overlap badly at minimum size

    QRectF m_imageRect;
    QRectF m_cropRect;
    bool m_keepAspectRatio = false;

    Handle m_activeHandle = Handle::None;
    QPointF m_dragStartPos;
    QRectF m_dragStartRect;
};
