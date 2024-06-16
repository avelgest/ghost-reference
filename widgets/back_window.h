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
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

signals:
    void modifierKeysChanged(Qt::KeyboardModifiers modifiers);
};

WindowMode BackWindow::windowMode() const { return m_windowMode; }
