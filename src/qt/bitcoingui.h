#ifndef BITCOINGUI_H
#define BITCOINGUI_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QResizeEvent>
#include <QToolBar>

class TransactionTableModel;
class ClientModel;
class WalletModel;
class TransactionView;
class TransactionsPage;
class AskPassphrasePage;
class OverviewPage;
class AddressBookPage;
class SendCoinsDialog;
class SignVerifyMessageDialog;
class Notificator;
class RPCConsole;
class Downloader;
class ImportKeys;
class ExportKeys;
class WebView;
//class GetSolarCoinPage;
class ClaimsPage;
//class ChatPage;
class BlockchainPage;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QTableView;
class QAbstractItemModel;
class QModelIndex;
class QProgressBar;
class QStackedWidget;
class QUrl;
QT_END_NAMESPACE

/**
  Bitcoin GUI main class. This class represents the main window of the Bitcoin UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class BitcoinGUI : public QMainWindow
{
    Q_OBJECT
public:
    explicit BitcoinGUI(QWidget *parent = 0);
    ~BitcoinGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel *clientModel);
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    void setWalletModel(WalletModel *walletModel);

protected:
    void changeEvent(QEvent *e);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void resizeEvent(QResizeEvent *e);
    void closeEvent(QCloseEvent *event);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;

    QMenu *trayIconMenu;
    QToolBar *toolbar;
    QStackedWidget *centralWidget;

    SignVerifyMessageDialog *signVerifyMessageDialog;
    AskPassphrasePage *askPassphrasePage;
    AskPassphrasePage *encryptWalletPage;
    OverviewPage *overviewPage;
    TransactionsPage *transactionsPage;
    TransactionView *transactionView;
    AddressBookPage *addressBookPage;
    AddressBookPage *receiveCoinsPage;
    SendCoinsDialog *sendCoinsPage;
    //GetSolarCoinPage *getSolarCoinPage;
    ClaimsPage *claimsPage;
    //ChatPage *chatPage;
    BlockchainPage *blockchainPage;

    QLabel *labelEncryptionIcon;
    QLabel *labelBalanceIcon;
    QLabel *labelStakingIcon;
    QLabel *labelConnectionsIcon;
    QLabel *labelBlocksIcon;
    QLabel *labelVersionIcon;
    QLabel *versionLabel;
    QLabel *balanceLabel;
    QLabel *stakingLabel;
    QLabel *connectionsLabel;
    QProgressBar *progressBar;

    QMenuBar *appMenuBar;
    QAction *overviewAction;
    QAction *historyAction;
    QAction *quitAction;
    QAction *logoutAction;
    QAction *sendCoinsAction;
    //QAction *getSolarCoinAction;
    QAction *claimsAction;
    //QAction *chatAction;
    QAction *blockchainAction;
    QAction *addressBookAction;
    QAction *signMessageAction;
    QAction *verifyMessageAction;
    QAction *aboutAction;
    QAction *aboutPostAction;
    QAction *aboutQtAction;
    QAction *receiveCoinsAction;
    QAction *optionsAction;
    //QAction *forumAction;
    QAction *webAction;
    QAction *toggleHideAction;
    QAction *exportAction;
    QAction *encryptWalletAction;
    QAction *backupWalletAction;
    QAction *changePassphraseAction;
    QAction *lockWalletAction;
    QAction *unlockWalletAction;
    QAction *openRPCConsoleAction;
    QAction *reloadBlockchainAction;
    QAction *importKeysAction;
    QAction *exportKeysAction;
    QAction *rescanWalletAction;
    QAction *checkForUpdateAction;

    QSystemTrayIcon *trayIcon;
    Notificator *notificator;
    RPCConsole *rpcConsole;

    QMovie *syncIconMovie;

    qint64 currentTotal;

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray (notification) icon */
    void createTrayIcon();

public slots:
    /** Lock/Unlock Wallet features until/when passphrase is entered */
    void lockWalletFeatures(bool lock);
    /** Set balance in status bar */
    void setBalanceLabel(qint64 balance, qint64 stake, qint64 unconfirmed, qint64 immature);
    /** Set version icon good/bad */
    void setVersionIcon(bool newVersion);
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count, int nTotalBlocks);
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    /** Notify the user of an error in the network or transaction handling code. */
    void error(const QString &title, const QString &message, bool modal);
    /** Asks the user whether to pay the transaction fee or to cancel the transaction.
       It is currently not possible to pass a return value to another thread through
       BlockingQueuedConnection, so an indirected pointer is used.
       https://bugreports.qt-project.org/browse/QTBUG-10440

      @param[in] nFeeRequired       the required fee
      @param[out] payFee            true to pay the fee, false to not pay the fee
    */
    void askFee(qint64 nFeeRequired, bool *payFee);
    void handleURI(QString strURI);
    void reloadBlockchainActionEnabled(bool enabled);
    void reloadBlockchain(bool autoReload=false);
    void importKeysActionEnabled(bool enabled);
    void importKeys();
    void exportKeysActionEnabled(bool enabled);
    void exportKeys();
    void checkForUpdateActionEnabled(bool enabled);
    void checkForUpdate();

private slots:
    /** Switch to askpassphrase page */
    void gotoAskPassphrasePage();
    /** Switch to encrypt wallet page */
    void gotoEncryptWalletPage();
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to address book page */
    void gotoAddressBookPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage();
    /** Switch to GetSolarCoin page */
    //void gotoGetSolarCoinPage();
    /** Switch to Claims page */
    void gotoClaimsPage();
    /** Switch to Chat page */
    //void gotoChatPage();
    /** Switch to Blockchain page */
    void gotoBlockchainPage();

    void resizeGUI();
    void logout();
    void exitApp();

    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");

    /** Show configuration dialog */
    void optionsClicked();
    /** Show forums page */
    //void forumClicked();
    /** Show web page */
    void webClicked();
    /** Show about dialog */
    void aboutClicked();
    /** Show about PoST dialog */
    void aboutPostClicked();
#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif
    /** Show incoming transaction notification for new transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void incomingTransaction(const QModelIndex & parent, int start, int end);
    /** Encrypt the wallet */
    void encryptWallet(bool status=true);
    /** Backup the wallet */
    void backupWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to lock wallet temporarily */
    void lockWallet();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();
    /** Rescan the blockchain */
    void rescanWallet();
    /** Check for wallet update from Help menu */
    void menuCheckForUpdate();
    /** Check for wallet update from timer */
    void timerCheckForUpdate();

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    void updateStakingIcon();
};

#endif
