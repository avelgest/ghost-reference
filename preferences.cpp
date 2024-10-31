#include "preferences.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <utility>

#include "app.h"
#include "global_hotkeys.h"

namespace
{
    using enum Preferences::Keys;

    const QString g_nullString;
    const PrefEnumItem g_nullEnumPrefItem;

    const char *const configName = "ghost_reference_config.json";

    QDir getConfigDir()
    {
        return {Preferences::configDir()};
    }

    const auto BoolType = QMetaType::Bool;
    const auto FloatType = QMetaType::Double;
    const auto IntType = QMetaType::Int;
    const auto StrType = QMetaType::QString;

    class PrefProp
    {
        QString m_name;
        QMetaType::Type m_type = QMetaType::UnknownType;
        QVariant m_defaultValue;
        QString m_displayName;
        QString m_description;

        PrefFloatRange m_range = {};
        PrefIntRange m_intRange = {};
        const QVector<PrefEnumItem> *m_enumValues = nullptr;

    public:
        PrefProp() = default;
        PrefProp(
            QString name,
            QMetaType::Type type,
            QVariant defaultValue,
            QString displayName,
            QString description)
            : m_name(std::move(name)),
              m_type(type),
              m_defaultValue(std::move(defaultValue)),
              m_displayName(std::move(displayName)),
              m_description(std::move(description))
        {
        }

        // Constructor for bool properties
        PrefProp(QString name, bool defaultValue, QString displayName, QString description)
            : PrefProp(std::move(name), BoolType, defaultValue, std::move(displayName), std::move(description))
        {}

        // Constructor for float properties
        PrefProp(QString name, qreal defaultValue, QString displayName, QString description, PrefFloatRange range)
            : PrefProp(std::move(name), FloatType, defaultValue, std::move(displayName), std::move(description))
        {
            m_range = range;
            m_intRange = {qRound(range.min), qRound(range.max)};
        }

        // Constructor for string / enum properties
        PrefProp(QString name, const QString &defaultValue, QString displayName, QString description,
                 const QVector<PrefEnumItem> *enumValues)
            : PrefProp(std::move(name), StrType, defaultValue, std::move(displayName), std::move(description))
        {
            m_enumValues = enumValues;
        }

        // Constructor for int properties
        PrefProp(QString name, qint32 defaultValue, QString displayName, QString description, PrefIntRange range)
            : PrefProp(std::move(name), IntType, defaultValue, std::move(displayName), std::move(description))
        {
            m_intRange = range;
            m_range = {static_cast<qreal>(range.min), static_cast<qreal>(range.max)};
        };

        const QString &name() const { return m_name; }
        QMetaType::Type type() const { return m_type; }
        const QVariant &defaultValue() const { return m_defaultValue; }
        const QString &displayName() const { return m_displayName; }
        const QString &description() const { return m_description; }
        const PrefFloatRange &range() const { return m_range; }
        const PrefIntRange &intRange() const { return m_intRange; }

        const PrefEnumItem &enumItem(int idx) const
        {
            if (isEnum() && idx >= 0 && idx < m_enumValues->length())
            {
                return m_enumValues->at(idx);
            }
            return g_nullEnumPrefItem;
        }

        bool isCompatible(const QVariant &value) const
        {
            return QMetaType::canConvert(QMetaType(m_type), value.metaType());
        }
        bool isCompatible(const QMetaType &type) const { return QMetaType::canConvert(QMetaType(m_type), type); }
        bool isEnum() const { return m_enumValues != nullptr; }
        bool isValid() const
        {
            return m_type != QMetaType::UnknownType;
        }
    };

