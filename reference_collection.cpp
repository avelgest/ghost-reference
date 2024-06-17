#include "reference_collection.h"

#include <QtCore/QDebug>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
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

void ReferenceCollection::renameReference(ReferenceImage &refItem, const QString &newName)
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
    const QString uniqueName = uniqueReferenceName(newName);
    if (uniqueName.isEmpty())
    {
        return;
    }

    refItemSP->m_name = uniqueName;
    refMap[uniqueName] = refItemSP;
    refMap.remove(oldName);
}

QList<ReferenceImageSP> ReferenceCollection::loadJson(const QJsonObject &json,
                                                      const QMap<QString, QPixmap> &pixmaps)
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

        const auto pixmapFound = pixmaps.find(it.key());
        RefImageLoader loader;
        if (pixmapFound != pixmaps.end())
        {
            loader = std::move(RefImageLoader(pixmapFound.value()));
        }

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

QString ReferenceCollection::uniqueReferenceName(const QString &basename)
{
    const ReferenceMap &refMap = *m_refMap;
    auto found = refMap.find(basename);
    if (found == refMap.end() || found.value().isNull())
    {
        return basename;
    }

    const QString fmtStr = basename + "(%1)";

    for (int x = 1; x < INT_MAX; x++)
    {
        const QString newName = fmtStr.arg(x);

        if (auto found = refMap.find(newName); found == refMap.end() || found.value().isNull())
        {
            return newName;
        }
    }
    qCritical() << "Unable to create unique name for " << basename;
    return basename;
}
