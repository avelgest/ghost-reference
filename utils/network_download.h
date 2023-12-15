#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QPromise>

class QNetworkReply;

namespace utils
{
    class NetworkDownload : public QObject
    {
        Q_DISABLE_COPY_MOVE(NetworkDownload);

        QPromise<QByteArray> m_promise;
        QNetworkReply *m_networkReply = nullptr;

    public:
        explicit NetworkDownload(const QUrl &url, QObject *parent = nullptr);
        ~NetworkDownload() override;

        int statusCode() const;

        QFuture<QByteArray> future() const;
        const QPromise<QByteArray> &promise() const;

    protected:
        void deleteNetworkReply();

    private slots:
        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onFinished();
    };
} // namespace utils
