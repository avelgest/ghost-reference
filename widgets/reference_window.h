#pragma once

#include <optional>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "../types.h"
#include "resize_frame.h"

class ReferenceWindow : public QWidget
{
public:
    enum class TabFit
    {
        NoFit,
        FitToWidth,
        FitToHeight
    };

private:
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReferenceWindow)

    RefWindowId m_identifier = 0;
    bool m_ghostState = false;
    TabFit m_tabFit = TabFit::FitToWidth;
    qreal m_opacity = 1.0;

    ReferenceImageSP m_activeImage;
    QList<ReferenceImageSP> m_refImages;
    QPoint m_lastMousePos;

    // For merging with other ReferenceWindows
    QPointer<ReferenceWindow> m_mergeDest;      // window to merge into
    QPointer<ReferenceWindow> m_mergeRequester; // a window requesting to merge with this window

    BackWindow *m_backWindow = nullptr;
    PictureWidget *m_pictureWidget = nullptr;
    TabBar *m_tabBar = nullptr;
    QWidget *m_overlay = nullptr;

public:
    static ReferenceWindow *activeWindow();

    explicit ReferenceWindow(QWidget *parent = nullptr);
    ~ReferenceWindow() override;

    RefWindowId identifier() const;
    void setIdentifier(RefWindowId id);

    void addReference(const ReferenceImageSP &refItem, bool clampSize = true);
    bool removeReference(const ReferenceImageSP &refItem);
    void clearReferences();
    ReferenceWindow *detachReference(ReferenceImageSP refItem);
    ReferenceWindow *duplicateActive(bool linked = true) const;

    // Set the crop of the reference image keeping the window's top-left the same.
    void setCrop(const QRect &crop);
    void setCrop(const QRectF &crop);

    void setVisible(bool visible) override;

    PictureWidget *pictureWidget() const;

    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    ReferenceImageSP &activeImage();
    const ReferenceImageSP &activeImage() const;
    void setActiveImage(const ReferenceImageSP &image);

    bool ghostState() const;
    void setGhostState(bool value);

    TabFit tabFit() const;
    void setTabFit(TabFit value);

    qreal opacity() const;
    void setOpacity(qreal value);

    bool isWindowFocused() const;

    const QList<ReferenceImageSP> &referenceImages() const;

    WindowMode windowMode() const;

protected:
    // Check whether this window should request a merge with another ReferenceWindow.
    // This doesn't perform the merge but only tells the windows to indicate that a merge will
    // occur.
    void checkShouldMerge();
    // Merge into ReferenceWindow other. Doing so adds all this window's reference items to other
    // and closes this window.
    void mergeInto(ReferenceWindow *other);

    ResizeFrame *resizeFrame() const;

    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void clampReferenceSize(const ReferenceImageSP &refItem);
    void drawHighlightedBorder();
    qreal ghostOpacity() const;
    SettingsPanel *settingsPanel() const;
    void setMergeDest(ReferenceWindow *target);
    void setMergeRequester(ReferenceWindow *requester);

    void onAppFocusChanged(QWidget *old, QWidget *now);
    void onFrameCrop(QMargins cropBy);
    void onFrameMove(QPoint diff);
    void onFrameResize(Qt::Edges fromEdges, QSize sizeChange);
    void onFrameViewMoved(QPoint diff);
    void onGlobalModeChanged(WindowMode mode);
    void onTransformStarted(ResizeFrame::TransformType transform);
    void onTransformFinished(ResizeFrame::TransformType transform);

public slots:
    void showSettingsWindow(const QPoint &atPos = {});

signals:
    void activeImageChanged(const ReferenceImageSP &newImage);
    void ghostStateChanged(bool newValue);
    void referenceAdded(const ReferenceImageSP &refItem);
    void referenceRemoved(const ReferenceImageSP &refItem);
    void visibilityChanged(bool visibility);
    void windowModeChanged(WindowMode newMode);

    // Another window has requested to merge with this window
    // (e.g. when another ReferenceWindow is dragged over this one)
    // requester will be null if the merge request has been cancelled.
    void mergeRequested(ReferenceWindow *requester);
};

inline RefWindowId ReferenceWindow::identifier() const
{
    return m_identifier;
}

inline void ReferenceWindow::setIdentifier(RefWindowId id)
{
    m_identifier = id;
}

inline void ReferenceWindow::setCrop(const QRect &crop)
{
    setCrop(crop.toRectF());
}

inline PictureWidget *ReferenceWindow::pictureWidget() const
{
    return m_pictureWidget;
}

inline ReferenceImageSP &ReferenceWindow::activeImage() { return m_activeImage; }
inline const ReferenceImageSP &ReferenceWindow::activeImage() const { return m_activeImage; }

inline bool ReferenceWindow::ghostState() const { return m_ghostState; }

inline ReferenceWindow::TabFit ReferenceWindow::tabFit() const { return m_tabFit; }

inline void ReferenceWindow::setTabFit(TabFit value) { m_tabFit = value; }

inline qreal ReferenceWindow::opacity() const
{
    return m_opacity;
}

inline const QList<ReferenceImageSP> &ReferenceWindow::referenceImages() const
{
    return m_refImages;
}

inline void ReferenceWindow::setVisible(bool visible)
{
    const bool oldHidden = isHidden();
    QWidget::setVisible(visible);
    if (oldHidden != isHidden())
    {
        emit visibilityChanged(visible);
    }
}