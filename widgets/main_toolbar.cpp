#include "main_toolbar.h"

#include <ranges>

#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPointer>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QTimer>

#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPen>

#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QToolButton>

#include "../app.h"
#include "../preferences.h"
#include "../reference_image.h"
#include "../reference_loading.h"
#include "../saving.h"

#include "back_window.h"
#include "main_toolbar_actions.h"
#include "reference_window.h"

namespace
{
    const Qt::WindowFlags initialWindowFlags = Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint;
    const QSize buttonSize(36, 36);
    const qreal cornerRadius = 4.;
    const QMargins toolbarMargins(0, 2, 2, 2);

    // Time to wait before the toolbar shrinks when in ghost mode
    const int fadeTimerDelayMs = 1500;

    const int expandAnimMs = 500;
    const int fadeAnimMs = 1500;
    const qreal fadedOpacity = 0.5;

    class DragWidget : public QWidget
    {
        Q_DISABLE_COPY_MOVE(DragWidget);
        static const int s_width = 24;

        QPoint m_lastMousePos;
        QPointer<QWidget> m_target;

    public:
        explicit DragWidget(MainToolbar *parent, QWidget *target = nullptr)
            : QWidget(parent),
              m_target(target ? target : parent)
        {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            setCursor(Qt::SizeAllCursor);
        };
        ~DragWidget() override = default;

        QSize sizeHint() const override { return {s_width, buttonSize.height()}; }

    protected:
        void mousePressEvent(QMouseEvent *event) override
        {
            if (event->button() == Qt::LeftButton)
            {
                m_lastMousePos = event->globalPos();
            }
        };

        void mouseMoveEvent(QMouseEvent *event) override
        {
            if (m_target && event->buttons() & Qt::LeftButton)
            {
                const QPoint diff = event->globalPos() - m_lastMousePos;
                m_lastMousePos = event->globalPos();

                m_target->move(m_target->pos() + diff);

                event->accept();
            }
        }

        void paintEvent([[maybe_unused]] QPaintEvent *event) override
        {
            const QSize margin(8, 4);
            const QColor dotColor(196, 196, 196);

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);

            QPen pen(Qt::DotLine);
            pen.setColor(dotColor);
            pen.setWidth(2);
            painter.setPen(pen);

            // The start and end points of the first dotted line
            const QPoint lineStart(margin.width(), margin.height());
            const QPoint lineEnd(lineStart.x(), 48 - margin.height());

