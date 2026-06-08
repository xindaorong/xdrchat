#include "registerdialog.h"
#include "ui_registerdialog.h"

#include "global.h"
#include "httpmgr.h"

#include <QJsonDocument>
#include <QLineEdit>
#include <QRegularExpression>
#include <QDebug>
#include<global.h>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    ui->err_tip->setStyleSheet(
        "#err_tip[state='normal']{ color: green; }"
        "#err_tip[state='err']{ color: red; }"
    );
    ui->err_tip->setProperty("state", "normal");
    repolish(ui->err_tip);
    ui->stackedWidget->setCurrentIndex(0);
    ui->confirm_edit_4->setEchoMode(QLineEdit::Password);
    ui->pass_edit_3->setEchoMode(QLineEdit::Password);

    // 修正：原来这里只是连了信号，但没有初始化处理表；这样回包后不会知道该怎么处理
    initHttpHandlers();
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_reg_mod_finish,
            this, &RegisterDialog::slot_reg_mod_finish);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

// 显示提示信息，并切换样式
void RegisterDialog::showTip(const QString &str, bool b_ok)
{
    ui->err_tip->setText(str);
    ui->err_tip->setProperty("state", b_ok ? "normal" : "err"); // 修正：成功/失败状态要根据 b_ok 切换
    repolish(ui->err_tip);
}

void RegisterDialog::on_get_code_clicked()
{
    auto email = ui->email_edit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool is_match = regex.match(email).hasMatch();
    if (!is_match)
    {
        showTip(tr("请输入正确的邮箱格式"), false);
        return;
    }

    ui->err_tip->setText(tr("邮箱格式正确"));
    ui->err_tip->setProperty("state", "normal");
    repolish(ui->err_tip);
    //发送http请求获取验证码
    QJsonObject json_obj;
    json_obj["email"]=email;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/get_verifycode"),
                                        json_obj, ReqId::ID_GET_VERIFY_CODE,Modules::REGISTERMOD);
}

void RegisterDialog::slot_reg_mod_finish(ReqId id, QString res, ErrorCode err)
{
    if (err != ErrorCode::Success)
    {
        showTip(tr("获取验证码失败，请检查网络"), false);
        return;
    }

    // 修正：服务端返回的是 JSON 字符串，必须先解析再取字段
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull())
    {
        showTip(tr("服务器返回数据异常"), false);
        return;
    }
    if (!jsonDoc.isObject())
    {
        showTip(tr("JSON 解析错误"), false);
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if (_handlers.contains(id))
    {
        _handlers[id](jsonObj);
    }
}

void RegisterDialog::initHttpHandlers()
{
    // 注册“获取验证码”回包逻辑
    _handlers.insert(ReqId::ID_GET_VERIFY_CODE, [this](const QJsonObject &jsonObj) {
        const int error = jsonObj["error"].toInt();
        if (error != static_cast<int>(ErrorCode::Success))
        {
            showTip(tr("参数错误"), false);
            return;
        }

        const auto email = jsonObj["email"].toString();
        showTip(tr("验证码已发送到邮箱，请注意查收"), true);
        qDebug() << "email is" << email;
    });
    //注册用户回报逻辑
    _handlers.insert(ReqId::ID_REG_USER, [this](const QJsonObject &jsonObj) {
        const int error = jsonObj["error"].toInt();
        if (error != static_cast<int>(ErrorCode::Success))
        {
            showTip(tr("参数错误"), false);
            return;
        }

        const auto email = jsonObj["email"].toString();
        showTip(tr("用户注册成功"), true);
        qDebug() << "email is" << email;
    });
}

void RegisterDialog::on_sure_btn_clicked()
{
    //1首先进行非空性校验
    if(ui->use_edit->text()=="")
    {
        showTip(tr("用户名不能为空"),false);
        return ;
    }
    if(ui->email_edit->text() == ""){
        showTip(tr("邮箱不能为空"), false);
        return;
    }

    if(ui->pass_edit_3->text() == ""){
        showTip(tr("密码不能为空"), false);
        return;
    }

    if(ui->confirm_edit_4->text() == ""){
        showTip(tr("确认密码不能为空"), false);
        return;
    }

    //2密码一致性校验
    if(ui->confirm_edit_4->text() != ui->pass_edit_3->text()){
        showTip(tr("密码和确认密码不匹配"), false);
        return;
    }
    //3验证码校验
    if(ui->verify_edit_5->text() == ""){
        showTip(tr("验证码不能为空"), false);
        return;
    }

    //4构造 HTTP 请求数据（JSON 格式）把用户输入的用户名、邮箱、密码、确认密码、验证码 打包成一个 JSON 对象，准备发给后端。
    QJsonObject json_obj;
    json_obj["user"] = ui->use_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = ui->pass_edit_3->text();
    json_obj["confirm"] = ui->confirm_edit_4->text();
    json_obj["varifycode"] = ui->verify_edit_5->text();

    //5发送 POST 注册请求
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                                        json_obj,ReqId::ID_REG_USER,Modules::REGISTERMOD);
}

