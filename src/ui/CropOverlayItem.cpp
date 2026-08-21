#include "CropOverlayItem.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

CropOverlayItem::CropOverlayItem(QGraphicsItem *parent) : QGraphicsItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
}

void CropOverlayItem::setImageRect(const QRectF &imageRect)
{
    prepareGeometryChange();
    m_imageRect = imageRect;
}

void CropOverlayItem::setCropRect(const QRectF &rect)
{
    m_cropRect = rect;
    update();
}

QRectF CropOverlayItem::boundingRect() const
{
    // The crop rect can sit flush against any edge of the image (it starts
    // out exactly matching it), and handles are drawn centered on its
    // corners/edges - so they can protrude past the image bounds. Qt requires
    // paint() to stay within whatever this reports, so pad by the handle
    // radius to cover that.
    const qreal margin = kHandleSize / 2.0 + 1.0;
    return m_imageRect.adjusted(-margin, -margin, margin, margin);
}

void CropOverlayItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // Darken everything outside the crop rect.
    QPainterPath outside;
    outside.addRect(m_imageRect);
    QPainterPath inside;
    inside.addRect(m_cropRect);
    painter->fillPath(outside.subtracted(inside), QColor(0, 0, 0, 150));

    painter->setPen(QPen(Qt::white, 1.5));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_cropRect);

    // Rule-of-thirds guides, to help with composition while cropping.
    painter->setPen(QPen(QColor(255, 255, 255, 110), 1.0));
    for (int i = 1; i <= 2; ++i) {
        const qreal fx = m_cropRect.left() + m_cropRect.width() * i / 3.0;
        painter->drawLine(QPointF(fx, m_cropRect.top()), QPointF(fx, m_cropRect.bottom()));
        const qreal fy = m_cropRect.top() + m_cropRect.height() * i / 3.0;
        painter->drawLine(QPointF(m_cropRect.left(), fy), QPointF(m_cropRect.right(), fy));
    }

    // Handles at the corners and edge midpoints.
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::white);
    const QPointF anchors[] = {
        m_cropRect.topLeft(),     QPointF(m_cropRect.center().x(), m_cropRect.top()),
        m_cropRect.topRight(),    QPointF(m_cropRect.right(), m_cropRect.center().y()),
        m_cropRect.bottomRight(), QPointF(m_cropRect.center().x(), m_cropRect.bottom()),
        m_cropRect.bottomLeft(),  QPointF(m_cropRect.left(), m_cropRect.center().y()),
    };
    for (const QPointF &anchor : anchors) {
        painter->drawRect(
            QRectF(anchor.x() - kHandleSize / 2, anchor.y() - kHandleSize / 2, kHandleSize, kHandleSize));
    }
}

CropOverlayItem::Handle CropOverlayItem::handleAt(const QPointF &pos) const
{
    const auto near = [&](const QPointF &anchor) {
        return QRectF(anchor.x() - kHandleSize, anchor.y() - kHandleSize, 2 * kHandleSize, 2 * kHandleSize)
            .contains(pos);
    };

    const QRectF &r = m_cropRect;
    if (near(r.topLeft()))
        return Handle::TopLeft;
    if (near(r.topRight()))
        return Handle::TopRight;
    if (near(r.bottomLeft()))
        return Handle::BottomLeft;
    if (near(r.bottomRight()))
        return Handle::BottomRight;
    if (near(QPointF(r.center().x(), r.top())))
        return Handle::Top;
    if (near(QPointF(r.center().x(), r.bottom())))
        return Handle::Bottom;
    if (near(QPointF(r.left(), r.center().y())))
        return Handle::Left;
    if (near(QPointF(r.right(), r.center().y())))
        return Handle::Right;
    if (r.contains(pos))
        return Handle::Move;
    return Handle::None;
}

QRectF CropOverlayItem::constrainRect(QRectF rect) const
{
    rect = rect.normalized();

    // Never let a resize shrink the selection into a sliver. Re-centering on
    // the affected axis is simpler than tracking which edge is "anchored" per
    // handle, at the cost of a small jump right at the minimum-size limit.
    if (rect.width() < kMinCropSize) {
        const qreal cx = rect.center().x();
        rect.setLeft(cx - kMinCropSize / 2);
        rect.setWidth(kMinCropSize);
    }
    if (rect.height() < kMinCropSize) {
        const qreal cy = rect.center().y();
        rect.setTop(cy - kMinCropSize / 2);
        rect.setHeight(kMinCropSize);
    }

    // Keep it fully within the image bounds.
    if (rect.left() < m_imageRect.left())
        rect.moveLeft(m_imageRect.left());
    if (rect.top() < m_imageRect.top())
        rect.moveTop(m_imageRect.top());
    if (rect.right() > m_imageRect.right())
        rect.moveRight(m_imageRect.right());
    if (rect.bottom() > m_imageRect.bottom())
        rect.moveBottom(m_imageRect.bottom());

    return rect;
}

