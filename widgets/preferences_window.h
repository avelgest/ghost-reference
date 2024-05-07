#pragma once

#include <QtWidgets/QWidget>

#include "../types.h"

class PreferencesWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PreferencesWindow)

private:
    Preferences *m_prefs = nullptr;

public:
    explicit PreferencesWindow(QWidget *parent = nullptr);
    explicit PreferencesWindow(Preferences *prefs, QWidget *parent = nullptr);
    ~PreferencesWindow() override = default;

    Preferences *prefs() const;
    void setPrefs(Preferences *prefs);

    QSize sizeHint() const override;
};

inline Preferences *PreferencesWindow::prefs() const { return m_prefs; }
