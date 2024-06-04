#include "reference_window.h"

#include <QtCore/Qt>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTabBar>

#include "../app.h"
#include "../preferences.h"
#include "../reference_collection.h"
#include "../reference_image.h"
#include "../reference_loading.h"

#include "picture_widget.h"
#include "resize_frame.h"
#include "settings_panel.h"

#include <QtWidgets/QStyle>

#include <QtWidgets/QPushButton>

namespace
{

    const int marginSize = 10;

    class TabBar : public QTabBar
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(TabBar)

        ReferenceWindow *m_parent;

    public:
        explicit TabBar(ReferenceWindow *parent)
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

            if (!parent)
            {
                qCritical("TabBar initialized without a parent");
                return;
            }

            for (const auto &refItem : parent->referenceImages())
            {
                referenceAdded(refItem);
            }

            QObject::connect(parent, &ReferenceWindow::activeImageChanged, this, &TabBar::activeImageChanged);
            QObject::connect(parent, &ReferenceWindow::referenceAdded, this, &TabBar::referenceAdded);
            QObject::connect(parent, &ReferenceWindow::referenceRemoved, this, &TabBar::referenceRemoved);
            QObject::connect(this, &TabBar::currentChanged, this, &TabBar::onCurrentChanged);
            QObject::connect(this, &TabBar::tabCloseRequested, this, &TabBar::onTabCloseRequested);
        }
        ~TabBar() override = default;

        int indexOf(const ReferenceImageSP &refItem)
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

        ReferenceImageSP referenceAt(int index) const
        {
            return tabData(index).value<ReferenceImageSP>();
        }

    protected slots:
        void activeImageChanged(const ReferenceImageSP &refItem)
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

        void referenceAdded(const ReferenceImageSP &refItem)
        {
            if (refItem)
            {

                const int idx = addTab(refItem->name());
                setTabData(idx, QVariant::fromValue(refItem));
                setTabToolTip(idx, refItem->name());

                setTabButton(idx, QTabBar::ButtonPosition::RightSide, createButtonWidget(refItem));
            }
        }

        void referenceRemoved(const ReferenceImageSP &refItem)
        {
            const int idx = indexOf(refItem);
            if (idx < 0)
            {
                qCritical() << "referenceRemoved: No tab found for ReferenceImage" << refItem->name();
                return;
            }
            removeTab(idx);
        }

        void onCurrentChanged(int index)
        {
            const auto refItem = tabData(index).value<ReferenceImageSP>();
            if (refItem && m_parent)
            {
                m_parent->setActiveImage(refItem);
            }
        }

        void onTabCloseRequested(int index) { removeReference(index); }

        void adjustParentSize()
        {
            if (m_parent)
            {
                m_parent->adjustSize();
            }
        }

        QSize tabSizeHint(int index) const override
        {
            const int height = 24;
            return {QTabBar::tabSizeHint(index).width(), height};
        }

        void removeReference(int index)
        {
            const auto refItem = tabData(index).value<ReferenceImageSP>();
            if (refItem)
            {
                m_parent->removeReference(refItem);
            }
        }

        void hideEvent(QHideEvent *event) override
        {
            QTabBar::hideEvent(event);
            adjustParentSize();
        }

        void mouseReleaseEvent(QMouseEvent *event) override
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

        void showEvent(QShowEvent *event) override
        {
            QTabBar::showEvent(event);
            adjustParentSize();
        }

        QWidget *createButtonWidget(const ReferenceImageSP &refItem)
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

            QObject::connect(closeBtn, &QPushButton::clicked, [this, refItem]()
                             { removeTab(indexOf(refItem)); });
            QObject::connect(detachBtn, &QPushButton::clicked, [this, refItem]()
                             { if (m_parent) m_parent->detachReference(refItem); });

            return widget;
        }
    };

    class Overlay : public QWidget
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(Overlay)
    private:
        bool m_mergeRequested = false;

    public:
        explicit Overlay(ReferenceWindow *parent)
            : QWidget(parent)
        {
            setAttribute(Qt::WA_TransparentForMouseEvents);
            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

            QObject::connect(parent, &ReferenceWindow::mergeRequested, this, &Overlay::onMergeRequested);
        }
        ~Overlay() override = default;

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            QPainter painter(this);
            if (m_mergeRequested)
            {
                const QColor mergeColor(0, 0, 255, 96);
                painter.fillRect(event->rect(), mergeColor);
            }
        };

    private:
        void onMergeRequested(ReferenceWindow *requester)
        {
            m_mergeRequested = (requester != nullptr);
            update();
        }
    };

    template <typename T>
    T flipMargins(const T &margins, bool horizontal, bool vertical)
    {
        return {horizontal ? margins.right() : margins.left(),
                vertical ? margins.bottom() : margins.top(),
                horizontal ? margins.left() : margins.right(),
                vertical ? margins.top() : margins.bottom()};
    }

    // Unused
    constexpr qreal minMagnitude(qreal value, qreal minMag)
    {
        return (std::abs(value) < minMag && std::abs(value) > DBL_EPSILON)
                   ? std::copysign(minMag, value)
                   : value;
    }

    // Unused
    constexpr QMarginsF minMagnitude(const QMarginsF &margins, qreal minMag)
    {
        return {minMagnitude(margins.left(), minMag),
                minMagnitude(margins.top(), minMag),
                minMagnitude(margins.right(), minMag),
                minMagnitude(margins.bottom(), minMag)};
    }

    // Linear interpolation
    float lerp(float a, float b, float t)
    {
        return a * (1 - t) + b * t;
    }

    QColor lerp(const QColor &a, const QColor &b, float t)
    {
        return QColor::fromRgbF(lerp(a.redF(), b.redF(), t),
                                lerp(a.greenF(), b.greenF(), t),
                                lerp(a.blueF(), b.blueF(), t),
                                lerp(a.alphaF(), b.alphaF(), t));
    }

    bool windowsShouldMerge(const ReferenceWindow *refWindow, const ReferenceWindow *mergeInto)
    {
        const int distThreshold = 100; // Merge distance threshold (pixels)
        const QPoint mergeIntoCenter = mergeInto->mapToGlobal(mergeInto->rect().center());
        const QPoint diff = mergeIntoCenter - QCursor::pos(refWindow->screen());

        const int distance2 = diff.x() * diff.x() + diff.y() * diff.y();
        return distance2 < (distThreshold * distThreshold);
    }

} // namespace

