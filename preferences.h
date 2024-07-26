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
        LocalFilesStore,
        LocalFilesStoreMaxMB,
    };

    explicit Preferences(QObject *parent = nullptr);
    ~Preferences() override;

    Preferences *duplicate(QObject *parent = nullptr) const;
    void copyFromOther(Preferences *other);

    static Preferences *loadFromDisk(QObject *parent = nullptr);
    static Preferences *loadFromJson(const QJsonDocument &json, QObject *parent);
    QJsonDocument toJsonDocument() const;
    void saveToDisk() const;

    bool checkAllEqual(Preferences *other);

    static const QString &getName(Keys key);
    static const QString &getDisplayName(Keys key);
    static const QString &getDescription(Keys key);
    static const PrefEnumItem &getEnumItem(Keys key, int idx);
    static PrefFloatRange getFloatRange(Keys key);
    static QMetaType::Type getType(Keys key);

    template <typename T>
    T get(Keys key) const;

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
    PREFS_SET(setFloat, qreal)
    PREFS_SET(setInt, int)
    PREFS_SET(setString, const QString &)
#undef PREFS_SET

private:
    QScopedPointer<PreferencesPrivate> p;
};

// Equivalent to App::ghostRefInstance()->preferences()
const Preferences *appPrefs();