            painter.drawLine(lineStart, lineEnd);
            painter.drawLine(lineStart.x() + 4, lineStart.y(), lineStart.x() + 4, lineEnd.y());
        }
    };

    class ToolbarSeparator : public QWidget
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(ToolbarSeparator)

    private:
        QAction *m_action = nullptr;

    public:
        ToolbarSeparator(QAction *action, QWidget *parent)
            : QWidget(parent),
              m_action(action)
        {
            setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        }
        ~ToolbarSeparator() override = default;

        QAction *action() const { return m_action; }

        QSize sizeHint() const override
        {
            const QStyleOption opt = styleOptions();
            const int extent = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent, &opt, parentWidget());
            return {extent, extent};
        }

    protected:
        QStyleOption styleOptions() const
        {
            QStyleOption opt;
            opt.initFrom(this);
            opt.state.setFlag(QStyle::State_Horizontal, true);
            return opt;
        }

        void paintEvent([[maybe_unused]] QPaintEvent *event) override
        {
            QPainter painter(this);
            const QStyleOption opt = styleOptions();
            style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &opt, &painter, parentWidget());
        }
    };

    QAction *getWidgetAction(const QWidget *widget)
    {
        if (const auto *separator = qobject_cast<const ToolbarSeparator *>(widget); separator)
        {
            return separator->action();
        }
        if (const auto *toolBtn = qobject_cast<const QToolButton *>(widget); toolBtn)
        {
            return toolBtn->defaultAction();
        }
        return nullptr;
    }

    QToolButton *createToolButton(QAction *action, MainToolbar *parent = nullptr)
    {
        auto *button = new QToolButton(parent);
        button->setAutoRaise(true);
        button->setDefaultAction(action);
        // button->setFixedSize(buttonSize);
        button->setIconSize(buttonSize);
        return button;
    }

    int getCollapsedWidth(const MainToolbar &toolbar)
    {
        QLayoutItem *rightItem = toolbar.layout()->itemAt(3);
        return rightItem->geometry().left();
    }

    QAbstractAnimation *createCollapseAnim(MainToolbar &toolbar)
    {
        QScopedPointer<QSequentialAnimationGroup> animGroup(new QSequentialAnimationGroup(&toolbar));
        auto *maskAnim = new QVariantAnimation(animGroup.get());
        auto *fadeAnim = new QPropertyAnimation(&toolbar, "opacity", animGroup.get());

        maskAnim->setKeyValueAt(0., toolbar.width());
        maskAnim->setKeyValueAt(1., getCollapsedWidth(toolbar));
        maskAnim->setEasingCurve(QEasingCurve::Linear);
        maskAnim->setDuration(expandAnimMs);

        QObject::connect(maskAnim, &QVariantAnimation::valueChanged,
                         [&toolbar](const QVariant &value)
                         {
                             toolbar.setMask(QRect(0, 0, value.toInt(), toolbar.height()));
                             toolbar.update();
                         });

        fadeAnim->setKeyValueAt(0., toolbar.opacity());
        fadeAnim->setKeyValueAt(1., fadedOpacity);
        fadeAnim->setDuration(fadeAnimMs);

        animGroup->addAnimation(maskAnim);
        animGroup->addAnimation(fadeAnim);
        return animGroup.take();
    }

    QAbstractAnimation *createExpandAnim(MainToolbar &toolbar)
    {
        QScopedPointer<QParallelAnimationGroup> animGroup(new QParallelAnimationGroup(&toolbar));
        auto *maskAnim = new QVariantAnimation(animGroup.get());
        auto *fadeAnim = new QPropertyAnimation(&toolbar, "opacity", animGroup.get());

        maskAnim->setKeyValueAt(0., toolbar.mask().boundingRect().width());
        maskAnim->setKeyValueAt(1., toolbar.width());
        maskAnim->setEasingCurve(QEasingCurve::InOutQuad);
        maskAnim->setDuration(expandAnimMs);

        QObject::connect(maskAnim, &QVariantAnimation::valueChanged,
                         [&toolbar](const QVariant &value)
                         {
                             toolbar.setMask(QRect(0, 0, value.toInt(), toolbar.height()));
                             toolbar.update();
                         });

        fadeAnim->setKeyValueAt(0., toolbar.opacity());
        fadeAnim->setKeyValueAt(1., 1.0);
        fadeAnim->setDuration(expandAnimMs);
        fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

        animGroup->addAnimation(maskAnim);
        animGroup->addAnimation(fadeAnim);
        return animGroup.take();
    }

    // Adds a menu to the last added QToolButton of mainToolbar.
    void addButtonMenu(MainToolbar *mainToolbar, QMenu *menu)
    {
        for (const auto &child : std::views::reverse(mainToolbar->children()))
        {
            if (child->inherits("QToolButton"))
            {
                auto *toolButton = qobject_cast<QToolButton *>(child);
                toolButton->setMenu(menu);
                toolButton->setPopupMode(QToolButton::MenuButtonPopup);
                return;
            }
        }
        qCritical() << "Could not add button menu. No QToolButton found.";
    }

    QMenu *createSaveButtonMenu(MainToolbar *parent)
    {
        auto *menu = new QMenu(parent);
        menu->addAction(&parent->mainToolbarActions()->saveSessionAs());
        return menu;
    }

    QMenu *createHideButtonMenu(MainToolbar *parent)
    {
        auto *menu = new QMenu(parent);

        QObject::connect(menu, &QMenu::aboutToShow, [menu]() {
            App *app = App::ghostRefInstance();
            menu->clear();

            for (const auto &refWindow : app->referenceWindows())
            {
                if (refWindow && !refWindow->isVisible())
                {
                    const QString name = refWindow->activeImage() ? refWindow->activeImage()->name() : "No Image";
                    QAction *action = menu->addAction(name);
                    QObject::connect(action, &QAction::triggered, refWindow,
                                     [refWindow]() { refWindow->setVisible(true); });
                }
            }
        });
        return menu;
    }

} // namespace

