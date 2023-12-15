#pragma once

#include <QtWidgets\QWidget>

class ToolbarWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ToolbarWindow)
private:
    /* data */
public:
    explicit ToolbarWindow(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~ToolbarWindow() override = default;
};
