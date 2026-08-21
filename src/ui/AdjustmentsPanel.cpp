#include "AdjustmentsPanel.h"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>

namespace
{
constexpr int kContrastScale = 100; // slider value 100 <=> contrast 1.0
}

AdjustmentsPanel::AdjustmentsPanel(QWidget *parent)
    : QWidget(parent),
      m_brightnessSlider(new QSlider(Qt::Horizontal, this)),
      m_contrastSlider(new QSlider(Qt::Horizontal, this)),
      m_brightnessValueLabel(new QLabel(this)),
      m_contrastValueLabel(new QLabel(this))
{
    m_brightnessSlider->setRange(-100, 100);
    m_contrastSlider->setRange(0, 300);

    auto *layout = new QFormLayout(this);
    layout->addRow(tr("Brightness"), m_brightnessSlider);
    layout->addRow(QString(), m_brightnessValueLabel);
    layout->addRow(tr("Contrast"), m_contrastSlider);
    layout->addRow(QString(), m_contrastValueLabel);

    setParameters(EditParameters{});
    setEnabled(false); // no photo selected yet; QWidget::setEnabled disables the whole tree

    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int value) {
        m_brightnessValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_contrastValueLabel->setText(QString::number(value / double(kContrastScale), 'f', 2));
        emitPreview();
    });
    connect(m_brightnessSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_contrastSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
}

void AdjustmentsPanel::setParameters(const EditParameters &params)
{
    const QSignalBlocker blockBrightness(m_brightnessSlider);
    const QSignalBlocker blockContrast(m_contrastSlider);

    m_brightnessSlider->setValue(static_cast<int>(params.brightness));
    m_contrastSlider->setValue(static_cast<int>(params.contrast * kContrastScale));

    m_brightnessValueLabel->setText(QString::number(m_brightnessSlider->value()));
    m_contrastValueLabel->setText(QString::number(params.contrast, 'f', 2));
}

EditParameters AdjustmentsPanel::currentParameters() const
{
    EditParameters params;
    params.brightness = m_brightnessSlider->value();
    params.contrast = m_contrastSlider->value() / double(kContrastScale);
    return params;
}

void AdjustmentsPanel::emitPreview()
{
    emit previewParametersChanged(currentParameters());
}
