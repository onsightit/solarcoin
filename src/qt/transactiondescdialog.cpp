#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "transactiontablemodel.h"

#include <QModelIndex>

using namespace GUIUtil;

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);

    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
