#pragma once

#include <memory>

#include "types.h"

class ReferenceCollection
{
    Q_DISABLE_COPY_MOVE(ReferenceCollection)

    typedef QMap<QString, ReferenceImageWP> ReferenceMap;

    std::unique_ptr<ReferenceMap> m_refMap;

public:
    ReferenceCollection();
    ~ReferenceCollection() = default;

    ReferenceImageSP newReferenceImage(const QString &name = "");
    ReferenceImageSP getReferenceImage(const QString &name);

    // Attempts to set name property of refItem to newName. If a reference item with this name
    // already exists (and force is false) then refItem's new name will not be exactly newName,
    // but have a suffix attached.
    // If force is True then refItems new name will be exactly newName and other reference with the
    // same name will be renamed.
    void renameReference(ReferenceImage &refItem, const QString &newName, bool force = false);

    QList<ReferenceImageSP> loadJson(const QJsonObject &json, const QMap<QString, QByteArray> &imageData);
    QJsonObject toJson() const;

    QList<ReferenceImageSP> references() const;

protected:
    QString uniqueReferenceName(const QString &basename);
};
