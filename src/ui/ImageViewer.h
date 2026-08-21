#pragma once

#include <QGraphicsView>

class QGraphicsPixmapItem;

// Central panel: displays the currently rendered (original + edits) image,
// with pan/zoom. Knows nothing about photos, edits, or OpenCV - it just
// shows whatever QImage it's handed.
class ImageViewer : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void clear();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void fitToView();

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    bool m_hasImage = false;
    bool m_userAdjustedZoom = false; // once true, auto-fit no longer overrides the user's zoom
};
