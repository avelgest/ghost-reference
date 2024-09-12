#pragma once

#include <memory>

#include <QtCore/QObject>

class App;
class QHotkey;

class GlobalHotkeys : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GlobalHotkeys)
public:
    enum Builtin
    {
        HideAllWindows
    };

    static const char *builtinName(Builtin enumValue);

private:
    bool m_enabled = false;
    std::vector<std::unique_ptr<QHotkey>> m_hotkeys;

    QHotkey *addHotkey(const QKeySequence &keySequence);

    // Reload all global hotkeys from the app's preferences
    void reloadAll();

public:
    explicit GlobalHotkeys(QObject *parent = nullptr);
    ~GlobalHotkeys() override;

    bool isEnabled() const;
    void setEnabled(bool value);
};

inline bool GlobalHotkeys::isEnabled() const
{
    return m_enabled;
}