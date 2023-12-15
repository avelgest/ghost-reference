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

#include "app.h"

namespace
{
    using enum Preferences::Keys;

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
        QString m_description;

    public:
        PrefProp() = default;
        PrefProp(QString name, QMetaType::Type type, QVariant defaultValue, QString description = {})
            : m_name(std::move(name)),
              m_type(type),
              m_defaultValue(std::move(defaultValue)),
              m_description(std::move(description))
        {
        }

        const QString &name() const
        {
            return m_name;
        }
        QMetaType::Type type() const { return m_type; }
        const QVariant &defaultValue() const { return m_defaultValue; }
        const QString &description() const { return m_description; }

        bool isCompatible(const QVariant &value) const
        {
            return QMetaType::canConvert(QMetaType(m_type), value.metaType());
        }
        bool isValid() const
        {
            return m_type != QMetaType::UnknownType;
        }
    };

    const auto &prefProperties()
    {
        static const QHash<Preferences::Keys, PrefProp> prefMap = {
            {AllowInternet,
             {"allowInternet", BoolType, true, "Allow references dragged from a browser or from internet links"}},
            {AnimateToolbarCollapse,
             {"animateToolBarCollapse", BoolType, true, "Animate collapsing/expanding the toolbar"}},
            {GhostModeOpacity,
             {"ghostModeOpacity", FloatType, 0.5, ""}},
            {LocalFilesStore,
             {"localFilesLink", BoolType, false, ""}},
            {LocalFilesStoreMaxMB,
             {"localFilesStoreMaxMB", IntType, 128, ""}},
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

const QString &Preferences::getDescription(Keys key)
{
    static const QString nullString;
    const auto it = prefProperties().find(key);
    return (it != prefProperties().end()) ? it.value().description() : nullString;
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