class MainToolbar::FadeStartTimer : protected QTimer
{
    Q_DISABLE_COPY_MOVE(FadeStartTimer)

    MainToolbar *m_mainToolbar;

public:
    explicit FadeStartTimer(MainToolbar *parent)
        : QTimer(parent),
          m_mainToolbar(parent)
    {
        setSingleShot(true);
    }
    ~FadeStartTimer() override = default;

    void ensureStarted()
    {
        if (!isActive())
        {
            start(fadeTimerDelayMs);
        }
    }

    void stop() { QTimer::stop(); }

protected:
    void timerEvent([[maybe_unused]] QTimerEvent *e) override
    {
        if (App::ghostRefInstance()->globalMode() == WindowMode::GhostMode)
        {
            m_mainToolbar->setExpanded(false);
        }
    }
};

MainToolbar::MainToolbar(QWidget *parent)
    : QWidget(parent),
      m_dragWidget(new DragWidget(this, this)),
      m_fadeStartTimer(new FadeStartTimer(this)),
      m_graphicsEffect(new GraphicsEffect(this))
{
    m_windowActions.reset(new MainToolbarActions(this));

    setAcceptDrops(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setWindowFlags(initialWindowFlags);

    m_graphicsEffect->setOpacity(1.0);
    setGraphicsEffect(m_graphicsEffect);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(toolbarMargins);

    layout->addWidget(m_dragWidget);

    addSeparator();

    // toggleGhostMode
    {
        auto *btn = qobject_cast<QToolButton *>(addWidgetFor(&m_windowActions->toggleGhostMode()));
        Q_ASSERT(btn);
        btn->setObjectName("toggle-ghost-mode-btn");
    }

    addAction(&m_windowActions->toggleAllRefsHidden());
    addButtonMenu(this, createHideButtonMenu(this));
    addSeparator();

    addAction(&m_windowActions->openSession());
    addAction(&m_windowActions->saveSession());
    addButtonMenu(this, createSaveButtonMenu(this));
    addSeparator();

    addAction(&m_windowActions->showPreferences());
    addAction(&m_windowActions->showHelp());
    addSeparator();

    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        addAction(&m_windowActions->minimizeToolbar());
    }
    addAction(&m_windowActions->closeApplication());
}

BackWindow *MainToolbar::backWindow()
{
    return App::ghostRefInstance()->backWindow();
}

MainToolbarActions *MainToolbar::mainToolbarActions()
{
    return m_windowActions.get();
}

void MainToolbar::setExpanded(bool value)
{
    if (m_expanded == value)
    {
        return;
    }

    m_expanded = value;
    if (appPrefs()->getBool(Preferences::AnimateToolbarCollapse))
    {
        setAnim((value) ? createExpandAnim(*this) : createCollapseAnim(*this));
        anim()->start();
    }
    else
    {
        const QRect mask = (value) ? rect() : QRect(0, 0, getCollapsedWidth(*this), height());
        setMask(mask);
    }
}

qreal MainToolbar::opacity() const { return m_graphicsEffect->opacity(); }

void MainToolbar::setOpacity(qreal value)
{
    m_graphicsEffect->setOpacity(value);
    update();
}

QSize MainToolbar::sizeHint() const
{
    return {1, buttonSize.height()};
}

