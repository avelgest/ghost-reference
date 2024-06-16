#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QWidget>

class ResizeFrameButton;

class ResizeFrame : public QWidget
{
private:
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResizeFrame)

    friend class ResizeFrameButton;

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

    void keyReleaseEvent(QKeyEvent *event) override;

public:
    QSize sizeHint() const override;

    TransformType currentTransform() const;
    void showOnlyMoveControl(bool value);

    QWidget *target() const;
    void setTarget(QWidget *value);

signals:
    void cropped(QMargins cropBy);
    void moved(QPoint change);
    void resized(Qt::Edges fromEdges, QSize sizeChange);
    void viewMoved(QPoint change);

    void transformStarted(TransformType type);
    void transformFinished(TransformType type);
};

class ResizeFrameButton : public QAbstractButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResizeFrameButton)
private:
    ResizeFrame *m_resizeFrame;
    Qt::Edges m_edges;
    bool m_isCorner = false;
    bool m_isVertical = false;

    QPoint m_lastMousePos;
    // The modifiers held when the button was pressed
    Qt::KeyboardModifiers m_modifiers;

public:
    explicit ResizeFrameButton(ResizeFrame *parent, Qt::Edges edges);
    ~ResizeFrameButton() override = default;

    Qt::Edges edges() const;
    ResizeFrame::TransformType transformType() const;

    QSize sizeHint() const override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    bool hitButton(const QPoint &pos) const override;

private:
    void onModifiersChanged(Qt::KeyboardModifiers modifiers);
    void refreshCursor();
};

inline QSize ResizeFrame::targetSize() const { return m_target ? m_target->size() : size(); }

inline ResizeFrame::TransformType ResizeFrame::currentTransform() const { return m_currentTransform; }
inline QWidget *ResizeFrame::target() const { return m_target; }

inline void ResizeFrame::setTarget(QWidget *value)
{
    m_target = value;
    updateGeometry();
}

inline Qt::Edges ResizeFrameButton::edges() const
{
    return m_edges;
}

inline bool ResizeFrameButton::hitButton(const QPoint &pos) const
{
    return isDown() ? true : QAbstractButton::hitButton(pos);
}