    const auto &prefProperties()
    {
        static const QHash<Preferences::Keys, PrefProp> prefMap = {
            {AllowInternet,
             {"allowInternet", BoolType, true, "Allow Network Access",
              "Allow references dragged from a browser or from internet URLs."}},
            {AnimateToolbarCollapse,
             {"animateToolBarCollapse", BoolType, true, "Animate Toolbar",
              "Animate collapsing/expanding the toolbar."}},
            {AskSaveBeforeClosing,
             {"askSaveBeforeClosing", BoolType, true, "Ask to save when exiting",
              "Ask to save any unsaved changes when closing the application."}},
            {GhostModeOpacity, {"ghostModeOpacity", 0.5, "Ghost Mode Opacity", "", {0., 1.}}},
            {GlobalHotkeysEnabled,
             {"globalHotkeysEnabled", true, "Global Hotkeys",
              "Enable global hotkeys (hotkeys that work event when another application is focused)."}},
            {LocalFilesLink,
             {"localFilesLink", BoolType, false, "Link Local Files by Default",
              "Default to storing local files as links when saving the session instead of creating copies."}},
            {LocalFilesStoreMaxMB,
             {"localFilesStoreMaxMB", IntType, 128, "Link Files Larger Than (MB)",
              "Always link local files that are larger than this."}},
            {LoggingEnabled,
             {"loggingEnabled", BoolType, true, "Enable Logging",
              "Write a log file to disk to assist with debugging."}},
            {OverrideKeyAlt, {"overrideKeyAlt", BoolType, true, "Alt", ""}},
            {OverrideKeyCtrl, {"overrideKeyCtrl", BoolType, false, "Ctrl", ""}},
            {OverrideKeyShift, {"overrideKeyShift", BoolType, false, "Shift", ""}},
            {UndoMaxSteps,
             {"undoMaxSteps",
              32,
              "Max undo steps",
              "The maximum number of undo steps to keep. 0 to disable undo.",
              {0, 1024}}}};
        return prefMap;
    }

    Preferences::Keys nameToKey(const QString &name)
    {
        static QHash<const QString, Preferences::Keys> nameMap;
        if (nameMap.isEmpty())
        {
            for (auto [key, value] : prefProperties().asKeyValueRange())
            {
                nameMap.insert(value.name(), key);
            }
        }

        return nameMap[name];
    }

    QJsonObject hotkeyMapToJson(const Preferences::HotkeyMap &hotkeys)
    {
        QJsonObject obj;
        for (const auto [key, value] : hotkeys.asKeyValueRange())
        {
            if (!value.isEmpty())
            {
                obj[key] = value.toString(QKeySequence::PortableText);
            }
        }

        return obj;
    }

    Preferences::HotkeyMap jsonToHotkeyMap(const QJsonValue &jsonValue, bool isGlobalHotkeys)
    {
        using HotkeyMap = Preferences::HotkeyMap;

        HotkeyMap hotkeys = isGlobalHotkeys ? Preferences::defaultGlobalHotkeys() : Preferences::defaultHotkeys();

        if (jsonValue.isObject())
        {
            const QJsonObject obj = jsonValue.toObject();
            for (auto it = obj.constBegin(); it < obj.constEnd(); it++)
            {
                hotkeys[it.key()] = QKeySequence::fromString(it.value().toString(), QKeySequence::PortableText);
            }

            return hotkeys;
        }

        return hotkeys;
    }

} // namespace

struct PreferencesPrivate
{
    QHash<Preferences::Keys, QVariant> m_properties;
    Preferences::HotkeyMap m_hotkeys;
    Preferences::HotkeyMap m_globalHotkeys;
};

Preferences::Preferences(QObject *parent)
    : QObject(parent),
      p(new PreferencesPrivate())
{}

Preferences::~Preferences() = default;

const Preferences::HotkeyMap &Preferences::defaultHotkeys()
{
    static const HotkeyMap hotkeys{};
    return hotkeys;
}

const Preferences::HotkeyMap &Preferences::defaultGlobalHotkeys()
{

    static HotkeyMap globalHotkeyDefaults;
    if (globalHotkeyDefaults.isEmpty())
    {
        for (const auto &hotkey : GlobalHotkeys::builtIns())
        {
            globalHotkeyDefaults[hotkey.name] = hotkey.key;
        }
    }
    return globalHotkeyDefaults;
}

