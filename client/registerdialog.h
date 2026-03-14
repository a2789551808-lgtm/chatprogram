#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
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

private slots:
    void on_get_code_clicked();

    void on_confirm_btn_clicked();

    void on_pushButton_clicked();

    void on_cancel_btn_clicked();

public slots:
    void slot_reg_mod_finish(ReqId id, QString res, ErrorCodes err);


private:
    void showTip(QString str, bool b_ok);
    Ui::RegisterDialog *ui;
    void initHttpHandlers();
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    QMap<TipErr, QString> _tip_errs;
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkVarifyValid();
    bool checkConfirmValid();

    void ChangeTipPage();
    QTimer * _countdown_timer;
    int _countdown;

signals:
    void sigSwitchLogin();
};

#endif // REGISTERDIALOG_H