ReferenceWindow::ReferenceWindow(QWidget *parent)
    : QWidget(parent),
      m_pictureWidget(new PictureWidget(this)),
      m_tabBar(new TabBar(this)),
      m_overlay(new Overlay(this))
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(marginSize, marginSize, marginSize, marginSize);
    grid->setSpacing(0);
    grid->addWidget(m_pictureWidget, 1, 0);
    grid->addWidget(m_overlay, 1, 0);
    grid->addWidget(m_tabBar, 2, 0);

    ResizeFrame *resizeFrame = m_pictureWidget->resizeFrame();

    QObject::connect(resizeFrame, &ResizeFrame::moved, this, &ReferenceWindow::onFrameMove);
    QObject::connect(resizeFrame, &ResizeFrame::cropped, this, &ReferenceWindow::onFrameCrop);
    QObject::connect(resizeFrame, &ResizeFrame::resized, this, &ReferenceWindow::onFrameResize);
    QObject::connect(resizeFrame, &ResizeFrame::viewMoved, this, &ReferenceWindow::onFrameViewMoved);
    QObject::connect(resizeFrame, &ResizeFrame::transformFinished, this, &ReferenceWindow::onTransformFinished);

    QObject::connect(App::ghostRefInstance(), &App::focusChanged, this, &ReferenceWindow::onAppFocusChanged);

    setWindowMode(windowMode());
}

ReferenceWindow::~ReferenceWindow()
{
    setMergeDest(nullptr);
    QObject::disconnect(App::ghostRefInstance(), nullptr, this, nullptr);
}

