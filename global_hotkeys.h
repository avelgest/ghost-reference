#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtGui/QKeySequence>

class App;
class QHotkey;

class GlobalHotkeys : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GlobalHotkeys)
public:
    enum BuiltIn
    {
        HideAllWindows,
        ToggleGhostMode
    };

    struct BuiltInDefault
    {
        BuiltIn builtIn;
        QString name;
        QKeySequence key;
    };

    static const QString &builtinName(BuiltIn enumValue);
    static const QList<BuiltInDefault> &builtIns();

    static QKeySequence getKey(BuiltIn builtIn);
    static QKeySequence getKey(const QString &name);

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