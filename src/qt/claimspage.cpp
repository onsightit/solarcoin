#include "claimspage.h"
#include "ui_claimspage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "util.h"
#include "cookiejar.h"
#include "webview.h"

using namespace GUIUtil;

ClaimsPage::ClaimsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClaimsPage),
    walletModel(0)
{
    ui->setupUi(this);

    // Setup header and styles
    if (fNoHeaders)
        GUIUtil::header(this, QString(""));
    else if (fSmallHeaders)
        GUIUtil::header(this, QString(":images/headerClaimsSmall"));
    else
        GUIUtil::header(this, QString(":images/headerClaims"));
    this->layout()->setContentsMargins(0, HEADER_HEIGHT, 0, 0);
}

ClaimsPage::~ClaimsPage()
{
    delete ui;
}

void ClaimsPage::setModel(WalletModel *model)
{
    this->walletModel = model;
}

void ClaimsPage::on_launchButton_clicked() // Launch Claims Form button
{
    QDesktopServices::openUrl(QUrl(claimsUrl, QUrl::TolerantMode));
}