void ReferenceWindow::addReference(const ReferenceImageSP &refItem)
{
    if (m_refImages.contains(refItem))
    {
        // Do nothing if this window already has refItem
        return;
    }
    m_refImages.append(refItem);
    clampReferenceSize(refItem);
    emit referenceAdded(refItem);

    if (!m_activeImage)
    {
        setActiveImage(refItem);
    }
}

bool ReferenceWindow::removeReference(const ReferenceImageSP &refItem)
{
    const qsizetype idx = m_refImages.indexOf(refItem);
    if (idx < 0)
    {
        return false;
    }
    m_refImages.removeAt(idx);
    emit referenceRemoved(refItem);

    if (refItem == activeImage())
    {
        const qsizetype numItems = m_refImages.count();
        setActiveImage((numItems == 0) ? nullptr : m_refImages.at(std::min(numItems - 1, idx)));
    }
    return true;
}

ReferenceWindow *ReferenceWindow::detachReference(const ReferenceImageSP refItem)
{
    // Offset for the new window
    const QPoint windowOffset = QPoint(100, 100);

    if (refItem.isNull())
    {
        return nullptr;
    }

    if (!removeReference(refItem))
    {
        qWarning() << "Unable to detach reference item:" << refItem->name() << "not found in window";
        return nullptr;
    }

    ReferenceWindow *newWindow = App::ghostRefInstance()->newReferenceWindow();
    newWindow->addReference(refItem);
    newWindow->move(pos() + windowOffset);
    newWindow->show();

    return newWindow;
}

void ReferenceWindow::fromJson(const QJsonObject &json)
{
    const QJsonArray posArray = json["pos"].toArray();
    if (posArray.count() == 2)
    {
        move(posArray[0].toInt(), posArray[1].toInt());
    }

    ReferenceCollection &refCollection = App::ghostRefInstance()->referenceItems();
    for (auto jsonVal : json["tabs"].toArray())
    {
        const QString refName = jsonVal.toString();
        const ReferenceImageSP refImage = refCollection.getReferenceImage(refName);
        if (refImage)
        {
            addReference(refImage); // TODO Disable image size clamping
        }
        else
        {
            // TODO add blank tab?
            qWarning() << "Unable to find reference image " << refName << "in reference collection";
        }
    }

    const int newActiveTab = static_cast<int>(json["activeTab"].toInteger(-1));
    if (newActiveTab >= 0 && newActiveTab < m_tabBar->count())
    {
        m_tabBar->setCurrentIndex(newActiveTab);
    }
}

QJsonObject ReferenceWindow::toJson() const
{
    QJsonArray tabs;
    // Use the tabbar rather than m_refImages so that order of tabs is kept
    auto *tabbar = qobject_cast<TabBar *>(m_tabBar);
    for (int i = 0; i < tabbar->count(); i++)
    {
        const ReferenceImageSP refItem = tabbar->referenceAt(i);
        tabs.push_back(refItem->name());
    }

    return {
        {"pos", QJsonArray({pos().x(), pos().y()})},
        {"tabs", tabs},
        {"activeTab", m_tabBar->currentIndex()}};
}

void ReferenceWindow::setGhostState(bool value)
{
    // value should always be false if not in GhostMode
    value &= (windowMode() == GhostMode);

    if (ghostState() == value)
    {
        return;
    }
    m_ghostState = value;

    m_pictureWidget->setOpacityMultiplier(value ? ghostOpacity() : 1.0);

    emit ghostStateChanged(ghostState());
}

bool ReferenceWindow::windowFocused() const
{
    const QWidget *focus = QApplication::focusWidget();
    return ((focus != nullptr) && (focus == this || isAncestorOf(focus)));
}

WindowMode ReferenceWindow::windowMode() const
{
    return m_windowMode;
}

void ReferenceWindow::setWindowMode(WindowMode mode)
{
    if (mode != GhostMode)
    {
        setGhostState(false);
    }
    m_windowMode = mode;
    emit windowModeChanged(mode);
}

