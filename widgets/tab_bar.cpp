#include "tab_bar.h"

#include <QtGui/QMouseEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

#include "../reference_image.h"
#include "../undo_stack.h"

#include "reference_window.h"

namespace
{
    QStyle *tabBarStyle()
    {
        static QStyle *style = nullptr;
        if (!style)
        {
            style = QStyleFactory::create("Fusion");
            if (!style)
            {
                style = QApplication::style();
            }
        }
        return style;
    }
} // namespace

TabBar::TabBar(ReferenceWindow *parent)
    : QTabBar(parent),
      m_parent(parent)
{
    setAutoHide(true);
    setElideMode(Qt::ElideRight);
    setMovable(true);
    setShape(QTabBar::TriangularSouth);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setTabsClosable(true);
    setUsesScrollButtons(false);

    setStyle(tabBarStyle());

    if (!parent)
    {
        qCritical("TabBar initialized without a parent");
        return;
    }

    for (const auto &refItem : parent->referenceImages())
    {
        onReferenceAdded(refItem);
    }

    QObject::connect(parent, &ReferenceWindow::activeImageChanged, this, &TabBar::onActiveImageChanged);
    QObject::connect(parent, &ReferenceWindow::referenceAdded, this, &TabBar::onReferenceAdded);
    QObject::connect(parent, &ReferenceWindow::referenceRemoved, this, &TabBar::onReferenceRemoved);
    QObject::connect(parent, &ReferenceWindow::windowModeChanged, this, &TabBar::onWindowModeChanged);
    QObject::connect(this, &TabBar::currentChanged, this, &TabBar::onCurrentChanged);
    QObject::connect(this, &TabBar::tabCloseRequested, this, &TabBar::onTabCloseRequested);
}

int TabBar::indexOf(const ReferenceImageSP &refItem)
{
    for (int i = 0; i < count(); i++)
    {
        if (tabData(i).value<ReferenceImageSP>() == refItem)
        {
            return i;
        }
    }
    return -1;
}

ReferenceImageSP TabBar::referenceAt(int index) const
{
    return tabData(index).value<ReferenceImageSP>();
}

void TabBar::onActiveImageChanged(const ReferenceImageSP &refItem)
{
    if (!refItem)
    {
        return;
    }
    const int idx = indexOf(refItem);
    if (idx < 0)
    {
        qCritical() << "activeImageChanged: No tab found for ReferenceImage" << refItem->name();
        return;
    }
    if (idx != currentIndex())
    {
        setCurrentIndex(idx);
    }
}

void TabBar::onReferenceAdded(const ReferenceImageSP &refItem)
{
    if (refItem)
    {
        const int idx = addTab(refItem->name());
        setTabData(idx, QVariant::fromValue(refItem));
        setTabToolTip(idx, refItem->name());

        setTabButton(idx, QTabBar::ButtonPosition::RightSide, createButtonWidget(refItem));

        // Update the tab text when refItem's name changes
        QObject::connect(refItem.get(), &ReferenceImage::nameChanged, this, [this, refItem]() {
            if (const int idx = indexOf(refItem); idx >= 0)
            {
                setTabText(idx, refItem->name());
            }
        });
    }
}

void TabBar::onReferenceRemoved(const ReferenceImageSP &refItem)
{
    const int idx = indexOf(refItem);
    if (idx < 0)
    {
        qCritical() << "referenceRemoved: No tab found for ReferenceImage" << refItem->name();
        return;
    }
    removeTab(idx);
    QObject::disconnect(refItem.get(), nullptr, this, nullptr);
}

void TabBar::onCurrentChanged(int index)
{
    const auto refItem = tabData(index).value<ReferenceImageSP>();
    if (refItem && m_parent)
    {
        m_parent->setActiveImage(refItem);
    }
}

void TabBar::onTabCloseRequested(int index) { removeReference(index); }

QWidget *TabBar::createButtonWidget(const ReferenceImageSP &refItem)
{
    auto *widget = new QWidget(this);
    auto *layout = new QHBoxLayout(widget);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const int buttonSize = style()->pixelMetric(QStyle::PM_TitleBarButtonSize);

    // TODO Refactor buttons into a separate class
    auto *detachBtn = new QPushButton(widget);
    detachBtn->setFlat(true);
    detachBtn->setFocusPolicy(Qt::NoFocus);
    detachBtn->setObjectName("detach-tab-btn");
    detachBtn->setToolTip("Detach tab");
    detachBtn->setMaximumWidth(buttonSize);
    layout->addWidget(detachBtn);

    auto *closeBtn = new QPushButton(widget);
    closeBtn->setFlat(true);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName("close-tab-btn");
    closeBtn->setMaximumWidth(buttonSize);
    closeBtn->setToolTip("Close tab");
    layout->addWidget(closeBtn);

    if (refItem.isNull())
    {
        detachBtn->setEnabled(false);
        return widget;
    }

    QObject::connect(closeBtn, &QPushButton::clicked, [this, refItem]() {
        UndoStack::get()->pushRefWindow(m_parent);
        removeTab(indexOf(refItem));
    });
    QObject::connect(detachBtn, &QPushButton::clicked, [this, refItem]() {
        if (m_parent)
        {
            UndoStack::get()->pushGlobalUndo();
            m_parent->detachReference(refItem);
        }
    });

    return widget;
}

void TabBar::removeReference(int index)
{
    const auto refItem = tabData(index).value<ReferenceImageSP>();
    if (refItem)
    {
        m_parent->removeReference(refItem);
    }
}

QSize TabBar::tabSizeHint(int index) const
{
    const int height = 24;
    return {QTabBar::tabSizeHint(index).width(), height};
}

void TabBar::hideEvent(QHideEvent *event)
{
    QTabBar::hideEvent(event);
    adjustParentSize();
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        if (const int tabIdx = tabAt(event->pos()); tabIdx >= 0)
        {
            removeReference(tabIdx);
            event->accept();
        }
    }
    if (!event->isAccepted())
    {
        QTabBar::mouseReleaseEvent(event);
    }
}

void TabBar::showEvent(QShowEvent *event)
{
    QTabBar::showEvent(event);
    adjustParentSize();
}

void TabBar::adjustParentSize()
{
    if (m_parent)
    {
        m_parent->adjustSize();
    }
}

void TabBar::onWindowModeChanged(WindowMode windowMode)
{
    // Hide this TabBar in GhostMode or when there are < 2 tabs
    bool visible = isVisible();
    if (windowMode == WindowMode::GhostMode)
    {
        visible = false;
    }
    else if (count() > 1)
    {
        visible = true;
    }

    if (visible != isVisible())
    {
        setVisible(visible);
        adjustParentSize();
    }
}
