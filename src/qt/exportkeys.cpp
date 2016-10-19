#include "exportkeys.h"
#include "ui_exportkeys.h"
#include "bitcoingui.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "util.h"
#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "init.h" // for pwalletMain

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/variant/get.hpp>
#include <boost/algorithm/string.hpp>

#include <QLabel>

using namespace GUIUtil;

ExportKeys::ExportKeys(QWidget *parent, WalletModel *walletModel) :
    QDialog(parent),
    walletModel(0),
    ui(new Ui::ExportKeys)
{
    this->walletModel = walletModel;
    this->setFixedWidth(480);

    ui->setupUi(this);
    ui->addressButton->setAutoDefault(true);
    ui->addressButton->setEnabled(false);
    ui->addressLabel->setFont(qFont);
    ui->addressLabel->setText("");
    ui->statusLabel->setWordWrap(true);
    ui->statusLabel->setFont(qFont);
    ui->exportButton->setEnabled(false);
    ui->exportButton->setAutoDefault(false);
    ui->unlockButton->setEnabled(true);
    ui->unlockButton->setDefault(true);
    ui->quitButton->setAutoDefault(false);

    ui->statusLabel->setText(tr("Select a receiving address from the Address Book. Your wallet must be unlocked and not staking.\n"));

    // These will be set true when Cancel/Continue/Quit pressed
    exportkeysQuit = false;
    requestAborted = false;
    exportFinished = false;

    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked && !fWalletUnlockStakingOnly)
    {
        ui->addressButton->setEnabled(true);
        ui->unlockButton->setEnabled(false);
        ui->unlockButton->setDefault(false);
    }

    connect(ui->addressLabel, SIGNAL(textChanged(QString)),
                this, SLOT(enableExportButton()));
}

ExportKeys::~ExportKeys()
{
    delete ui;
}

void ExportKeys::showEvent(QShowEvent *e)
{
}

void ExportKeys::on_unlockButton_clicked()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
    dlg.setModel(walletModel);
    dlg.exec();

    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked && !fWalletUnlockStakingOnly)
    {
        ui->addressButton->setEnabled(true);
        ui->unlockButton->setEnabled(false);
        ui->unlockButton->setDefault(false);
    }
}

void ExportKeys::on_quitButton_clicked() // Cancel button
{
    exportkeysQuit = true;

    if (!exportFinished)
    {
        // Clean-up
        if (!requestAborted)
        {
            requestAborted = true;
        }
        exportkeysFinished();
    }
    BitcoinGUI *p = qobject_cast<BitcoinGUI *>(parent());
    p->exportKeysActionEnabled(true); // Set menu option back to true when dialog closes.

    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked && !fWalletUnlockStakingOnly)
        fWalletUnlockStakingOnly = true;

    this->close();
}

void ExportKeys::closeEvent(QCloseEvent *event)
{
    if (!exportkeysQuit)
        on_quitButton_clicked();
    else
        QDialog::closeEvent(event);
}

// During the export progress, it can be canceled
void ExportKeys::cancelExport()
{
    ui->statusLabel->setText(tr("The export was canceled."));

    requestAborted = true;

    ui->exportButton->setEnabled(true);
    ui->exportButton->setDefault(true);
}

void ExportKeys::on_addressButton_clicked()
{
    exportFinished = false;

    // get address
    AddressBookPage dlg(AddressBookPage::ForSigning, AddressBookPage::ReceivingTab, this);
    dlg.setModel(this->walletModel->getAddressTableModel());
    if (dlg.exec())
    {
        address = dlg.getReturnValue();
    }
    ui->addressLabel->setText(address);

    enableExportButton();
}

void ExportKeys::on_exportButton_clicked()
{
    exportFinished = false;

    // get address
    address = (ui->addressLabel->text());

    // These will be set true when Cancel/Quit pressed
    exportkeysQuit = false;
    requestAborted = false;

    startRequest(address);
}

// This will be called when export button is clicked
void ExportKeys::startRequest(QString a)
{
    exportFinished = false;

    QString statusText(tr("Please wait for the export to complete..."));
    ui->statusLabel->setText(statusText);

    if (fWalletUnlockStakingOnly)
    {
        statusText = tr("Please unlock the wallet to continue. Enter your password and UNCHECK \"For staking only\".");
        ui->statusLabel->setText(statusText);
        ui->exportButton->setEnabled(false);
        ui->addressButton->setEnabled(false);
        ui->unlockButton->setEnabled(true);
        ui->unlockButton->setDefault(true);
        requestAborted = true;
    }

    std::string strAddress = a.toStdString().c_str();
    CBitcoinAddress address;
    if (!address.SetString(strAddress))
    {
        statusText = tr("Invalid Healthcoin address: ").append(strAddress.c_str());
        ui->statusLabel->setText(statusText);
        requestAborted = true;
    }

    CKeyID keyID;
    if (!address.GetKeyID(keyID))
    {
        statusText = tr("Address does not refer to a key in wallet.");
        ui->statusLabel->setText(statusText);
        requestAborted = true;
    }

    CSecret vchSecret;
    bool fCompressed;
    if (!requestAborted)
    {
        if (!pwalletMain->GetSecret(keyID, vchSecret, fCompressed))
        {
            statusText = tr("Private key for address ").append(strAddress.c_str()).append(tr(" is unknown."));
            ui->statusLabel->setText(statusText);
            requestAborted = true;
        }
    }
    if (!requestAborted)
    {
        statusText = tr("Export was successful.  Export another or press 'Cancel'.\n\n").append(CBitcoinSecret(vchSecret, fCompressed).ToString().c_str());
        ui->statusLabel->setText(statusText);
    }
    exportkeysFinished();
    return;
}

// When export finished or canceled, this will be called
void ExportKeys::exportkeysFinished()
{
    // when canceled
    if (requestAborted)
    {
        if (ui->addressLabel->text().isEmpty())
            ui->exportButton->setEnabled(false);
        ui->quitButton->setDefault(true);
        return;
    }

    ui->exportButton->setEnabled(false);
    ui->addressButton->setDefault(true);

    exportFinished = true;
}

void ExportKeys::enableExportButton()
{
    ui->exportButton->setEnabled(!(ui->addressLabel->text()).isEmpty());
}

// This is called when the Address is already pre-defined
void ExportKeys::setAddress(QString a)
{
    address = a;

    ui->addressLabel->setText(address);
}

