#pragma once

#include <QWidget>

#include "core/EditParameters.h"

class QSlider;
class QLabel;

// Right-hand dock panel with the adjustment sliders for the currently
// selected photo. Emits a live preview signal on every slider move, and a
// separate "committed" signal only once the user releases the slider - the
// former drives the on-screen render, the latter is what gets persisted.
class AdjustmentsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AdjustmentsPanel(QWidget *parent = nullptr);

    void setParameters(const EditParameters &params);

signals:
    void previewParametersChanged(const EditParameters &params);
    void parametersCommitted(const EditParameters &params);

private:
    EditParameters currentParameters() const;
    void emitPreview();

    QSlider *m_brightnessSlider;
    QSlider *m_contrastSlider;
    QLabel *m_brightnessValueLabel;
    QLabel *m_contrastValueLabel;
};
