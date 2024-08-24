#include "reference_window.h"

#include <QtCore/Qt>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <QtGui/QContextMenuEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMenu>

#include "../app.h"
#include "../preferences.h"
#include "../reference_collection.h"
#include "../reference_image.h"
#include "../reference_loading.h"
#include "../undo_stack.h"

#include "back_window.h"
#include "main_toolbar.h"
#include "picture_widget.h"
#include "resize_frame.h"
#include "settings_panel.h"
#include "tab_bar.h"

#include <QtWidgets/QStyle>

namespace
{

    const int marginSize = 10;

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

    void fitToCurrentTab(const ReferenceWindow *refWindow, const ReferenceImageSP &refImage)
    {
        const ReferenceImageSP &current = refWindow->activeImage();
        if (refImage.isNull() || current.isNull() || current == refImage)
        {
            return;
        }

        const QSizeF oldImgSize = current->displaySize().toSizeF();
        const QSizeF newImgSize = refImage->displaySize().toSizeF();
        QSizeF fitSize;

        switch (refWindow->tabFit())
        {
        case ReferenceWindow::TabFit::FitToHeight:
            fitSize = newImgSize * oldImgSize.height() / newImgSize.height();
            break;
        case ReferenceWindow::TabFit::FitToWidth:
            fitSize = newImgSize * oldImgSize.width() / newImgSize.width();
            break;
        default:
            return;
        }
        refImage->setDisplaySize(fitSize.toSize());
    }

    void markAppUnsavedChanges()
    {
        App::ghostRefInstance()->setUnsavedChanges();
    }

    // Paste reference images from the clipboard into refWindow
    void pasteRefsFromClipboard(ReferenceWindow *refWindow)
    {
        const QList<ReferenceImageSP> newRefImages = refLoad::fromClipboard();

        if (newRefImages.isEmpty())
        {
            qWarning() << "Unable to load any reference images from the clipboard";
            return;
        }

        for (const auto &refImage : newRefImages)
        {
            refWindow->addReference(refImage, true);
        }
        refWindow->setActiveImage(newRefImages.last());
    }

    void addUndoStep(ReferenceWindow *refWindow, bool refItems = false)
    {
        UndoStack *undoStack = App::ghostRefInstance()->undoStack();
        undoStack->pushRefWindow(refWindow, refItems);
    }

    void initActions(ReferenceWindow *refWindow)
    {
        QAction *action = nullptr;

        action = refWindow->addAction("Hide", QKeySequence("Alt+H"));
        QObject::connect(action, &QAction::triggered, refWindow, [=]() { refWindow->hide(); });

        action = refWindow->addAction("Close", QKeySequence::Delete);
        QObject::connect(action, &QAction::triggered, refWindow, [=]() {
            addUndoStep(refWindow, true);
            refWindow->close();
        });

        action = refWindow->addAction("Paste", QKeySequence::Paste);
        QObject::connect(action, &QAction::triggered, refWindow, [=]() { pasteRefsFromClipboard(refWindow); });
    }

    QPoint defaultWindowPos()
    {
        const App *app = App::ghostRefInstance();
        if (app->mainToolbar() && app->mainToolbar()->isVisible())
        {
            const QPoint offset(50, 50);
            return app->mainToolbar()->pos() + offset;
        }
        const QScreen *screen = App::primaryScreen();
        if (screen)
        {
            return {screen->size().width() / 2, screen->size().height() / 2};
        }
        return {};
    }

} // namespace

ReferenceWindow *ReferenceWindow::activeWindow()
{
    for (const auto &refWindow : App::ghostRefInstance()->referenceWindows())
    {
        if (refWindow && refWindow->isWindowFocused())
        {
            return refWindow;
        }
    }
    return nullptr;
}

ReferenceWindow::ReferenceWindow(QWidget *parent)
    : QWidget(parent),
      m_backWindow(qobject_cast<BackWindow *>(parent)),
      m_pictureWidget(new PictureWidget(this)),
      m_tabBar(new TabBar(this)),
      m_overlay(new Overlay(this))
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);

    move(defaultWindowPos());

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(marginSize, marginSize, marginSize, marginSize);
    grid->setSpacing(0);
    grid->addWidget(m_pictureWidget, 1, 0);
    grid->addWidget(m_overlay, 1, 0);
    grid->addWidget(m_tabBar, 2, 0);

    ResizeFrame *resizeFrame = this->resizeFrame();

    QObject::connect(resizeFrame, &ResizeFrame::moved, this, &ReferenceWindow::onFrameMove);
    QObject::connect(resizeFrame, &ResizeFrame::cropped, this, &ReferenceWindow::onFrameCrop);
    QObject::connect(resizeFrame, &ResizeFrame::resized, this, &ReferenceWindow::onFrameResize);
    QObject::connect(resizeFrame, &ResizeFrame::viewMoved, this, &ReferenceWindow::onFrameViewMoved);
    QObject::connect(resizeFrame, &ResizeFrame::transformStarted, this, &ReferenceWindow::onTransformStarted);
    QObject::connect(resizeFrame, &ResizeFrame::transformFinished, this, &ReferenceWindow::onTransformFinished);

    App *app = App::ghostRefInstance();
    QObject::connect(app, &App::focusChanged, this, &ReferenceWindow::onAppFocusChanged);
    QObject::connect(app, &App::globalModeChanged, this, &ReferenceWindow::onGlobalModeChanged);

    onGlobalModeChanged(app->globalMode());

    initActions(this);

    onGlobalModeChanged(windowMode());
}

ReferenceWindow::~ReferenceWindow()
{
    setMergeDest(nullptr);
    QObject::disconnect(App::ghostRefInstance(), nullptr, this, nullptr);
}

