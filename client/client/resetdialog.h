#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class ResetDialog;
}

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

private slots:

    void on_verify_btn_clicked_clicked();
    void slot_reset_mod_finish(ReqId id, QString res, ErrorCode err);
    void on_sur_btn_clicked();
    void on_return_btn_clicked();


private:
    Ui::ResetDialog *ui;
    QMap<TipErr, QString> _tip_errs;
    void initHandlers();
    bool checkUserValid();
    bool checkPassValid();
    bool checkEmailValid();
    bool checkVarifyValid();
    void AddTipErr(TipErr te, QString tips);
    void DelTipErr(TipErr te);
    void showTip(QString str,bool b_ok);


    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
signals:
    void switchLogin();
};

#endif // RESETDIALOG_H
