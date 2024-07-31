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

namespace
{
    using enum Preferences::Keys;

    const QString g_nullString;
    const PrefEnumItem g_nullEnumPrefItem;

    const char *const configName = "ghost_reference_config.json";

    QDir configDir()
    {
        const QString configDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return {configDirPath};
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

        // Constructor for float Props
        PrefProp(QString name, qreal defaultValue,
                 QString displayName, QString description,
                 PrefFloatRange range)
            : PrefProp(std::move(name), FloatType,
                       std::move(defaultValue), std::move(displayName), std::move(description))
        {
            m_range = range;
        }

        // Constructor for string / enum properties
        PrefProp(QString name, QString defaultValue,
                 QString displayName, QString description,
                 const QVector<PrefEnumItem> *enumValues)
            : PrefProp(std::move(name), StrType,
                       std::move(defaultValue), std::move(displayName), std::move(description))
        {
            m_enumValues = enumValues;
        }

        const QString &name() const { return m_name; }
        QMetaType::Type type() const { return m_type; }
        const QVariant &defaultValue() const { return m_defaultValue; }
        const QString &displayName() const { return m_displayName; }
        const QString &description() const { return m_description; }
        const PrefFloatRange &range() const { return m_range; }

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
            {LocalFilesLink,
             {"localFilesLink", BoolType, false, "Link Local Files by Default",
              "Default to storing local files as links when saving the session instead of creating copies."}},
            {LocalFilesStoreMaxMB,
             {"localFilesStoreMaxMB", IntType, 128, "Link Files Larger Than (MB)",
              "Always link local files that are larger than this."}},
        };
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

} // namespace

struct PreferencesPrivate
{
    QHash<Preferences::Keys, QVariant> m_properties;
};

Preferences::Preferences(QObject *parent)
    : QObject(parent),
      p(new PreferencesPrivate())
{
}

Preferences::~Preferences() = default;

Preferences *Preferences::duplicate(QObject *parent) const
{
    auto *newPrefs = new Preferences(parent);
    newPrefs->p->m_properties = p->m_properties;
    return newPrefs;
}

void Preferences::copyFromOther(Preferences *other)
{
    if (other)
    {
        p->m_properties = other->p->m_properties;
    }
}

template <typename T>
T Preferences::get(Keys key) const
{
    const QVariant prop = p->m_properties[key];
    if (!prop.isValid())
    {
        const PrefProp propData = prefProperties()[key];
        if (!propData.isValid())
        {
            qCritical() << "Attempted to get nonexistent preference:" << key;
            return {};
        }
        return propData.defaultValue().value<T>();
    }
    return prop.value<T>();
}

void Preferences::set(Keys key, const QVariant &value)
{
    if (key == InvalidPreference)
    {
        return;
    }
    const PrefProp propData = prefProperties()[key];
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

Preferences *Preferences::loadFromDisk(QObject *parent)
{
    const QString configDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QDir configDir(configDirPath);
    if (configDir.exists(configName))
    {
        QFile configFile(configDir.absoluteFilePath(configName));
        if (configFile.open(QIODevice::ReadOnly))
        {
            const QByteArray json = configFile.readAll();
            Preferences *loaded = loadFromJson(QJsonDocument::fromJson(json), parent);
            if (loaded)
            {
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
        qInfo() << "Config file not found. Using default preferences.";
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
    for (auto it = json.object().constBegin(); it < json.object().constEnd(); it++)
    {
        prefs->set(nameToKey(it.key()), it.value().toVariant());
    }

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
    return QJsonDocument(obj);
}

void Preferences::saveToDisk() const
{
    const QDir dir = configDir();
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

bool Preferences::checkAllEqual(Preferences *other)
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

    auto &props = p->m_properties;
    auto &otherProps = other->p->m_properties;

    return std::ranges::all_of(keys.cbegin(), keys.cend(), [=](auto key) { return props[key] == otherProps[key]; });
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
