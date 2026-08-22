#pragma once

#include <QWidget>

class QToolButton;

// A titled section that can be collapsed to hide its content and reclaim
// vertical space. Used by AdjustmentsPanel to group its sliders into
// Geometry/Light/Color so the panel doesn't show every control at once.
//
// Expanded by default; collapse state is per-instance UI state only, not
// persisted anywhere (resets to expanded on next launch).
class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);

    // Takes ownership of contentLayout, installing it as this section's
    // content area. Call once, after populating it with the section's child
    // widgets - there's no support for replacing it afterward.
    void setContentLayout(QLayout *contentLayout);

private:
    QToolButton *m_toggleButton;
    QWidget *m_contentArea;
};
