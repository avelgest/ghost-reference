#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

class ResizeFrame : public QWidget
{
private:
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResizeFrame)

public:
    enum TransformType
    {
        NoTransform,
        Moving,
        Cropping,
        Resizing,
        MovingView,
    };

private:
    Qt::Edges m_currentEdges;
    TransformType m_currentTransform = NoTransform;
    QPoint m_lastMousePos;
    QPointer<QWidget> m_target = nullptr;

    QSize targetSize() const;

public:
    explicit ResizeFrame(QWidget *parent = nullptr);
    ~ResizeFrame() override = default;

protected:
    void setCurrentTransform(TransformType transform);

    Qt::Edges hotspotAtPoint(QPointF pos) const;
    QMarginsF relativeMargins(Qt::Edges edges, QPoint diff) const;
    void updateCursor(QPointF position, Qt::KeyboardModifiers modifiers);
    void updateCursor();

    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

public:
    QSize sizeHint() const override;

    TransformType currentTransform() const;

    QWidget *target() const;
    void setTarget(QWidget *value);

signals:
    void cropped(QMargins cropBy);
    void moved(QPoint change);
    void resized(Qt::Edges fromEdges, QSize newSize);
    void viewMoved(QPoint change);

    void transformStarted(TransformType type);
    void transformFinished(TransformType type);
};

inline QSize ResizeFrame::targetSize() const { return m_target ? m_target->size() : size(); }

inline ResizeFrame::TransformType ResizeFrame::currentTransform() const { return m_currentTransform; }
inline QWidget *ResizeFrame::target() const { return m_target; }

inline void ResizeFrame::setTarget(QWidget *value)
{
    m_target = value;
    updateGeometry();
}
