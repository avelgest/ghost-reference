#pragma once

#include <QtWidgets/QWidget>

#include "../types.h"

class BackWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BackWindow)

    WindowMode m_windowMode = TransformMode;

public:
    explicit BackWindow(QWidget *parent = nullptr);
    ~BackWindow() override = default;

    inline WindowMode windowMode() const;
    void setWindowMode(WindowMode value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

WindowMode BackWindow::windowMode() const { return m_windowMode; }
