#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>

struct PreferencesPrivate;

struct PrefEnumItem
{
    QString identifier;
    QString name;
    QString value;
};

struct PrefFloatRange
{
    qreal min = std::numeric_limits<qreal>::min();
    qreal max = std::numeric_limits<qreal>::max();

    qreal size() const { return max - min; }
};

struct PrefIntRange
{
    qint32 min = std::numeric_limits<qint32>::min();
    qint32 max = std::numeric_limits<qint32>::max();
};

class Preferences : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Preferences)

public:
    enum Keys
    {
        InvalidPreference,

        AllowInternet,
        AnimateToolbarCollapse,
        AskSaveBeforeClosing,
        GhostModeOpacity,
        GlobalHotkeysEnabled,
        LocalFilesLink,
        LocalFilesStoreMaxMB,
        LoggingEnabled,
        OverrideKeyAlt,
        OverrideKeyCtrl,
        OverrideKeyShift,
        UndoMaxSteps,
    };

    explicit Preferences(QObject *parent = nullptr);
    ~Preferences() override;

    Preferences *duplicate(QObject *parent = nullptr) const;
    void copyFromOther(const Preferences *other);

    static Preferences *loadFromDisk(QObject *parent = nullptr);
    static Preferences *loadFromJson(const QJsonDocument &json, QObject *parent);
    QJsonDocument toJsonDocument() const;
    void saveToDisk() const;

    static QString configDir();

    bool checkAllEqual(const Preferences *other) const;

    static const QString &getName(Keys key);
    static const QString &getDisplayName(Keys key);
    static const QString &getDescription(Keys key);
    static const PrefEnumItem &getEnumItem(Keys key, int idx);
    static PrefFloatRange getFloatRange(Keys key);
    static PrefIntRange getIntRange(Keys key);
    static QMetaType::Type getType(Keys key);

    template <typename T>
    T get(Keys key) const;

    QVariant getVariant(Keys key) const;

    void set(Keys key, const QVariant &value);

#define PREFS_GET(name, type) \
    type name(Keys key) const { return get<type>(key); }

    PREFS_GET(getBool, bool)
    PREFS_GET(getFloat, qreal)
    PREFS_GET(getInt, int)
    PREFS_GET(getString, QString)
#undef PREFS_GET

#define PREFS_SET(name, type) \
    void name(Keys key, type value) { set(key, value); }

    PREFS_SET(setBool, bool)
    PREFS_SET(setString, const QString &)
#undef PREFS_SET

    void setFloat(Keys key, qreal value);
    void setInt(Keys key, int value);

    using HotkeyMap = QMap<QString, QKeySequence>;

    HotkeyMap &hotkeys();
    const HotkeyMap &hotkeys() const;
    HotkeyMap &globalHotkeys();
    const HotkeyMap &globalHotkeys() const;

    static const HotkeyMap &defaultHotkeys();
    static const HotkeyMap &defaultGlobalHotkeys();

    // Restore a hotkey to its default value
    void resetHotkey(const QString &hotkeyName, bool globalHotkey);

    // Returns the modifer keys used to enter override (unghost) mode.
    Qt::KeyboardModifiers overrideKeys() const;

private:
    QScopedPointer<PreferencesPrivate> p;
};

// Equivalent to App::ghostRefInstance()->preferences()
const Preferences *appPrefs();
