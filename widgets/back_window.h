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
    MainToolbarActions *m_mainToolbarActions = nullptr;

public:
    explicit BackWindow(QWidget *parent = nullptr);
    ~BackWindow() override = default;

    SettingsPanel *settingsWindow();
    SettingsPanel *showSettingsWindow(const QPoint &atPos);
    void hideSettingsWindow();

    inline WindowMode windowMode() const;
    void setWindowMode(WindowMode value);

    MainToolbarActions *mainToolbarActions() const;
    // Add actions from the main toolbar to the back window so they are always accessible
    // via keyboard shortcuts.
    void setMainToolbarActions(MainToolbarActions *actions);

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

inline MainToolbarActions *BackWindow::mainToolbarActions() const
{
    return m_mainToolbarActions;
}