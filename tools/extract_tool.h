#pragma once

#include "tool.h"

#include <QtCore/QPointer>

class ExtractTool : public Tool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExtractTool)

private:
    QPoint m_startPoint;
    QPoint m_endPoint;
    QPointer<QWidget> m_target;

public:
    ExtractTool();
    ~ExtractTool() override = default;

    void drawOverlay(ReferenceWindow *refWindow, QPainter &painter) override;

protected:
    void mouseMoveEvent(QWidget *widget, QMouseEvent *event) override;
    void mousePressEvent(QWidget *widget, QMouseEvent *event) override;
    void mouseReleaseEvent(QWidget *widget, QMouseEvent *event) override;

private:
    bool isDragging() const;
    bool extractSelection() const;
};
