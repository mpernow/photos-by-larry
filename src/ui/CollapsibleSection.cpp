#include "CollapsibleSection.h"

#include <QToolButton>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent), m_toggleButton(new QToolButton(this)), m_contentArea(new QWidget(this))
{
    m_toggleButton->setText(title);
    m_toggleButton->setCheckable(true);
    m_toggleButton->setChecked(true);
    m_toggleButton->setArrowType(Qt::DownArrow);
    m_toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // Flat + bold is just enough to read as a section header rather than a
    // regular button, without pulling in a whole custom-painted widget.
    m_toggleButton->setStyleSheet("QToolButton { border: none; font-weight: bold; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_toggleButton);
    layout->addWidget(m_contentArea);

    connect(m_toggleButton, &QToolButton::toggled, this, [this](bool expanded) {
        m_contentArea->setVisible(expanded);
        m_toggleButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    });
}

void CollapsibleSection::setContentLayout(QLayout *contentLayout)
{
    m_contentArea->setLayout(contentLayout);
}
