#ifndef WHATSNEWDIALOG_H
#define WHATSNEWDIALOG_H

#include <QDialog>

namespace Ui {
    class WhatsNewDialog;
}
class ClientModel;

/** "Whatsnew" dialog box */
class WhatsNewDialog : public QDialog
{
    Q_OBJECT

public:
    bool whatsNewAccepted;
    explicit WhatsNewDialog(QWidget *parent = 0);
    ~WhatsNewDialog();

    void setModel(ClientModel *model);
private:
    Ui::WhatsNewDialog *ui;

private slots:
    void on_buttonBox_accepted();
};

#endif // WHATSNEWDIALOG_H
