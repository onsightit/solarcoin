#include "postdialog.h"
#include "ui_postdialog.h"
#include "clientmodel.h"
#include "util.h"
#include "guiutil.h"
#include "guiconstants.h"

using namespace GUIUtil;

bool postAccepted = false;

PostDialog::PostDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PostDialog)
{
    ui->setupUi(this);
    ui->description->setFont(veriFont);
}

void PostDialog::setModel(ClientModel *model)
{
}

PostDialog::~PostDialog()
{
    delete ui;
}

void PostDialog::on_buttonBox_accepted()
{
    postAccepted = true;
    close();
}
