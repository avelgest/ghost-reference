#include "reference_collection.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#include "reference_image.h"
#include "reference_loading.h"

// TODO Implement cleaning to remove old null pointers when adding a reference item

ReferenceCollection::ReferenceCollection()
    : m_refMap(std::make_unique<ReferenceMap>())
{}

ReferenceImageSP ReferenceCollection::newReferenceImage(const QString &name)
{
    ReferenceMap &refMap = *m_refMap;

    const auto refImage = ReferenceImageSP(new ReferenceImage());
    refImage->m_name = uniqueReferenceName(name.isEmpty() ? "Untitled" : name);

    refMap[refImage->name()] = refImage;

    return refImage;
}

ReferenceImageSP ReferenceCollection::getReferenceImage(const QString &name)
{
    const auto found = m_refMap->find(name);
    if (found != m_refMap->end() && !found.value().isNull())
    {
        return found.value().toStrongRef();
    }
    return {};
}

void ReferenceCollection::renameReference(ReferenceImage &refItem, const QString &newName, bool force)
{
    ReferenceMap &refMap = *m_refMap;
    const QString oldName = refItem.name();
    if (newName == oldName)
    {
        return;
    }

    const ReferenceImageSP refItemSP = getReferenceImage(oldName);
    if (!refItemSP)
    {
        return;
    }

    QString uniqueName = uniqueReferenceName(newName.trimmed(), refItemSP);
    if (uniqueName.isEmpty())
    {
        return;
    }

    // When force is true always set refItemSP's name to newName and rename any other
    // ReferenceImage with that name.
    if (force)
    {
        if (const ReferenceImageSP existing = getReferenceImage(newName); existing)
        {
            renameReference(*existing, uniqueName, false);
            uniqueName = newName;
        }
        Q_ASSERT(uniqueName == newName);
    }

    refItemSP->m_name = uniqueName;
    refMap[uniqueName] = refItemSP;
    refMap.remove(oldName);
}

QList<ReferenceImageSP> ReferenceCollection::loadJson(const QJsonObject &json,
                                                      const QMap<QString, QByteArray> &imageData)
{
    QList<ReferenceImageSP> loadedRefs;

    for (auto it = json.constBegin(); it != json.constEnd(); it++)
    {
        const QJsonObject jsonObj = it->toObject();
        if (jsonObj.isEmpty())
        {
            qCritical() << "Invalid JSON value for " << it.key();
            continue;
        }

        const auto imageFound = imageData.find(it.key());
        RefImageLoaderUP loader;
        if (imageFound != imageData.end())
        {
            loader = std::move(std::make_unique<RefImageLoader>(imageFound.value()));
        }

        // N.B. Linked (not stored in the .ghr) files will be loaded in refImage->fromJson

        ReferenceImageSP refImage = newReferenceImage();
        refImage->fromJson(jsonObj, std::move(loader));
        loadedRefs.push_back(std::move(refImage));
    }
    return loadedRefs;
}

QJsonObject ReferenceCollection::toJson() const
{
    QJsonObject json;
    for (const auto &val : *m_refMap)
    {
        if (!val.isNull())
        {
            const ReferenceImageSP refItem = val.toStrongRef();
            json[refItem->name()] = refItem->toJson();
        }
    }
    return json;
}

QList<ReferenceImageSP> ReferenceCollection::references() const
{
    QList<ReferenceImageSP> outList;
    for (auto &value : m_refMap->values())
    {
        if (!value.isNull())
        {
            outList.emplace_back(value.toStrongRef());
        }
    }
    return outList;
}

void ReferenceCollection::clear()
{
    m_refMap->clear();
}

QString ReferenceCollection::uniqueReferenceName(const QString &basename, const ReferenceImageSP &ignore)
{
    const ReferenceMap &refMap = *m_refMap;
    auto found = refMap.find(basename);
    if (found == refMap.end() || found.value().isNull())
    {
        return basename;
    }

    // Already a reference with this name so add a number in brackets to basename
    int suffixNum = 1;
    QString fmtStr;

    // Check for an existing numeric suffix;
    static const QRegularExpression re(R"(.*\((\d+)\))");
    if (const QRegularExpressionMatch match = re.match(basename); match.hasMatch())
    {
        suffixNum = match.capturedTexts().last().toInt();
        qDebug() << match.capturedTexts().last() << suffixNum;
        suffixNum = std::clamp(suffixNum, 1, INT_MAX / 2) + 1;
        fmtStr = QString(basename).replace(match.capturedStart(1), match.capturedLength(1), "%1");
    }
    else
    {
        suffixNum = 1;
        fmtStr = basename + " (%1)";
    }

    for (; suffixNum < INT_MAX; suffixNum++)
    {
        const QString newName = fmtStr.arg(suffixNum);

        if (auto found = refMap.find(newName);
            found == refMap.end() || found.value().isNull() || found.value() == ignore)
        {
            return newName;
        }
    }
    qCritical() << "Unable to create unique name for " << basename;
    return basename;
}
