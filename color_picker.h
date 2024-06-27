#pragma once

#include <memory>

#include "tool.h"

class QIcon;

class ColorPicker : public Tool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ColorPicker)

    std::unique_ptr<QWidget> m_toolWindow = nullptr;

public:
    explicit ColorPicker();
    ~ColorPicker() override = default;

    static QIcon icon();

    void mouseMoveEvent(QWidget *widget, QMouseEvent *event) override;
    void mouseReleaseEvent(QWidget *widget, QMouseEvent *event) override;
    void keyReleaseEvent(QWidget *widget, QKeyEvent *event) override;

protected:
    void onActivate() override;
    void onDeactivate() override;

signals:
    void colorPicked(QColor color);
};
