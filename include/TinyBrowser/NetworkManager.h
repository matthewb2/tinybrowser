#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace TinyBrowser {

class NetworkManager : public QObject {
  Q_OBJECT

public:
  explicit NetworkManager(QObject* parent = nullptr);
  ~NetworkManager() override;

  void get(const QUrl& url);

signals:
  void finished(const QString& html, const QUrl& url);
  void error(const QString& errorString);

private slots:
  void onReplyFinished(QNetworkReply* reply);

private:
  QNetworkAccessManager* m_nam = nullptr;
};

} // namespace TinyBrowser