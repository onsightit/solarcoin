#include "forumspage.h"
#include "ui_forumspage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "util.h"
#include "cookiejar.h"
#include "webview.h"

using namespace GUIUtil;

ForumsPage::ForumsPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ForumsPage),
    walletModel(0)
{
    ui->setupUi(this);

    // Setup header and styles
    if (fNoHeaders)
        GUIUtil::header(this, QString(""));
    else if (fSmallHeaders)
        GUIUtil::header(this, QString(":images/headerForumsSmall"));
    else
        GUIUtil::header(this, QString(":images/headerForums"));
    this->layout()->setContentsMargins(0, HEADER_HEIGHT, 0, 0);

    CookieJar *forumsJar = new CookieJar;
    ui->webView->page()->networkAccessManager()->setCookieJar(forumsJar);

    ui->webView->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAsNeeded);
    ui->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    ui->frame->setVisible(true); // Set to true to enable webView navigation buttons
    connect(ui->webView->page()->networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), ui->webView, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError> & )));
    connect(ui->webView->page(), SIGNAL(linkClicked(QUrl)), ui->webView, SLOT(myOpenUrl(QUrl)));

    // buttons
    ui->back->setDisabled(true);
    ui->home->setDisabled(true);
    ui->forward->setDisabled(true);
    ui->webView->sendButtons(ui->back, ui->home, ui->forward);
    connect(ui->back, SIGNAL(clicked()), ui->webView, SLOT(myBack()));
    connect(ui->home, SIGNAL(clicked()), ui->webView, SLOT(myHome()));
    connect(ui->forward, SIGNAL(clicked()), ui->webView, SLOT(myForward()));
    connect(ui->reload, SIGNAL(clicked()), ui->webView, SLOT(myReload()));
}

ForumsPage::~ForumsPage()
{
    delete ui;
}

void ForumsPage::setModel(WalletModel *model)
{
    this->walletModel = model;

    QUrl url(QString(walletUrl).append("wallet/forums.php?v=").append(FormatVersion(CLIENT_VERSION).c_str()));
    ui->webView->myOpenUrl(url);
}
