#pragma once

#include <memory>

#include <QtCore/QFuture>
#include <QtCore/QPromise>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include "types.h"
#include "utils/network_download.h"
#include "utils/result.h"

class QDragEnterEvent;
class QDropEvent;
class QMimeData;
class QUrl;
class QVariant;

template <typename T>
class QList;
template <typename T>
class QFuture;

namespace refLoad
{
    using RefImageList = QList<ReferenceImageSP>;

    ReferenceImageSP fromFilepath(const QString &filepath);
    ReferenceImageSP fromPixmap(const QPixmap &pixmap);
    ReferenceImageSP fromUrl(const QUrl &url);

    QList<ReferenceImageSP> fromClipboard();
    QList<ReferenceImageSP> fromDropEvent(const QDropEvent *event);
    QList<ReferenceImageSP> fromMimeData(const QMimeData *mimeData);

    bool isSupported(const QMimeData *mimeData);
    bool isSupported(const QDragEnterEvent *event);
    bool isSupported(const QUrl &url);

    bool isSupportedClipboard();

}; // namespace refLoad

class RefLoader
{
    Q_DISABLE_COPY(RefLoader);

private:
    QPromise<QVariant> m_promise;
    QString m_error;

protected:
    QPromise<QVariant> &promise();
    const QPromise<QVariant> &promise() const;
    void setError(const QString &errString);

public:
    RefLoader() = default;

    RefLoader(RefLoader &&) = default;
    RefLoader &operator=(RefLoader &&) = default;

    virtual ~RefLoader() = default;

    bool finished() const;
    bool isError() const;
    virtual bool isValid() const;
    virtual QFuture<QVariant> future() const;

    virtual RefType type() const = 0;
};

class RefImageLoader : public RefLoader
{
    Q_DISABLE_COPY(RefImageLoader)

    std::unique_ptr<utils::NetworkDownload> m_download = nullptr;

public:
    RefImageLoader() = default;
    explicit RefImageLoader(const QUrl &url);
    explicit RefImageLoader(const QString &url);
    explicit RefImageLoader(const QImage &image);
    explicit RefImageLoader(const QPixmap &pixmap);
    ~RefImageLoader() override = default;

    RefImageLoader(RefImageLoader &&) = default;
    RefImageLoader &operator=(RefImageLoader &&) = default;

    QFuture<QVariant> future() const override;
    QPixmap pixmap() const;
    RefType type() const override { return RefType::Image; }
};

inline QPromise<QVariant> &RefLoader::promise() { return m_promise; }
inline const QPromise<QVariant> &RefLoader::promise() const { return m_promise; }

inline void RefLoader::setError(const QString &errString) { m_error = errString; }

inline bool RefLoader::finished() const { return future().isFinished(); }

inline bool RefLoader::isError() const { return !m_error.isEmpty(); }

inline bool RefLoader::isValid() const { return finished(); }
