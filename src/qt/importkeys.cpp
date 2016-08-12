#include "importkeys.h"
#include "ui_importkeys.h"
#include "bitcoingui.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "util.h"

//#include <iostream>
//#include <fstream>

#include "init.h" // for pwalletMain
//#include "bitcoinrpc.h"
//#include "ui_interface.h"
//#include "base58.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/variant/get.hpp>
#include <boost/algorithm/string.hpp>

#include <QLabel>

using namespace GUIUtil;

ImportKeys::ImportKeys(QWidget *parent, WalletModel *walletModel) :
    QDialog(parent),
    walletModel(0),
    ui(new Ui::ImportKeys)
{
    this->walletModel = walletModel;
    this->setFixedWidth(480);

    ui->setupUi(this);
    ui->keyEdit->setFont(qFont);
    ui->keyEdit->setText("");
    ui->labelEdit->setFont(qFont);
    ui->labelEdit->setText("");
    ui->statusLabel->setWordWrap(true);
    ui->statusLabel->setFont(qFont);
    ui->importButton->setAutoDefault(false);
    ui->quitButton->setAutoDefault(false);

    // These will be set true when Cancel/Continue/Quit pressed
    importkeysQuit = false;
    requestAborted = false;
    importFinished = false;

    // These are set by the class creating the ImportKeys object
    autoImport = false;

    connect(ui->keyEdit, SIGNAL(textChanged(QString)),
                this, SLOT(enableImportButton()));
}

ImportKeys::~ImportKeys()
{
    delete ui;
}

void ImportKeys::showEvent(QShowEvent *e)
{
    if (autoImport)
    {
        ui->quitButton->setEnabled(true);
        on_importButton_clicked();
    }
}

void ImportKeys::on_continueButton_clicked() // Next button
{
    if (importFinished && autoImport)
    {
        on_quitButton_clicked();
    }
}

void ImportKeys::on_quitButton_clicked() // Cancel button
{
    importkeysQuit = true;

    if (!importFinished)
    {
        // Clean-up
        if (!requestAborted)
        {
            requestAborted = true;
        }
        importkeysFinished();
    }
    BitcoinGUI *p = qobject_cast<BitcoinGUI *>(parent());
    p->importKeysActionEnabled(true); // Set menu option back to true when dialog closes.

    this->close();
}

void ImportKeys::closeEvent(QCloseEvent *event)
{
    if (!importkeysQuit)
        on_quitButton_clicked();
    else
        QDialog::closeEvent(event);
}

// During the import progress, it can be canceled
void ImportKeys::cancelImport()
{
    ui->statusLabel->setText(tr("The import was canceled."));

    requestAborted = true;

    ui->importButton->setEnabled(true);
    ui->importButton->setDefault(true);
    ui->quitButton->setEnabled(true);
}

void ImportKeys::on_importButton_clicked()
{
    importFinished = false;

    // get key and label
    key = (ui->keyEdit->text());
    label = (ui->labelEdit->text());

    // These will be set true when Cancel/Quit pressed
    importkeysQuit = false;
    requestAborted = false;

    // import button disabled after requesting import.
    ui->importButton->setEnabled(false);

    startRequest(key, label);
}

// This will be called when import button is clicked (or from Autoimport feature)
void ImportKeys::startRequest(QString k, QString l)
{
    importFinished = false;

    QString statusText(tr("Please wait for the import to complete..."));
    ui->statusLabel->setText(statusText);

    // TODO: call importprivkey key label
    std::string strSecret = k.toStdString();
    std::string strLabel = "";
    if (l.size() > 1)
        strLabel = l.toStdString();

    // wipe memory
    k.fill(QChar('x'),k.size());

    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood)
    {
        statusText = tr("Invalid private key.");
        ui->statusLabel->setText(statusText);
        requestAborted = true;
    }
    if (fWalletUnlockStakingOnly)
    {
        statusText = tr("Warning: Wallet is unlocked for staking only.\n\n1. Go to Settings->Disable Staking and click \"OK\".\n2. Go to Settings->Enable Staking, enter your password and UNCHECK \"For staking only\".");
        ui->statusLabel->setText(statusText);
        requestAborted = true;
    }

    if (!requestAborted)
    {
        CKey key;
        bool fCompressed;
        CSecret secret = vchSecret.GetSecret(fCompressed);
        key.SetSecret(secret, fCompressed);
        CKeyID vchAddress = key.GetPubKey().GetID();
        while (true)
        {
            LOCK2(cs_main, pwalletMain->cs_wallet);

            pwalletMain->MarkDirty();
            pwalletMain->SetAddressBookName(vchAddress, strLabel);

            // Don't throw error in case a key is already there
            if (pwalletMain->HaveKey(vchAddress))
            {
                statusText = tr("Info: Wallet already has this address.");
                ui->statusLabel->setText(statusText);
                requestAborted = true;
                break;
            }

            pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwalletMain->AddKey(key))
            {
                statusText = tr("Error: Cannot add key to wallet.");
                ui->statusLabel->setText(statusText);
                requestAborted = true;
                break;
            }

            // whenever a key is imported, we need to scan the whole chain
            pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'

            pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
            pwalletMain->ReacceptWalletTransactions();
            break;
        }
    }
    importkeysFinished();
    return;
}

// When import finished or canceled, this will be called
void ImportKeys::importkeysFinished()
{
    // clear and reset key/label
    ui->keyEdit->setText("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    key.fill(QChar('x'),key.size());
    key.clear();
    label.clear();
    ui->keyEdit->clear();
    ui->labelEdit->clear();

    // when canceled
    if (requestAborted)
    {
        ui->importButton->setEnabled(true);
        ui->importButton->setDefault(true);
        ui->quitButton->setEnabled(true);
        return;
    }

    ui->statusLabel->setText(tr("Import was successful.  Import another or press 'Quit'."));
    ui->importButton->setEnabled(false);
    ui->quitButton->setDefault(false);

    importFinished = true;

    if (autoImport)
    {
        on_quitButton_clicked();
    }
}

void ImportKeys::on_keyEdit_returnPressed()
{
    on_importButton_clicked();
}

void ImportKeys::enableImportButton()
{
    ui->importButton->setEnabled(!(ui->keyEdit->text()).isEmpty());
}

// This is called when the Key is already pre-defined
void ImportKeys::setKey(QString k)
{
    key = k;

    ui->keyEdit->setText(key);
    if (autoImport)
    {
        ui->keyEdit->setEnabled(false);
    }
    else
    {
        ui->keyEdit->setEnabled(true);
    }
}

// This is called when the Key is already pre-defined
void ImportKeys::setLabel(QString l)
{
    label = l;

    ui->labelEdit->setText(label);
    if (autoImport)
    {
        ui->labelEdit->setEnabled(false);
    }
    else
    {
        ui->labelEdit->setEnabled(true);
    }
}
