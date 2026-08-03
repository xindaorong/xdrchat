#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H
#include"global.h"
#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

private:
     Ui::LoginDialog *ui;

     void slot_forget_pwd();
     void slot_login_mod_finish(ReqId id, QString res, ErrorCode err);
     void slot_tcp_con_finish(bool bsuccess);

     bool checkUserValid();
     bool checkPwdValid();
     void initHttpHandlers();
     void showTip(QString str,bool b_ok);
     bool enableBtn(bool enabled);


     int _uid;
     QString _token;
    QMap<ReqId, std::function<void(const QJsonObject &)>> _handlers;
signals:
// ：LoginDialog 喊话"我要注册"，MainWindow 听到后负责换页面。
    void switchRegister();
    void switchReset();
    void sig_connect_tcp(ServerInfo si);//连接tcp信号



private slots:
    void on_log_btn_clicked();

};

#endif // LOGINDIALOG_H
