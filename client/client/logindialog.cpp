
#include "logindialog.h"
#include "ui_logindialog.h"
#include"httpmgr.h"
#include"tcpmgr.h"
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    //在LoginDialog的构造函数里连接按钮点击事件
    connect(ui->pushButton_2, &QPushButton::clicked, this, &LoginDialog::switchRegister);
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");
    ui->forget_label->setCursor(Qt::PointingHandCursor);
    connect(ui->forget_label,&ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);
    //连接登陆界面忘记密码信号
    initHttpHandlers();
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_login_mod_finish,this,&LoginDialog::slot_login_mod_finish);
    //连接tcp连接请求的信号和槽函数
    connect(this,&LoginDialog::sig_connect_tcp,TcpMgr::GetInstance().get(),&TcpMgr::slot_tcp_connect);
    //连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_con_success,this,&LoginDialog::slot_tcp_con_finish);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed, this, [this](int err) {
        showTip(formatServerError(err), false);
        enableBtn(true);
    });
}
void LoginDialog:: slot_forget_pwd()
{
    qDebug()<<"slot forget pwd";
    emit switchReset();
}
LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_log_btn_clicked()
{
    qDebug()<<"login btn clicked";
    if(checkUserValid() == false){
        return;
    }

    if(checkPwdValid() == false){
        return ;
    }

    auto user = ui->user_lineEdit->text();
    auto pwd = ui->pass_lineEdit->text();
    //发送http请求登录
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = xorString(pwd);
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),
                                        json_obj, ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}
bool LoginDialog::checkUserValid(){

    auto user = ui->user_lineEdit->text();
    if(user.isEmpty()){
        qDebug() << "User empty " ;
        return false;
    }

    return true;
}

bool LoginDialog::checkPwdValid(){
    auto pwd = ui->pass_lineEdit->text();
    if(pwd.length() < 6 || pwd.length() > 15){
        qDebug() << "Pass length invalid";
        return false;
    }

    return true;
}

void LoginDialog::initHttpHandlers()
{
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        qDebug() << "login user response:" << jsonObj
                 << "error:" << error
                 << "message:" << serverErrorToString(error);
        if(error != ErrorCode::Success){
            showTip(formatServerError(error),false);
            return;
        }
        auto user = jsonObj["user"].toString();
        //发送信号通知tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        qDebug()<< "user is " << user << " uid is " << si.Uid <<" host is "
                 << si.Host << " Port is " << si.Port << " Token is " << si.Token;
        emit sig_connect_tcp(si);
    });
}
void LoginDialog::showTip(QString str, bool b_ok)
{
    if(b_ok){
        ui->err_tip->setProperty("state","normal");
    }else{
        ui->err_tip->setProperty("state","err");
    }

    ui->err_tip->setText(str);

    repolish(ui->err_tip);
}
void LoginDialog::slot_login_mod_finish(ReqId id, QString res, ErrorCode err)
{
    qDebug() << "login response id:" << id << "http err:" << err << "raw:" << res;

    if(err != ErrorCode::Success){
        showTip(formatServerError(err),false);
        return;
    }

    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //调用对应的逻辑,根据id回调。
    if (!_handlers.contains(id)) {
        qDebug() << "login handler not found, id:" << id << "json:" << jsonDoc.object();
        showTip(tr("未知请求回包，id：%1").arg(id), false);
        return;
    }

    _handlers[id](jsonDoc.object());

    return;
}
bool LoginDialog::enableBtn(bool enabled)
{
    ui->log_btn->setEnabled(enabled);
    ui->pushButton_2->setEnabled(enabled);
    return true;
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess)
{

    if(bsuccess){
        showTip(tr("聊天服务连接成功，正在登录..."),true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;

        QJsonDocument doc(jsonObj);
        QString jsonString = doc.toJson(QJsonDocument::Indented);

        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN, jsonString);

    }else{
        showTip(tr("网络异常"),false);
        enableBtn(true);
    }

}
