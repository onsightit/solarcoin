#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>

namespace Ui {
    class UpdateDialog;
}
class ClientModel;

/** "Update" dialog box */
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    bool updateAccepted;
    explicit UpdateDialog(QWidget *parent = 0);
    ~UpdateDialog();

    void setModel(ClientModel *model);
private:
    Ui::UpdateDialog *ui;

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
};

#endif // UPDATEDIALOG_H
