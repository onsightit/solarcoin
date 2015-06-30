#ifndef BLOCKCHAINPAGE_H
#define BLOCKCHAINPAGE_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class BlockchainPage;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Trade page widget */
class BlockchainPage : public QWidget
{
    Q_OBJECT

public:
    explicit BlockchainPage(QWidget *parent = 0);
    ~BlockchainPage();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

// signals:

private:
    Ui::BlockchainPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:

};

#endif // BLOCKCHAINPAGE_H