void MainToolbar::actionEvent(QActionEvent *event)
{
    QWidget::actionEvent(event);

    QAction *action = event->action();

    switch (event->type())
    {
    case QEvent::ActionAdded:
        addWidgetFor(action);
        break;
    case QEvent::ActionRemoved:
        removeWidgetFor(action);
        break;
    default:
        break;
    }
}

void MainToolbar::contextMenuEvent(QContextMenuEvent *event)
{
    QAction toggleExpanded("Toggle Expanded");
    QObject::connect(&toggleExpanded, &QAction::triggered, [this]()
                     { setExpanded(!expanded()); });

    QMenu menu;
    menu.addAction(&m_windowActions->paste());
    menu.addAction(&toggleExpanded);
    menu.exec(event->globalPos());
}

void MainToolbar::dragEnterEvent(QDragEnterEvent *event)
{
    event->setAccepted(refLoad::isSupported(event) || sessionSaving::isSessionFile(event));
}

void MainToolbar::dropEvent(QDropEvent *event)
{
    if (refLoad::isSupported(event))
    {
        newReferenceWindow(refLoad::fromDropEvent(event));
    }
    else if (sessionSaving::isSessionFile(event))
    {
        if (const QString filePath = sessionSaving::getSessionFilePath(event); !filePath.isEmpty())
        {
            App::ghostRefInstance()->loadSession(filePath);
        }
    }
}

void MainToolbar::enterEvent([[maybe_unused]] QEnterEvent *event)
{
    setExpanded(true);
    m_fadeStartTimer->stop();
}

void MainToolbar::leaveEvent([[maybe_unused]] QEvent *event)
{
    if (App::ghostRefInstance()->globalMode() == WindowMode::GhostMode)
    {
        m_fadeStartTimer->ensureStarted();
    }
}

void MainToolbar::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPen pen(palette().color(QPalette::Dark));
        pen.setWidth(1);

        painter.setBrush(palette().color(QPalette::Window));
        painter.setPen(pen);

        const QRect drawRect = mask().isNull() ? rect() : mask().boundingRect();

        painter.drawRoundedRect(drawRect, cornerRadius, cornerRadius);
    }
    QWidget::paintEvent(event);
}

ReferenceWindow *MainToolbar::newReferenceWindow(const ReferenceImageSP &refItem)
{
    ReferenceWindow *ref_win = App::ghostRefInstance()->newReferenceWindow();
    ref_win->addReference(refItem, true);
    ref_win->setActiveImage(refItem);
    ref_win->show();
    return ref_win;
}

ReferenceWindow *MainToolbar::newReferenceWindow(const QList<ReferenceImageSP> &loadResults)
{
    ReferenceWindow *refWindow = nullptr;
    for (const auto &result : loadResults)
    {
        if (result)
        {
            if (!refWindow)
            {
                refWindow = newReferenceWindow(result);
            }
            else
            {
                refWindow->addReference(result, true);
            }
        }
        else
        {
            qCritical() << "Null ReferenceImageSP given";
        }
    }
    if (!refWindow)
    {
        // TODO Show message box
    }
    return refWindow;
}

QAction *MainToolbar::addSeparator()
{
    auto *action = new QAction(this);
    action->setSeparator(true);
    addAction(action);
    return action;
}

QWidget *MainToolbar::addWidgetFor(QAction *action)
{
    QWidget *widget = nullptr;
    if (action->isSeparator())
    {
        widget = new ToolbarSeparator(action, this);
    }
    else
    {
        widget = createToolButton(action, this);
    }
    layout()->addWidget(widget);

    return widget;
}

bool MainToolbar::removeWidgetFor(const QAction *action)
{
    for (int i = 0; i < layout()->count(); i++)
    {
        QLayoutItem *item = layout()->itemAt(i);
        QWidget *widget = item->widget();
        if (getWidgetAction(widget) == action)
        {
            layout()->removeItem(item);
            widget->deleteLater();
            return true;
        }
    }
    return false;
}

void MainToolbar::setAnim(QAbstractAnimation *anim)
{
    m_anim.reset(anim);
}

#include "main_toolbar.moc"
