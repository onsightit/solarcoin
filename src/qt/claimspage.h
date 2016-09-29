#ifndef CLAIMSPAGE_H
#define CLAIMSPAGE_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class ClaimsPage;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Trade page widget */
class ClaimsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ClaimsPage(QWidget *parent = 0);
    ~ClaimsPage();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

// signals:

private:
    Ui::ClaimsPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:
    void on_launchButton_clicked();

};

#endif // CLAIMSPAGE_H