void ReferenceWindow::addReference(const ReferenceImageSP &refItem, bool clampSize)
{
    if (m_refImages.contains(refItem))
    {
        // Do nothing if this window already has refItem
        return;
    }
    m_refImages.append(refItem);

    if (clampSize)
    {
        clampReferenceSize(refItem);
    }
    emit referenceAdded(refItem);

    if (!m_activeImage)
    {
        setActiveImage(refItem);
    }
    markAppUnsavedChanges();
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
    markAppUnsavedChanges();
    return true;
}

void ReferenceWindow::clearReferences()
{
    setActiveImage(nullptr);
    while (!m_refImages.isEmpty())
    {
        const bool success = removeReference(m_refImages.back());
        if (!success)
        {
            qCritical() << "Unable to remove reference while clearing ReferenceWindow:" << m_refImages.back()->name();
        }
    }
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
    newWindow->addReference(refItem, false);
    newWindow->move(pos() + windowOffset);
    newWindow->show();

    markAppUnsavedChanges();

    return newWindow;
}

void ReferenceWindow::fromJson(const QJsonObject &json)
{
    // Prevent reference images from being deleted after clear
    const QList<ReferenceImageSP> oldRefImages = m_refImages;
    clearReferences();

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
            addReference(refImage, false);
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
    // Use the tab bar rather than m_refImages so that order of tabs is kept
    for (int i = 0; i < m_tabBar->count(); i++)
    {
        const ReferenceImageSP refItem = m_tabBar->referenceAt(i);
        tabs.push_back(refItem->name());
    }

    return {{"pos", QJsonArray({pos().x(), pos().y()})}, {"tabs", tabs}, {"activeTab", m_tabBar->currentIndex()}};
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

bool ReferenceWindow::isWindowFocused() const
{
    const QWidget *focus = QApplication::focusWidget();
    return ((focus != nullptr) && (focus == this || isAncestorOf(focus)));
}

WindowMode ReferenceWindow::windowMode() const
{
    return m_backWindow->windowMode();
}

void ReferenceWindow::onGlobalModeChanged(WindowMode mode)
{
    if (mode != GhostMode)
    {
        setGhostState(false);
    }
    update();
    emit windowModeChanged(mode);
}

void ReferenceWindow::setActiveImage(const ReferenceImageSP &image)
{
    if (image != activeImage())
    {
        if (!activeImage().isNull())
        {
            QObject::disconnect(activeImage().get(), &ReferenceImage::baseImageChanged, this,
                                &ReferenceWindow::adjustSize);
        }
        m_activeImage = image;
        if (!image.isNull())
        {
            fitToCurrentTab(this, image);
            QObject::connect(image.get(), &ReferenceImage::baseImageChanged, this, &ReferenceWindow::adjustSize);
        }
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
    const QColor color1(96, 96, 255, 196);
    const QColor color2(32, 32, 255, 0);

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

SettingsPanel *ReferenceWindow::settingsPanel() const
{
    return m_backWindow ? m_backWindow->settingsWindow() : nullptr;
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

void ReferenceWindow::onFrameResize(Qt::Edges fromEdges, QSize sizeChange)
{
    if (!m_activeImage)
    {
        return;
    }

    const QSize newImgSize = m_activeImage->displaySize() + sizeChange;

    m_activeImage->setDisplaySize(newImgSize);

    const QSize oldSize = size();
    adjustSize();
    sizeChange = size() - oldSize;
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

void ReferenceWindow::onTransformStarted(ResizeFrame::TransformType transform)
{
    if (transform != ResizeFrame::NoTransform)
    {
        raise();
        if (settingsPanel())
        {
            settingsPanel()->raise();
        }
        App::ghostRefInstance()->undoStack()->pushWindowAndRefItem(this, activeImage());
    }
}

void ReferenceWindow::onTransformFinished(ResizeFrame::TransformType transform)
{
    if (transform == ResizeFrame::Moving && m_mergeDest)
    {
        mergeInto(m_mergeDest);
    }
    if (!m_refImages.isEmpty())
    {
        markAppUnsavedChanges();
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
        other->addReference(refItem, true);
    }
    close();
}

ResizeFrame *ReferenceWindow::resizeFrame() const
{
    return m_pictureWidget->resizeFrame();
}

void ReferenceWindow::closeEvent([[maybe_unused]] QCloseEvent *event)
{
    auto logger = qInfo() << "Closing reference window for references: ";
    for (const auto &refItem : m_refImages)
    {
        logger << (refItem->name().isEmpty() ? "[no name]" : refItem->name());
    }
}

void ReferenceWindow::contextMenuEvent(QContextMenuEvent *event)
{
    if (windowMode() == ToolMode)
    {
        // Let the active tool handle the event
        return;
    }

    if (activeImage())
    {
        showSettingsWindow();
    }
    else
    {
        // Show a context menu
        QMenu menu(this);
        QAction *paste = menu.addAction("Paste");
        QObject::connect(paste, &QAction::triggered, [this]() { pasteRefsFromClipboard(this); });
        paste->setEnabled(refLoad::isSupportedClipboard());

        menu.exec(event->globalPos());
    }
    event->accept();
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
            addReference(refImage, true);
            if (!okResultFound)
            {
                okResultFound = true;
                setActiveImage(refImage);
            }
        }
        else
        {
            qCritical() << "Null ReferenceImageSP given";
        }
    }
}

void ReferenceWindow::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void ReferenceWindow::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

void ReferenceWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    // If this window is active draw a border in its margin that lerps between two colors.
    if (isWindowFocused() && windowMode() != GhostMode)
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
    if (m_backWindow)
    {
        m_backWindow->showSettingsWindow(atPos);
    }
}

#include "reference_window.moc"
