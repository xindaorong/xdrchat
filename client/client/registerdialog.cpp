#include "registerdialog.h"
#include "ui_registerdialog.h"

#include "clickedlabel.h"
#include "global.h"
#include "httpmgr.h"

#include <QDebug>
#include <QJsonDocument>
#include <QLineEdit>
#include <QRegularExpression>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , _countdown_timer(new QTimer(this))
    , _countdown(5)
{
    ui->setupUi(this);
    ui->err_tip->setStyleSheet(
        "#err_tip[state='normal']{ color: green; }"
        "#err_tip[state='err']{ color: red; }"
    );
    ui->err_tip->setProperty("state", "normal");
    repolish(ui->err_tip);
    ui->stackedWidget->setCurrentIndex(0);
    //day11 设定输入框后清空字符串
    ui->err_tip->clear();

    ui->confirm_edit->setEchoMode(QLineEdit::Password);
    ui->pass_edit->setEchoMode(QLineEdit::Password);

    // 当用户在用户名输入框完成编辑（按Enter或焦点移开）时，自动校验用户名合法性
    // sender: ui->user_edit (QLineEdit)
    // signal: editingFinished — 编辑完成时触发一次，而非每次按键
    // receiver: this (RegisterDialog)
    // slot: lambda 捕获 this，调用 checkUserValid() 执行校验逻辑
    connect(ui->user_edit,&QLineEdit::editingFinished,this,[this](){
        checkUserValid();
    });

    connect(ui->email_edit, &QLineEdit::editingFinished, this, [this](){
        checkEmailValid();
    });

    connect(ui->pass_edit, &QLineEdit::editingFinished, this, [this](){
        checkPassValid();
    });

    connect(ui->confirm_edit, &QLineEdit::editingFinished, this, [this](){
        checkConfirmValid();
    });

    connect(ui->varify_edit, &QLineEdit::editingFinished, this, [this](){
        checkVarifyValid();
    });

    //设置浮动显示手形状
    ui->pass_visible->setCursor(Qt::PointingHandCursor);
    ui->confirm_visible->setCursor(Qt::PointingHandCursor);

    ui->pass_visible->SetState("unvisible","unvisible_hover","","visible",
                               "visible_hover","");

    ui->confirm_visible->SetState("unvisible","unvisible_hover","","visible",
                                  "visible_hover","");
    //连接点击事件
    connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
        auto state = ui->pass_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->pass_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->pass_edit->setEchoMode(QLineEdit::Normal);
        }
        qDebug() << "Label was clicked!";
    });
    //这个connect负责在注册页面初始化时建立点击连接关系
    connect(ui->confirm_visible, &ClickedLabel::clicked, this, [this]() {
        auto state = ui->confirm_visible->GetCurState();
        if(state == ClickLbState::Normal){
            ui->confirm_edit->setEchoMode(QLineEdit::Password);
        }else{
            ui->confirm_edit->setEchoMode(QLineEdit::Normal);
        }
        qDebug() << "Label was clicked!";
    });

    // 创建定时器
    connect(_countdown_timer, &QTimer::timeout, [this](){
        if(_countdown==0){
            //主动叫停发送信号--停止计时器
            _countdown_timer->stop();
            //发出登录界面信号，切换到登录界面
            emit sigSwitchLogin();
            return;
        }
        _countdown--;
        //%l是占位符把_countdown的内容填进去
        auto str = QString("注册成功，%1 s后返回登录").arg(_countdown);
        ui->tip_lb->setText(str);
    });

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
    ui->err_tip->setProperty("state", b_ok ? "normal" : "err");
    repolish(ui->err_tip);
}
//添加错误
void RegisterDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    showTip(tips, false);
}
//删除错误
void RegisterDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if(_tip_errs.isEmpty()){
      ui->err_tip->clear();
      return;
    }

    showTip(_tip_errs.first(), false);
}
//实现错误检测
bool RegisterDialog::checkUserValid()
{
    if(ui->user_edit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegisterDialog::checkPassValid()
{
    auto pass = ui->pass_edit->text();

    if(pass.length() < 6 || pass.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 创建一个正则表达式对象，按照上述密码要求
    // 这个正则表达式解释：
    // ^[a-zA-Z0-9!@#$%^&*]{6,15}$ 密码长度至少6，可以是字母、数字和特定的特殊字符
    QRegularExpression regExp("^[a-zA-Z0-9!@#$%^&*]{6,15}$");
    bool match = regExp.match(pass).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);

    return true;
}

bool RegisterDialog::checkEmailValid()
{
    //验证邮箱的地址正则表达式
    auto email = ui->email_edit->text();
    // 邮箱地址的正则表达式
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)") ;
    bool match = regex.match(email).hasMatch(); // 执行正则表达式匹配
    if(!match){
        //提示邮箱不正确
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }

    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegisterDialog::checkConfirmValid()
{
    auto confirm = ui->confirm_edit->text();
    if(confirm.isEmpty()){
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("确认密码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    if(confirm != ui->pass_edit->text()){
        AddTipErr(TipErr::TIP_PWD_CONFIRM, tr("两次密码不一致"));
        return false;
    }

    DelTipErr(TipErr::TIP_PWD_CONFIRM);
    return true;
}

bool RegisterDialog::checkVarifyValid()
{
    auto pass = ui->varify_edit->text();
    if(pass.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void RegisterDialog::on_get_code_clicked()
{
    if(!checkEmailValid()){
        return;
    }

    //发送http请求获取验证码
    QJsonObject json_obj;
    json_obj["email"]=ui->email_edit->text();
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

    // 服务端返回的是 JSON 字符串，必须先解析再取字段
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

    //注册注册用户回包逻辑
    _handlers.insert(ReqId::ID_REG_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != static_cast<int>(ErrorCode::Success)){
            showTip(tr("参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        showTip(tr("用户注册成功"), true);
        qDebug()<< "email is " << email ;
        qDebug()<< "user uuid is " <<  jsonObj["uuid"].toString();
        ChangeTipPage();
    });
}

void RegisterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    _countdown = 5;
    ui->stackedWidget->setCurrentWidget(ui->page_2);

    // 启动定时器，设置间隔为1000毫秒（1秒）
    _countdown_timer->start(1000);
}
//检测所有条件成立之后再发送请求--对那个确认按钮进行逻辑的修改
void RegisterDialog::on_sure_btn_clicked()
{
    bool valid = checkUserValid();
    if(!valid){
        return;
    }

    valid = checkEmailValid();
    if(!valid){
        return;
    }

    valid = checkPassValid();
    if(!valid){
        return;
    }

    valid = checkConfirmValid();
    if(!valid){
        return;
    }

    valid = checkVarifyValid();
    if(!valid){
        return;
    }

    //发送http注册用户请求
    QJsonObject json_obj;
    json_obj["user"] = ui->user_edit->text();
    json_obj["email"] = ui->email_edit->text();
    json_obj["passwd"] = ui->pass_edit->text();
    json_obj["confirm"] = ui->confirm_edit->text();
    json_obj["varifycode"] = ui->varify_edit->text();
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                 json_obj, ReqId::ID_REG_USER,Modules::REGISTERMOD);
}

void RegisterDialog::on_return_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

void RegisterDialog::on_cancel_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}
