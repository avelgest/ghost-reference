#include "network_download.h"

#include <QtCore/QByteArray>
#include <QtCore/QFuture>
#include <QtCore/QPromise>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "../app.h"

using NetworkDownload = utils::NetworkDownload;

namespace
{
    const int progressValues = 100;

    QNetworkAccessManager *networkManager()
    {
        return App::ghostRefInstance()->networkManager();
    }

} // namespace

NetworkDownload::NetworkDownload(const QUrl &url, QObject *parent)
    : QObject(parent)
{
    QNetworkAccessManager *manager = networkManager();
    m_networkReply = manager->get(QNetworkRequest(url));

    m_promise.setProgressRange(0, progressValues);
    m_promise.start();

    QObject::connect(m_networkReply, &QNetworkReply::downloadProgress, this, &NetworkDownload::onDownloadProgress);
    QObject::connect(m_networkReply, &QNetworkReply::finished, this, &NetworkDownload::onFinished);
}

NetworkDownload::~NetworkDownload()
{
    deleteNetworkReply();
    m_promise.finish();
}

int NetworkDownload::statusCode() const
{
    return m_networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

bool utils::NetworkDownload::anyError() const
{
    return m_networkReply && m_networkReply->error() != QNetworkReply::NoError;
}

QString utils::NetworkDownload::errorMessage() const
{
    return anyError() ? m_networkReply->errorString() : QString();
}

QUrl utils::NetworkDownload::url() const
{
    return m_networkReply ? m_networkReply->url() : QUrl();
}

QFuture<QByteArray> NetworkDownload::future() const { return m_promise.future(); }

const QPromise<QByteArray> &NetworkDownload::promise() const { return m_promise; }

void NetworkDownload::deleteNetworkReply()
{
    if (m_networkReply)
    {
        m_networkReply->deleteLater();
        m_networkReply = nullptr;
    }
}

void NetworkDownload::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    const qreal fac = static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal);
    const int newProgress = qRound(fac * progressValues);
    m_promise.setProgressValue(newProgress);
}

void NetworkDownload::onFinished()
{
    if (m_networkReply->error() == QNetworkReply::NoError)
    {
        QByteArray data = m_networkReply->readAll();
        m_promise.addResult(std::move(data));
    }
    else
    {
        qCritical() << "Download of" << m_networkReply->url()
                    << "failed with error code" << m_networkReply->error();
    }
    deleteNetworkReply();
    m_promise.finish();
}
