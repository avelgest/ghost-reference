#pragma once

#include <QtWidgets\QToolBar>
#include <QtWidgets\QWidget>

#include "main_window_actions.h"

class MainWindow : public QToolBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

    QWidget *m_dragWidget = nullptr;
    QScopedPointer<Actions> m_windowActions;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    QSize sizeHint() const override;
};