QRectF CropOverlayItem::applyAspectLock(QRectF rect, Handle handle) const
{
    if (m_dragStartRect.width() <= 0 || m_dragStartRect.height() <= 0)
        return rect; // nothing sane to lock onto

    // Uniform scale from the drag's starting size preserves its ratio
    // exactly; pick whichever axis moved more (proportionally) as the one
    // the user actually meant to drive, and scale both by that factor.
    const qreal scaleW = rect.width() / m_dragStartRect.width();
    const qreal scaleH = rect.height() / m_dragStartRect.height();
    const qreal scale = std::abs(scaleW - 1.0) >= std::abs(scaleH - 1.0) ? scaleW : scaleH;

    const qreal newWidth = m_dragStartRect.width() * scale;
    const qreal newHeight = m_dragStartRect.height() * scale;

    switch (handle) {
    case Handle::TopLeft: {
        const QPointF anchor = rect.bottomRight(); // opposite corner, unmoved by the raw drag above
        return QRectF(anchor.x() - newWidth, anchor.y() - newHeight, newWidth, newHeight);
    }
    case Handle::TopRight: {
        const QPointF anchor = rect.bottomLeft();
        return QRectF(anchor.x(), anchor.y() - newHeight, newWidth, newHeight);
    }
    case Handle::BottomLeft: {
        const QPointF anchor = rect.topRight();
        return QRectF(anchor.x() - newWidth, anchor.y(), newWidth, newHeight);
    }
    case Handle::BottomRight: {
        const QPointF anchor = rect.topLeft();
        return QRectF(anchor.x(), anchor.y(), newWidth, newHeight);
    }
    case Handle::Top:
    case Handle::Bottom:
    case Handle::Left:
    case Handle::Right: {
        // A single edge has no natural opposite point to hold fixed, so
        // scale symmetrically about the drag's starting center instead.
        const QPointF center = m_dragStartRect.center();
        return QRectF(center.x() - newWidth / 2.0, center.y() - newHeight / 2.0, newWidth, newHeight);
    }
    case Handle::Move:
    case Handle::None:
        return rect;
    }
    return rect;
}

void CropOverlayItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_activeHandle = handleAt(event->pos());
    if (m_activeHandle == Handle::None) {
        event->ignore(); // let clicks outside the crop rect fall through
        return;
    }
    m_dragStartPos = event->pos();
    m_dragStartRect = m_cropRect;
    event->accept();
}

void CropOverlayItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == Handle::None)
        return;

    const QPointF delta = event->pos() - m_dragStartPos;
    QRectF rect = m_dragStartRect;

    switch (m_activeHandle) {
    case Handle::Move:
        rect.translate(delta);
        break;
    case Handle::TopLeft:
        rect.setTopLeft(rect.topLeft() + delta);
        break;
    case Handle::Top:
        rect.setTop(rect.top() + delta.y());
        break;
    case Handle::TopRight:
        rect.setTopRight(rect.topRight() + delta);
        break;
    case Handle::Right:
        rect.setRight(rect.right() + delta.x());
        break;
    case Handle::BottomRight:
        rect.setBottomRight(rect.bottomRight() + delta);
        break;
    case Handle::Bottom:
        rect.setBottom(rect.bottom() + delta.y());
        break;
    case Handle::BottomLeft:
        rect.setBottomLeft(rect.bottomLeft() + delta);
        break;
    case Handle::Left:
        rect.setLeft(rect.left() + delta.x());
        break;
    case Handle::None:
        break;
    }

    if (m_keepAspectRatio && m_activeHandle != Handle::Move)
        rect = applyAspectLock(rect, m_activeHandle);

    // constrainRect()'s minimum-size step is a no-op here since translating
    // doesn't change the rect's size, so it's safe to reuse for Move too.
    setCropRect(constrainRect(rect));
}

void CropOverlayItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    m_activeHandle = Handle::None;
}
