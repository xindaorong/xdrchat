
#include "logindialog.h"
#include "ui_logindialog.h"

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
