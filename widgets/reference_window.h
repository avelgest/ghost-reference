#pragma once

#include <optional>

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "../types.h"

class QTabBar;

class ReferenceWindow : public QWidget
{

private:
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ReferenceWindow)

    bool m_ghostState = false;
    WindowMode m_windowMode = GhostMode;

    ReferenceImageSP m_activeImage;
    QList<ReferenceImageSP> m_refImages;
    QPoint m_lastMousePos;

    PictureWidget *m_pictureWidget = nullptr;
    QPointer<SettingsPanel> m_settingsPanel = nullptr;
    QTabBar *m_tabBar = nullptr;

public:
    explicit ReferenceWindow(QWidget *parent = nullptr);
    explicit ReferenceWindow(QImage &image, QWidget *parent = nullptr);
    ~ReferenceWindow() override = default;

    void addReference(const ReferenceImageSP &refItem);
    void removeReference(const ReferenceImageSP &refItem);

    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

    ReferenceImageSP &activeImage();
    const ReferenceImageSP &activeImage() const;
    void setActiveImage(const ReferenceImageSP &image);

    bool ghostState() const;
    void setGhostState(bool value);

    bool windowFocused() const;

    const QList<ReferenceImageSP> &referenceImages() const;

    WindowMode windowMode() const;
    void setWindowMode(WindowMode mode);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void clampReferenceSize(const ReferenceImageSP &refItem);
    void drawHighlightedBorder();
    qreal ghostOpacity() const;

    void onAppFocusChanged(QWidget *old, QWidget *now);
    void onFrameCrop(QMargins cropBy);
    void onFrameMove(QPoint diff);
    void onFrameResize(Qt::Edges fromEdges, QSize newSize);
    void onFrameViewMoved(QPoint diff);

public slots:
    void showSettingsWindow(const QPoint &atPos = {});

signals:
    void activeImageChanged(const ReferenceImageSP &newImage);
    void ghostStateChanged(bool newValue);
    void referenceAdded(const ReferenceImageSP &refItem);
    void referenceRemoved(const ReferenceImageSP &refItem);
    void windowModeChanged(WindowMode newMode);
};

inline ReferenceImageSP &ReferenceWindow::activeImage() { return m_activeImage; }
inline const ReferenceImageSP &ReferenceWindow::activeImage() const { return m_activeImage; }

inline bool ReferenceWindow::ghostState() const { return m_ghostState; }

inline const QList<ReferenceImageSP> &ReferenceWindow::referenceImages() const
{
    return m_refImages;
}
