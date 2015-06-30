#ifndef RESCANDIALOG_H
#define RESCANDIALOG_H

#include <QDialog>

namespace Ui {
    class RescanDialog;
}
class ClientModel;

/** "Rescan" dialog box */
class RescanDialog : public QDialog
{
    Q_OBJECT

public:
    bool rescanAccepted;
    explicit RescanDialog(QWidget *parent = 0);
    ~RescanDialog();

    void setModel(ClientModel *model);
private:
    Ui::RescanDialog *ui;

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
};

#endif // RESCANDIALOG_H