void ReferenceWindow::setActiveImage(const ReferenceImageSP &image)
{
    if (image != activeImage())
    {
        m_activeImage = image;
        emit activeImageChanged(m_activeImage);
        adjustSize();
    }
}

void ReferenceWindow::clampReferenceSize(const ReferenceImageSP &refItem)
{
    if (!screen() || !refItem)
    {
        return;
    }
    const QSize refSize = refItem->displaySize();
    const ReferenceImageSP &activeRef = activeImage();
    QSize newSize;

    if (activeRef && activeRef->isLoaded())
    {
        newSize = refSize.scaled(activeRef->displaySize(), Qt::KeepAspectRatio);
    }
    else
    {
        const QSize screenSize = screen()->size();
        newSize = refSize.boundedTo(screenSize / 2).expandedTo(screenSize / 4);
    }

    if (newSize != refSize)
    {
        refItem->setDisplaySize(newSize);
    }
}

void ReferenceWindow::drawHighlightedBorder()
{
    // Draw a border that lerps between two colors
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen;
    pen.setWidth(1);

    const auto marginSizeF = static_cast<float>(marginSize);
    const QColor color1(96, 96, 255);
    const QColor color2(255, 255, 255, static_cast<int>(255 / marginSizeF));

    QRect borderRect = rect();
    for (int i = 0; i <= marginSize; i++)
    {
        const float t = 1 - static_cast<float>(i) / marginSizeF;

        pen.setColor(lerp(color1, color2, t));
        painter.setPen(pen);

        painter.drawRect(borderRect);
        borderRect = borderRect.marginsRemoved({1, 1, 1, 1});
    }
}

qreal ReferenceWindow::ghostOpacity() const
{
    return (m_activeImage) ? m_activeImage->hoverOpacity()
                           : appPrefs()->getFloat(Preferences::GhostModeOpacity);
}

void ReferenceWindow::setMergeDest(ReferenceWindow *target)
{
    if (target == this)
    {
        qWarning() << "Cannot merge a Reference Window with itself";
        return;
    }
    if (target == m_mergeDest)
    {
        return;
    }

    // If already requesting a merge with another window clear the current request
    if (m_mergeDest && m_mergeDest != target && m_mergeDest->m_mergeRequester == this)
    {
        m_mergeDest->setMergeRequester(nullptr);
    }

    m_mergeDest = target;
    if (target)
    {
        target->setMergeRequester(this);
    }
}

void ReferenceWindow::setMergeRequester(ReferenceWindow *requester)
{
    if (requester != m_mergeRequester)
    {
        m_mergeRequester = requester;
        emit mergeRequested(requester);
    }
}

void ReferenceWindow::onAppFocusChanged(QWidget *old, QWidget *now)
{
    if (old == this || now == this)
    {
        update();
    }
}

void ReferenceWindow::onFrameCrop(QMargins cropBy)
{
    if (!m_activeImage)
    {
        return;
    }
    ReferenceImage &refImage = *m_activeImage;

    QMarginsF cropByF = cropBy.toMarginsF() / refImage.zoom();

    // Switch the crop direction if the image is flipped
    cropByF = flipMargins(cropByF, refImage.flipHorizontal(), refImage.flipVertical());

    const QRectF oldCrop = refImage.cropF();

    QRectF newCrop = oldCrop.marginsRemoved(cropByF);
    refImage.setCropF(newCrop);

    newCrop = refImage.cropF();
    const QRect oldGeo = geometry();

    adjustSize();

    // Move the window so that the left or top edges appear to move when cropped from
    if (cropBy.left() || cropBy.top())
    {
        const QPoint moveBy = oldGeo.bottomRight() - geometry().bottomRight();
        move(pos() + moveBy);
    }
}

void ReferenceWindow::onFrameMove(QPoint diff)
{
    move(pos() + diff);

    checkShouldMerge();
}

void ReferenceWindow::onFrameResize(Qt::Edges fromEdges, QSize newSize)
{
    if (!m_activeImage)
    {
        return;
    }

    m_activeImage->setDisplaySize(newSize);

    const QSize oldSize = size();
    adjustSize();
    const QSize sizeChange = size() - oldSize;
    const QPoint moveBy((fromEdges & Qt::LeftEdge) ? 0 : -sizeChange.width(),
                        (fromEdges & Qt::TopEdge) ? 0 : -sizeChange.height());
    move(pos() + moveBy);
}

