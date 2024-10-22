#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include "../types.h"

class BackWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BackWindow)

    WindowMode m_windowMode = TransformMode;
    QPointer<SettingsPanel> m_settingsPanel = nullptr;
    BackWindowActions *m_backWindowActions = nullptr;

public:
    explicit BackWindow(QWidget *parent = nullptr);
    ~BackWindow() override = default;

    SettingsPanel *settingsWindow();
    SettingsPanel *showSettingsWindow(const QPoint &atPos);
    void hideSettingsWindow();

    inline WindowMode windowMode() const;
    void setWindowMode(WindowMode value);

    BackWindowActions *backWindowActions() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

signals:
    void modifierKeysChanged(Qt::KeyboardModifiers modifiers);
};

inline WindowMode BackWindow::windowMode() const
{
    return m_windowMode;
}

inline BackWindowActions *BackWindow::backWindowActions() const
{
    return m_backWindowActions;
}