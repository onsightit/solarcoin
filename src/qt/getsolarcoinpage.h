#ifndef GETSOLARCOINPAGE_H
#define GETSOLARCOINPAGE_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class GetSolarCoinPage;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Trade page widget */
class GetSolarCoinPage : public QWidget
{
    Q_OBJECT

public:
    explicit GetSolarCoinPage(QWidget *parent = 0);
    ~GetSolarCoinPage();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

// signals:

private:
    Ui::GetSolarCoinPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:

};

#endif // GETSOLARCOINPAGE_H
