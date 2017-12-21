#ifndef BITCOIN_QT_ASKPASSPHRASEPAGE_H
#define BITCOIN_QT_ASKPASSPHRASEPAGE_H

#include <QDialog>

namespace Ui {
    class AskPassphrasePage;
}

class WalletModel;

/** Ask for passphrase. Used for unlocking at startup.
 */
class AskPassphrasePage : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Encrypt,    /**< Ask passphrase twice and encrypt */
        Lock,       /**< Ask passphrase and lock */
        Unlock      /**< Ask passphrase and unlock */
    };

    explicit AskPassphrasePage(Mode mode, QWidget *parent = 0);
    ~AskPassphrasePage();

    void setModel(WalletModel *model);

private:
    Ui::AskPassphrasePage *ui;
    Mode mode;
    WalletModel *model;
    bool fCapsLock;

private Q_SLOTS:
    void accept();
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);

Q_SIGNALS:
    void lockWalletFeatures(bool lock);
};

#endif // BITCOIN_QT_ASKPASSPHRASEPAGE_H
