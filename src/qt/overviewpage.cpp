#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "util.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "cookiejar.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "bitcoingui.h"
#include "webview.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QDesktopServices>
#include <QUrl>
#include <QWebView>
#include <QGraphicsView>

using namespace GUIUtil;

#define DECORATION_SIZE 46
#define NUM_ITEMS 9

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::SLR)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = COLOR_BAREADDRESS;
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if (amount > 0)
        {
            foreground = COLOR_POSITIVE;
        }
        else
        {
            foreground = COLOR_BAREADDRESS;
        }
        painter->setPen(foreground);
        QString amountText =  BitcoinUnits::formatMaxDecimals(unit, amount, decimals, true, hideAmounts);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    int decimals;
    bool hideAmounts;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    // Setup header and styles
    if (fNoHeaders)
        GUIUtil::header(this, QString(""));
    else if (fSmallHeaders)
        GUIUtil::header(this, QString(":images/headerOverviewSmall"));
    else
        GUIUtil::header(this, QString(":images/headerOverview"));

    ui->setupUi(this);
    this->layout()->setContentsMargins(0, 0 + HEADER_HEIGHT, 0, 0);

    // try to make more room for small screens
    if (fNoHeaders) {
        ui->gridLayout_3->layout()->setContentsMargins(0, 10, 10, 0);
    }

    ui->labelBalance->setFont(qFontLargerBold);
    ui->labelTransactions->setFont(qFontLargerBold);

    ui->labelSpendableText->setFont(qFont);
    ui->labelSpendable->setFont(qFont);
    ui->labelStakeText->setFont(qFont);
    ui->labelStake->setFont(qFont);
    ui->labelUnconfirmedText->setFont(qFont);
    ui->labelUnconfirmed->setFont(qFont);
    ui->labelTotalText->setFont(qFont);
    ui->labelTotal->setFont(qFont);

    // Add icons to the Balance section
    ui->labelSpendableText->setText("<html><img src=':icons/spendable' width=16 height=16 border=0 align='bottom'> Spendable:</html>");
    ui->labelStakeText->setText("<html><img src=':icons/staking' width=16 height=16 border=0 align='bottom'> Staking:</html>");
    ui->labelUnconfirmedText->setText("<html><img src=':icons/unconfirmed' width=16 height=16 border=0 align='bottom'> Unconfirmed:</html>");
    ui->labelTotalText->setText("<html><img src=':icons/total' width=16 height=16 border=0 align='bottom'> Total:</html>");

    // Recent transactionsBalances
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 1));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listTransactions->setMouseTracking(true);
    ui->listTransactions->viewport()->setAttribute(Qt::WA_Hover, true);
    ui->listTransactions->setStyleSheet("QListView { background: white; color: " + STR_FONT_COLOR + "; border-radius: 10px; border: 0; padding-right: 10px; padding-bottom: 5px; } \
                                         QListView::hover { background: qlineargradient(x1: 0, y1: 1, x2: 0, y2: 0, stop: 0 #fafbfe, stop: 1 #ECF3FA); }");
    ui->listTransactions->setFont(qFont);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of date") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of date") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    QString maxDecimalsTooltipText("\nUse Settings/Options/Display to hide decimals.");
    qint64 total = balance + stake + unconfirmedBalance + immatureBalance;

    BitcoinUnits *bcu = new BitcoinUnits(this, this->model);
    int unit = model->getOptionsModel()->getDisplayUnit();
    bool hideAmounts = model->getOptionsModel()->getHideAmounts();
    if (model->getOptionsModel()->getDecimalPoints() < bcu->maxdecimals(unit))
    {
        maxDecimalsTooltipText = QString(""); // The user already knows about the option to set decimals
    }

    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;

    ui->labelSpendable->setText(bcu->formatWithUnit(unit, balance, false, hideAmounts));
    ui->labelSpendable->setToolTip(tr("%1%2").arg(bcu->formatWithUnitWithMaxDecimals(unit, balance, bcu->maxdecimals(unit), true, false)).arg(maxDecimalsTooltipText));
    ui->labelStake->setText(bcu->formatWithUnit(unit, stake, false, hideAmounts));
    ui->labelStake->setToolTip(tr("%1%2").arg(bcu->formatWithUnitWithMaxDecimals(unit, stake, bcu->maxdecimals(unit), true, false)).arg(maxDecimalsTooltipText));
    ui->labelUnconfirmed->setText(bcu->formatWithUnit(unit, unconfirmedBalance, false, hideAmounts));
    ui->labelUnconfirmed->setToolTip(tr("%1%2").arg(bcu->formatWithUnitWithMaxDecimals(unit, unconfirmedBalance, bcu->maxdecimals(unit), true, false)).arg(maxDecimalsTooltipText));
    //ui->labelImmature->setText(bcu->formatWithUnit(unit, immatureBalance, false, hideAmounts));
    //ui->labelImmature->setToolTip(tr("%1%2").arg(bcu->formatWithUnitWithMaxDecimals(unit, immatureBalance, bcu->maxdecimals(unit), true, false)).arg(maxDecimalsTooltipText));
    //ui->labelImmature->setVisible(true);
    //ui->labelImmatureText->setVisible(true);
    ui->labelTotal->setText(bcu->formatWithUnit(unit, total, false, hideAmounts));
    ui->labelTotal->setToolTip(tr("%1%2").arg(bcu->formatWithUnitWithMaxDecimals(unit, total, bcu->maxdecimals(unit), true, false)).arg(maxDecimalsTooltipText));

    delete bcu;
}

void OverviewPage::setNumTransactions(int count)
{
    //ui->labelNumTransactions->setText(QLocale::system().toString(count));
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(model->getOptionsModel(), SIGNAL(decimalPointsChanged(int)), this, SLOT(updateDecimalPoints()));
        connect(model->getOptionsModel(), SIGNAL(hideAmountsChanged(bool)), this, SLOT(updateHideAmounts()));
    }

    // update the display unit, to not use the default ("SLR")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();
        txdelegate->decimals = model->getOptionsModel()->getDecimalPoints();
        txdelegate->hideAmounts = model->getOptionsModel()->getHideAmounts();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateDecimalPoints()
{
    updateDisplayUnit();
}

void OverviewPage::updateHideAmounts()
{
    updateDisplayUnit();
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::myOpenUrl(QUrl url)
{
    QDesktopServices::openUrl(url);
}

void OverviewPage::sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist)
{
    qnr->ignoreSslErrors();
}
