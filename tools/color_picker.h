#pragma once

#include <memory>

#include "tool.h"

class QIcon;

class ColorPicker : public Tool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ColorPicker)

    // Use the original (base) image without effects like saturation applied
    static bool s_useOriginal;

    std::unique_ptr<QWidget> m_toolWindow = nullptr;

public:
    explicit ColorPicker();
    ~ColorPicker() override = default;

    static QIcon icon();

    static bool useOriginal();
    static void setUseOriginal(bool value);

    void mouseMoveEvent(QWidget *widget, QMouseEvent *event) override;
    void mouseReleaseEvent(QWidget *widget, QMouseEvent *event) override;
    void keyReleaseEvent(QWidget *widget, QKeyEvent *event) override;

protected:
    void onActivate() override;
    void onDeactivate() override;

signals:
    void colorPicked(QColor color);
};

inline bool ColorPicker::useOriginal()
{
    return s_useOriginal;
}

inline void ColorPicker::setUseOriginal(bool value)
{
    s_useOriginal = value;
}