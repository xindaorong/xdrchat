#include "httpmgr.h"
#include "global.h"
HttpMgr::HttpMgr() 
{
    //连接http请求和完成信号，信号槽机制保证队列消费
    // WRONG: this skipped the module dispatch slot.
    // connect(this,&HttpMgr::sig_http_finish,this,&HttpMgr::sig_reg_mod_finish);
    connect(this,&HttpMgr::sig_http_finish,this,&HttpMgr::slot_http_finish);
}

// WRONG: httpmgr.h declared ~HttpMgr(), but there was no definition here.
HttpMgr::~HttpMgr() = default;

void HttpMgr::PostHttpReq(const QUrl& url,const QJsonObject& json,ReqId req_id,Modules mod)
{
    //创建一个HTTP Post请求,并设置请求头和请求体
    QByteArray data=QJsonDocument(json).toJson();
    QJsonObject logJson = json;
    if (logJson.contains("passwd")) {
        logJson["passwd"] = "***";
    }
    if (logJson.contains("confirm")) {
        logJson["confirm"] = "***";
    }
    qDebug() << "http post url:" << url
             << "req_id:" << req_id
             << "module:" << mod
             << "body:" << QJsonDocument(logJson).toJson(QJsonDocument::Compact);
    //通过url构造请求
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length()));
    //发送请求,并处理响应，获取自己的智能指针，构造伪闭包并增加智能指针引用计数
    auto self=shared_from_this();
    QNetworkReply* reply=_manager.post(request,data);
    //设置信号和槽等待发送完成
    QObject::connect(reply, &QNetworkReply::finished, [reply, self, req_id, mod, url]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 处理错误或成功
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "http error url:" << url
                     << "status:" << statusCode
                     << "error:" << reply->errorString();
            emit self->sig_http_finish(req_id, "", ErrorCode::ERR_NETWORK, mod);
        } else {
            QString res = reply->readAll();
            qDebug() << "http response url:" << url
                     << "status:" << statusCode
                     << "req_id:" << req_id
                     << "module:" << mod
                     << "raw:" << res;
            emit self->sig_http_finish(req_id, res, ErrorCode::Success, mod);
        }
        reply->deleteLater();
    // WRONG: connect needs ");" after the lambda body.
    // }
    });  // ✅ 正确：lambda 体结束的 } 后紧跟 ); 结束 connect 语句
}
// WRONG: sig_http_finish is a signal, so do not implement it as a normal function.
// void HttpMgr::sig_http_finish(ReqId id, QString res, ErrorCode err, Modules mod)
void HttpMgr::slot_http_finish(ReqId id, QString res, ErrorCode err, Modules mod)
{
    if(mod==Modules::REGISTERMOD)
    {
        //发送信号通知指定模版http响应结束
        emit sig_reg_mod_finish(id,res,err);
    }

    if(mod==Modules::RESETMOD)
    {
        //发送信号通知指定模块http响应结束
        emit sig_reset_mod_finish(id,res,err);
    }
    if(mod==Modules::LOGINMOD)
    {
        emit sig_login_mod_finish(id,res,err);
    }
}
