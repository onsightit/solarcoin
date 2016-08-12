#ifndef IMPORTKEYS_H
#define IMPORTKEYS_H

#include "walletmodel.h"
#include <QDialog>
#include <QMessageBox>

class JlCompress;

namespace Ui {
class ImportKeys;
}

class ImportKeys : public QDialog
{
    Q_OBJECT

public:
    explicit ImportKeys(QWidget *parent = 0, WalletModel *walletModel = 0);
    ~ImportKeys();

private:
    void showEvent(QShowEvent *e);
    WalletModel *walletModel;
    void startRequest(QString k, QString l);
    Ui::ImportKeys *ui;
    QString key;
    QString label;

public:
    void setKey(QString k);
    void setLabel(QString l);

    // These will be set true when Cancel/Continue/Quit pressed
    bool requestAborted;
    bool importkeysQuit;
    bool importFinished;

    // These are set by the class creating the ImportKeys object
    bool autoImport;

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void on_importButton_clicked();

    void on_quitButton_clicked();

    void on_continueButton_clicked();

    void on_keyEdit_returnPressed();

    // slot for finished() signal from reply
    void importkeysFinished();

    void enableImportButton();
    void cancelImport();
};

#endif // IMPORTKEYS_H