void ReferenceWindow::onFrameViewMoved(QPoint diff)
{
    if (m_activeImage)
    {
        const QPointF shiftBy = -diff.toPointF() / m_activeImage->zoom();
        m_activeImage->shiftCropF(shiftBy);
    }
}

void ReferenceWindow::onTransformFinished(ResizeFrame::TransformType transform)
{
    if (transform == ResizeFrame::Moving && m_mergeDest)
    {
        mergeInto(m_mergeDest);
    }
}

void ReferenceWindow::checkShouldMerge()
{
    for (const auto &refWindow : App::ghostRefInstance()->referenceWindows())
    {
        if (refWindow && refWindow->isVisible() && refWindow != this)
        {
            if (windowsShouldMerge(this, refWindow))
            {
                setMergeDest(refWindow);
                return;
            }
        }
    }

    // No windows found to merge with so clear any already set merge
    if (m_mergeDest)
    {
        setMergeDest(nullptr);
    }
}

void ReferenceWindow::mergeInto(ReferenceWindow *other)
{
    setMergeDest(nullptr);
    for (auto &refItem : m_refImages)
    {
        other->addReference(refItem);
    }
    close();
}

void ReferenceWindow::closeEvent([[maybe_unused]] QCloseEvent *event)
{
    auto logger = qDebug() << "Closing reference window for references: ";
    for (const auto &refItem : m_refImages)
    {
        logger << (refItem->name().isEmpty() ? "[no name]" : refItem->name());
    }
}

void ReferenceWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->setAccepted(refLoad::isSupported(event));
}

void ReferenceWindow::dropEvent(QDropEvent *event)
{
    event->accept();

    const QList<ReferenceImageSP> results = refLoad::fromDropEvent(event);
    if (results.isEmpty())
    {
        // TODO Show messagebox
        qCritical() << "Unable to load reference images from drop event";
        return;
    }

    bool okResultFound = false;

    for (const auto &refImage : results)
    {
        if (refImage)
        {
            addReference(refImage);
            if (!okResultFound)
            {
                okResultFound = true;
                setActiveImage(refImage);
            }
        }
        else
        {
            qCritical() << "Null referenceImageSP given";
        }
    }
}

void ReferenceWindow::enterEvent([[maybe_unused]] QEnterEvent *event)
{
}

void ReferenceWindow::hideEvent([[maybe_unused]] QHideEvent *event)
{
    if (m_settingsPanel)
    {
        m_settingsPanel->close();
    }
}

void ReferenceWindow::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
    switch (event->key())
    {
    case Qt::Key_Delete:
        close();
        break;
    default:
        event->ignore();
        QWidget::keyReleaseEvent(event);
        break;
    }
}

void ReferenceWindow::leaveEvent([[maybe_unused]] QEvent *event)
{
}

void ReferenceWindow::paintEvent([[maybe_unused]] QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // If this window is active draw a border in its margin that lerps between two colors.
    if (windowFocused())
    {
        drawHighlightedBorder();
    }
}

void ReferenceWindow::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);

    if (!event->isAccepted() && event->angleDelta().y() != 0)
    {
        const int currentTab = m_tabBar->currentIndex();
        const bool wheelUp = event->angleDelta().y() > 0;
        m_tabBar->setCurrentIndex(wheelUp ? currentTab - 1 : currentTab + 1);
        event->accept();
    }
}

void ReferenceWindow::showSettingsWindow(const QPoint &atPos)
{
    if (!m_settingsPanel)
    {
        m_settingsPanel = new SettingsPanel(parentWidget(), m_activeImage);
        m_settingsPanel->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_settingsPanel->move(atPos.isNull() ? QCursor::pos() : atPos);
    m_settingsPanel->show();
    m_settingsPanel->setFocus();
}

#include "reference_window.moc"
