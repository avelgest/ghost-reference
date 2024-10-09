#pragma once

#include <optional>

#include <QtWidgets/QWidget>

#include <QtGui/QPixmap>

#include "../types.h"

class PictureWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PictureWidget)

    ReferenceImageSP m_imageSP;
    ReferenceWindow *m_referenceWindow = nullptr;
    ResizeFrame *m_resizeFrame = nullptr;
    qreal m_opacityMultiplier = 1.0;

    QPixmap m_cachedImage;
    bool m_cacheInvalidated = true;

public:
    explicit PictureWidget(QWidget *parent = nullptr);
    ~PictureWidget() override = default;

    QSize sizeHint() const override;

    // Map a point from local widget coordinates to the reference image's baseImage coordinates
    QPointF localToBaseImage(const QPointF &localPos) const;
    // Map a point from local widget coordinates to the reference image's displayImage coordinates
    QPointF localToDisplayImage(const QPointF &localPos) const;

    const ReferenceImageSP &image() const;
    void setImage(const ReferenceImageSP &image);

    qreal opacityMultiplier() const;
    void setOpacityMultiplier(qreal value);

    ResizeFrame *resizeFrame() const;

    ReferenceWindow *referenceWindow() const;
    void setReferenceWindow(ReferenceWindow *refWindow);

    WindowMode windowMode() const;

    // Invalidates the cached image causing it to be redrawn. Also calls update().
    void invalidateCache();

protected:
    bool isCacheInvalidated() const;

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void onReferenceCursorChanged(const std::optional<QCursor> &cursor, std::optional<RefType> refType);
    void onWindowModeChanged(WindowMode newMode);
};

inline const ReferenceImageSP &PictureWidget::image() const { return m_imageSP; }

inline qreal PictureWidget::opacityMultiplier() const { return m_opacityMultiplier; }

inline void PictureWidget::setOpacityMultiplier(qreal value)
{
    m_opacityMultiplier = std::clamp(value, 0., 1.);
    update();
}

inline ResizeFrame *PictureWidget::resizeFrame() const { return m_resizeFrame; }

inline ReferenceWindow *PictureWidget::referenceWindow() const { return m_referenceWindow; }
