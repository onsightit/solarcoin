#ifndef TRANSACTIONSPAGE_H
#define TRANSACTIONSPAGE_H

#include <QWidget>
#include <QVBoxLayout>

class TransactionTableModel;
class ClientModel;
class WalletModel;
class TransactionView;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class TransactionsPage;
}

/** Transactions ("history") page widget */
class TransactionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionsPage(QWidget *parent = 0);
    ~TransactionsPage();

private:
    Ui::TransactionsPage *ui;

};

#endif // TRANSACTIONSPAGE_H