Preferences *Preferences::duplicate(QObject *parent) const
{
    auto *newPrefs = new Preferences(parent);
    newPrefs->p->m_properties = p->m_properties;
    newPrefs->p->m_hotkeys = p->m_hotkeys;
    newPrefs->p->m_globalHotkeys = p->m_globalHotkeys;
    return newPrefs;
}

void Preferences::copyFromOther(const Preferences *other)
{
    if (other)
    {
        p->m_properties = other->p->m_properties;
        p->m_hotkeys = other->p->m_hotkeys;
        p->m_globalHotkeys = other->p->m_globalHotkeys;
    }
}

template <typename T>
T Preferences::get(Keys key) const
{
    return getVariant(key).value<T>();
}

QVariant Preferences::getVariant(Keys key) const
{
    QVariant prop = p->m_properties[key];
    if (!prop.isValid())
    {
        const PrefProp propData = prefProperties()[key];
        if (!propData.isValid())
        {
            qCritical() << "Attempted to get nonexistent preference:" << key;
            return {};
        }
        return propData.defaultValue();
    }
    return prop;
}

void Preferences::set(Keys key, const QVariant &value)
{
    if (key == InvalidPreference)
    {
        return;
    }
    const PrefProp &propData = prefProperties()[key];
    if (!propData.isValid())
    {
        qCritical() << "Attempted to set nonexistent preference:" << key;
        return;
    }
    if (!propData.isCompatible(value))
    {
        qCritical() << "Type is incompatible with" << key;
        return;
    }
    p->m_properties[key] = value;
}

void Preferences::setFloat(Keys key, qreal value)
{
    if (key == InvalidPreference) return;

    // TODO Avoid double lookup from prefProperties here and in Preferences::set
    const PrefProp &propData = prefProperties()[key];

    value = std::clamp(value, propData.range().min, propData.range().max);
    set(key, value);
}

void Preferences::setInt(Keys key, int value)
{
    if (key == InvalidPreference) return;

    const PrefProp &propData = prefProperties()[key];

    value = std::clamp(value, propData.intRange().min, propData.intRange().max);
    set(key, value);
}

Preferences::HotkeyMap &Preferences::hotkeys()
{
    return p->m_hotkeys;
}

const Preferences::HotkeyMap &Preferences::hotkeys() const
{
    return p->m_hotkeys;
}

Preferences::HotkeyMap &Preferences::globalHotkeys()
{
    return p->m_globalHotkeys;
}

const Preferences::HotkeyMap &Preferences::globalHotkeys() const
{
    return p->m_globalHotkeys;
}

void Preferences::resetHotkey(const QString &hotkeyName, bool globalHotkey)
{
    const HotkeyMap &defaults = globalHotkey ? defaultGlobalHotkeys() : defaultHotkeys();
    HotkeyMap &hotkeys = globalHotkey ? p->m_globalHotkeys : p->m_hotkeys;
    hotkeys[hotkeyName] = defaults[hotkeyName];
}

Qt::KeyboardModifiers Preferences::overrideKeys() const
{
    Qt::KeyboardModifiers modifiers;
    if (getBool(OverrideKeyAlt)) modifiers |= Qt::AltModifier;
    if (getBool(OverrideKeyCtrl)) modifiers |= Qt::ControlModifier;
    if (getBool(OverrideKeyShift)) modifiers |= Qt::ShiftModifier;

    return modifiers;
}

Preferences *Preferences::loadFromDisk(QObject *parent)
{
    const QDir configDir = getConfigDir();
    if (configDir.exists(configName))
    {
        QFile configFile(configDir.absoluteFilePath(configName));
        if (configFile.open(QIODevice::ReadOnly))
        {
            const QByteArray json = configFile.readAll();
            Preferences *loaded = loadFromJson(QJsonDocument::fromJson(json), parent);
            if (loaded)
            {
                qInfo() << "Loaded preferences from" << configFile.fileName();
                return loaded;
            }
            qCritical() << "Error reading from config";
        }
        else
        {
            qCritical() << "Unable to open config:" << configFile.fileName();
        }
    }
    else
    {
        qInfo() << "Config file not found in" << configDir.absolutePath() << "Using default preferences.";
    }

    // Return default preferences if loading the config file failed
    return new Preferences(parent);
}

