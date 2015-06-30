#ifndef POSTDIALOG_H
#define POSTDIALOG_H

#include <QDialog>

namespace Ui {
    class PostDialog;
}
class ClientModel;

/** "Post" dialog box */
class PostDialog : public QDialog
{
    Q_OBJECT

public:
    bool postAccepted;
    explicit PostDialog(QWidget *parent = 0);
    ~PostDialog();

    void setModel(ClientModel *model);
private:
    Ui::PostDialog *ui;

private slots:
    void on_buttonBox_accepted();
};

#endif // POSTDIALOG_H
