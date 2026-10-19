#include "TinyBrowser/NetworkManager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace TinyBrowser {

NetworkManager::NetworkManager(QObject* parent)
  : QObject(parent),
    m_nam(new QNetworkAccessManager(this)) {}

NetworkManager::~NetworkManager() = default;

void NetworkManager::get(const QUrl& url) {
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply* reply = m_nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    onReplyFinished(reply);
  });
}

void NetworkManager::onReplyFinished(QNetworkReply* reply) {
  if (!reply) {
    return;
  }

  const QUrl url = reply->url();
  if (reply->error() == QNetworkReply::NoError) {
    const QString html = QString::fromUtf8(reply->readAll());
    emit finished(html, url);
  } else {
    emit error(reply->errorString());
  }
  reply->deleteLater();
}

} // namespace TinyBrowser