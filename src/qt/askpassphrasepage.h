#ifndef ASKPASSPHRASEPAGE_H
#define ASKPASSPHRASEPAGE_H

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

private slots:
    void accept();
    void textChanged();
    bool event(QEvent *event);
    bool eventFilter(QObject *, QEvent *event);

signals:
    void lockWalletFeatures(bool lock);
};

#endif // ASKPASSPHRASEPAGE_H
