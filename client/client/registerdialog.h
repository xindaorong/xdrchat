#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <functional>

#include "global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private:
    Ui::RegisterDialog *ui;
    void showTip(const QString &str, bool b_ok);
    void initHttpHandlers();

    // 根据请求 ID 分发不同的注册相关响应
    QMap<ReqId, std::function<void(const QJsonObject &)>> _handlers;

private slots:
    // Qt 自动连接规则：on_<objectName>_<signal>()
    void on_get_code_clicked();
    void slot_reg_mod_finish(ReqId id, QString res, ErrorCode err);
    void on_sure_btn_clicked();
};

#endif // REGISTERDIALOG_H
