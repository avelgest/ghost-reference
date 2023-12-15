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

    // ReferenceCollection(ReferenceCollection &&) = default;
    // ReferenceCollection &operator=(ReferenceCollection &&) = default;

    ReferenceImageSP newReferenceImage();
    ReferenceImageSP getReferenceImage(const QString &name);
    void renameReference(ReferenceImage &refItem, const QString &newName);

    QList<ReferenceImageSP> loadJson(const QJsonObject &json, const QMap<QString, QPixmap> &pixmaps);
    QJsonObject toJson() const;

    QList<ReferenceImageSP> references() const;

protected:
    QString uniqueReferenceName(const QString &basename);
};
