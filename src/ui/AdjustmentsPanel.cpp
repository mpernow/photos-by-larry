#include "AdjustmentsPanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace
{
constexpr int kContrastScale = 100; // slider value 100 <=> contrast 1.0
}

AdjustmentsPanel::AdjustmentsPanel(QWidget *parent)
    : QWidget(parent),
      m_brightnessSlider(new QSlider(Qt::Horizontal, this)),
      m_contrastSlider(new QSlider(Qt::Horizontal, this)),
      m_temperatureSlider(new QSlider(Qt::Horizontal, this)),
      m_tintSlider(new QSlider(Qt::Horizontal, this)),
      m_brightnessValueLabel(new QLabel(this)),
      m_contrastValueLabel(new QLabel(this)),
      m_temperatureValueLabel(new QLabel(this)),
      m_tintValueLabel(new QLabel(this)),
      m_rotateLeftButton(new QPushButton(tr("Rotate Left"), this)),
      m_rotateRightButton(new QPushButton(tr("Rotate Right"), this)),
      m_cropButton(new QPushButton(tr("Crop"), this)),
      m_cropApplyButton(new QPushButton(tr("Apply"), this)),
      m_cropCancelButton(new QPushButton(tr("Cancel"), this)),
      m_keepAspectRatioCheckBox(new QCheckBox(tr("Keep aspect ratio"), this))
{
    m_brightnessSlider->setRange(-100, 100);
    m_contrastSlider->setRange(0, 300);
    m_temperatureSlider->setRange(-100, 100);
    m_tintSlider->setRange(-100, 100);

    auto *sliderLayout = new QFormLayout;
    sliderLayout->addRow(tr("Brightness"), m_brightnessSlider);
    sliderLayout->addRow(QString(), m_brightnessValueLabel);
    sliderLayout->addRow(tr("Contrast"), m_contrastSlider);
    sliderLayout->addRow(QString(), m_contrastValueLabel);
    sliderLayout->addRow(tr("Temperature"), m_temperatureSlider);
    sliderLayout->addRow(QString(), m_temperatureValueLabel);
    sliderLayout->addRow(tr("Tint"), m_tintSlider);
    sliderLayout->addRow(QString(), m_tintValueLabel);

    auto *rotateLayout = new QHBoxLayout;
    rotateLayout->addWidget(m_rotateLeftButton);
    rotateLayout->addWidget(m_rotateRightButton);

    m_cropApplyButton->setVisible(false);
    m_cropCancelButton->setVisible(false);
    m_keepAspectRatioCheckBox->setVisible(false);
    auto *cropLayout = new QHBoxLayout;
    cropLayout->addWidget(m_cropButton);
    cropLayout->addWidget(m_cropApplyButton);
    cropLayout->addWidget(m_cropCancelButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(sliderLayout);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(new QLabel(tr("Rotate"), this));
    mainLayout->addLayout(rotateLayout);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(new QLabel(tr("Crop"), this));
    mainLayout->addLayout(cropLayout);
    mainLayout->addWidget(m_keepAspectRatioCheckBox);
    mainLayout->addStretch(1);

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
    connect(m_temperatureSlider, &QSlider::valueChanged, this, [this](int value) {
        m_temperatureValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_tintSlider, &QSlider::valueChanged, this, [this](int value) {
        m_tintValueLabel->setText(QString::number(value));
        emitPreview();
    });
    connect(m_brightnessSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_contrastSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_temperatureSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });
    connect(m_tintSlider, &QSlider::sliderReleased, this,
            [this]() { emit parametersCommitted(currentParameters()); });

    connect(m_rotateLeftButton, &QPushButton::clicked, this,
            &AdjustmentsPanel::rotateCounterClockwiseRequested);
    connect(m_rotateRightButton, &QPushButton::clicked, this, &AdjustmentsPanel::rotateClockwiseRequested);

    connect(m_cropButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropRequested);
    connect(m_cropApplyButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropApplyRequested);
    connect(m_cropCancelButton, &QPushButton::clicked, this, &AdjustmentsPanel::cropCancelRequested);

    connect(m_keepAspectRatioCheckBox, &QCheckBox::toggled, this, &AdjustmentsPanel::keepAspectRatioToggled);
}

void AdjustmentsPanel::setParameters(const EditParameters &params)
{
    // currentParameters() below needs to carry rotation/crop forward even
    // though those don't have sliders - otherwise committing a slider change
    // would silently reset any existing rotate/crop.
    m_baseParameters = params;

    const QSignalBlocker blockBrightness(m_brightnessSlider);
    const QSignalBlocker blockContrast(m_contrastSlider);
    const QSignalBlocker blockTemperature(m_temperatureSlider);
    const QSignalBlocker blockTint(m_tintSlider);

    m_brightnessSlider->setValue(static_cast<int>(params.brightness));
    m_contrastSlider->setValue(static_cast<int>(params.contrast * kContrastScale));
    m_temperatureSlider->setValue(static_cast<int>(params.temperature));
    m_tintSlider->setValue(static_cast<int>(params.tint));

    m_brightnessValueLabel->setText(QString::number(m_brightnessSlider->value()));
    m_contrastValueLabel->setText(QString::number(params.contrast, 'f', 2));
    m_temperatureValueLabel->setText(QString::number(m_temperatureSlider->value()));
    m_tintValueLabel->setText(QString::number(m_tintSlider->value()));
}

void AdjustmentsPanel::setCropModeActive(bool active)
{
    m_cropButton->setVisible(!active);
    m_cropApplyButton->setVisible(active);
    m_cropCancelButton->setVisible(active);
    m_keepAspectRatioCheckBox->setVisible(active);

    // Adjusting any slider or rotating mid-crop would re-render the
    // background from under the overlay the user is actively positioning
    // (or, for rotate, change the image's dimensions entirely), so lock
    // those out until they're done.
    m_brightnessSlider->setEnabled(!active);
    m_contrastSlider->setEnabled(!active);
    m_temperatureSlider->setEnabled(!active);
    m_tintSlider->setEnabled(!active);
    m_rotateLeftButton->setEnabled(!active);
    m_rotateRightButton->setEnabled(!active);
}

EditParameters AdjustmentsPanel::currentParameters() const
{
    EditParameters params = m_baseParameters;
    params.brightness = m_brightnessSlider->value();
    params.contrast = m_contrastSlider->value() / double(kContrastScale);
    params.temperature = m_temperatureSlider->value();
    params.tint = m_tintSlider->value();
    return params;
}

void AdjustmentsPanel::emitPreview()
{
    emit previewParametersChanged(currentParameters());
}