Preferences *Preferences::loadFromJson(const QJsonDocument &json, QObject *parent)
{
    auto prefs = QScopedPointer(new Preferences(parent));

    if (!json.isObject())
    {
        return nullptr;
    }
    const QJsonObject obj = json.object();
    for (auto it = obj.constBegin(); it < obj.constEnd(); it++)
    {
        if (it.key() != "hotkeys" && it.key() != "globalHotkeys")
        {
            prefs->set(nameToKey(it.key()), it.value().toVariant());
        }
    }

    // Load hotkeys / global hotkeys
    prefs->p->m_hotkeys = jsonToHotkeyMap(obj["hotkeys"], false);
    prefs->p->m_globalHotkeys = jsonToHotkeyMap(obj["globalHotkeys"], true);

    return prefs.take();
}

QJsonDocument Preferences::toJsonDocument() const
{
    QJsonObject obj;
    for (const auto [key, value] : p->m_properties.asKeyValueRange())
    {
        const auto &propData = prefProperties()[key];
        if (propData.isValid())
        {
            QJsonValue jsonValue = QJsonValue::fromVariant(value);
            if (jsonValue.isNull())
            {
                jsonValue = QJsonValue::fromVariant(propData.defaultValue());
            }
            obj[propData.name()] = jsonValue;
        }
    }

    obj["hotkeys"] = hotkeyMapToJson(p->m_hotkeys);
    obj["globalHotkeys"] = hotkeyMapToJson(p->m_globalHotkeys);

    return QJsonDocument(obj);
}

void Preferences::saveToDisk() const
{
    const QDir dir = getConfigDir();
    if (!dir.exists())
    {
        dir.mkpath(dir.path());
    }
    QFile configFile(dir.absoluteFilePath(configName));
    if (!configFile.open(QIODevice::WriteOnly))
    {
        qCritical() << "Unable to write to config file: " << configFile.fileName();
        return;
    }

    const QJsonDocument doc = toJsonDocument();
    configFile.write(doc.toJson());
    qInfo() << "Preferences written to" << configFile.fileName();
}

QString Preferences::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool Preferences::checkAllEqual(const Preferences *other) const
{
    if (other == this)
    {
        return true;
    }
    if (other == nullptr)
    {
        return false;
    }
    static const auto keys = prefProperties().keys();

    if (std::ranges::any_of(keys.cbegin(), keys.cend(),
                            [&](auto key) { return getVariant(key) != other->getVariant(key); }))
    {
        return false;
    }

    return (p->m_hotkeys == other->p->m_hotkeys && p->m_globalHotkeys == other->p->m_globalHotkeys);
}

const QString &Preferences::getName(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().name() : g_nullString;
}

const QString &Preferences::getDisplayName(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().displayName() : g_nullString;
}

const QString &Preferences::getDescription(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().description() : g_nullString;
}

const PrefEnumItem &Preferences::getEnumItem(Keys key, int idx)
{
    if (const auto it = prefProperties().find(key); it != prefProperties().end() && it->isEnum())
    {
        return it->enumItem(idx);
    }
    return g_nullEnumPrefItem;
}

PrefFloatRange Preferences::getFloatRange(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().range() : PrefFloatRange();
}

PrefIntRange Preferences::getIntRange(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().intRange() : PrefIntRange();
}

QMetaType::Type Preferences::getType(Keys key)
{
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().type() : QMetaType::Void;
}

// Explicit template initialization
#define TEMPLATE_INST(type) template type Preferences::get<type>(Keys) const;
TEMPLATE_INST(bool)
TEMPLATE_INST(int)
TEMPLATE_INST(qreal)
TEMPLATE_INST(QString)
#undef TEMPLATE_INST

const Preferences *appPrefs()
{
    return App::ghostRefInstance()->preferences();
}
