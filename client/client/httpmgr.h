#ifndef HTTPMGR_H
#define HTTPMGR_H

#include "global.h"
#include "singleton.h"

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

class HttpMgr : public QObject, public Singleton<HttpMgr>,
                public std::enable_shared_from_this<HttpMgr>
{
    Q_OBJECT

public:
    ~HttpMgr();

    // 发送 HTTP POST 请求
    void PostHttpReq(const QUrl &url, const QJsonObject &json, ReqId req_id, Modules mod);

signals:
    void sig_http_finish(ReqId id, QString res, ErrorCode err, Modules mod);
    void sig_reg_mod_finish(ReqId id, QString res, ErrorCode err);

private:
    friend class Singleton<HttpMgr>;
    HttpMgr();
    QNetworkAccessManager _manager;

private slots:
    void slot_http_finish(ReqId id, QString res, ErrorCode err, Modules mod);
};

#endif // HTTPMGR_H
