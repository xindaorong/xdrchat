#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

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
signals:
// ：LoginDialog 喊话"我要注册"，MainWindow 听到后负责换页面。
    void switchRegister();

private:
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
