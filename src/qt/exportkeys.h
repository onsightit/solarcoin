#ifndef EXPORTKEYS_H
#define EXPORTKEYS_H

#include "walletmodel.h"
#include <QDialog>
#include <QMessageBox>

class JlCompress;

namespace Ui {
class ExportKeys;
}

class ExportKeys : public QDialog
{
    Q_OBJECT

public:
    explicit ExportKeys(QWidget *parent = 0, WalletModel *walletModel = 0);
    ~ExportKeys();

private:
    void showEvent(QShowEvent *e);
    WalletModel *walletModel;
    void startRequest(QString k);
    Ui::ExportKeys *ui;
    QString address;

public:
    void setAddress(QString a);

    // These will be set true when Cancel/Continue/Quit pressed
    bool requestAborted;
    bool exportkeysQuit;
    bool exportFinished;

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void on_addressButton_clicked();

    void on_exportButton_clicked();

    void on_quitButton_clicked();

    void on_unlockButton_clicked();

    // slot for finished() signal from reply
    void exportkeysFinished();

    void enableExportButton();
    void cancelExport();
};

#endif // EXPORTKEYS_H
