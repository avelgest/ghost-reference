#include "reference_titlebar.h"

#include <QtGui/QAction>
#include <QtGui/QPainter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>

#include "reference_window.h"
#include "settings_panel.h"

namespace
{
    const int titlebarHeight = 36;
} // namespace

ReferenceTitlebar::ReferenceTitlebar(ReferenceWindow *parent)
    : QToolBar(parent)
{
    // setAutoFillBackground(true);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy.setRetainSizeWhenHidden(true);
    setSizePolicy(sizePolicy);

    initActions();
}

void ReferenceTitlebar::showSettingsPanel() const
{
    if (referenceWindow())
    {
        referenceWindow()->showSettingsWindow();
    }
}

ReferenceWindow *ReferenceTitlebar::referenceWindow() const
{
    return qobject_cast<ReferenceWindow *>(parentWidget());
}

QSize ReferenceTitlebar::sizeHint() const
{
    return {0, titlebarHeight};
}

void ReferenceTitlebar::paintEvent([[maybe_unused]] QPaintEvent *event)
{
    const QColor backgroundColor = palette().color(QWidget::backgroundRole());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::GlobalColor::transparent));
    painter.setBrush(QBrush(backgroundColor));

    const int curveRadius = 8;
    painter.drawRoundedRect(rect(), curveRadius, curveRadius);
    painter.drawRect(rect().translated({0, curveRadius}));
}

void ReferenceTitlebar::initActions()
{
    QAction *showRefSettings = addAction("Settings");
    QObject::connect(showRefSettings, &QAction::triggered, this, &ReferenceTitlebar::showSettingsPanel);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    addWidget(spacer);

    QAction *minimizeWindow = addAction(style()->standardIcon(QStyle::SP_TitleBarMinButton),
                                        "Minimize Window");
    QObject::connect(minimizeWindow, &QAction::triggered, this, &ReferenceTitlebar::minimizeWindow);

    QAction *closeWindow = addAction(style()->standardIcon(QStyle::SP_TitleBarCloseButton),
                                     "Close Window");
    QObject::connect(closeWindow, &QAction::triggered, this, &ReferenceTitlebar::closeWindow);
}

void ReferenceTitlebar::closeWindow() const
{
    if (referenceWindow())
    {
        referenceWindow()->close();
    }
}

void ReferenceTitlebar::minimizeWindow() const
{
    if (referenceWindow())
    {
        referenceWindow()->hide();
    }
}
